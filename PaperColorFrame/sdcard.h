// sdcard.h - the microSD card: is one inserted, can it be mounted, and a small self-test.
//
// THE SHARED BUS. The card sits on SPI2 (MOSI 13, MISO 14, SCLK 15, chip select 47) together with
// the e-paper panel (chip select 44). Facts checked in the M5GFX / arduino-esp32 sources:
//   * M5GFX starts that bus through Arduino's global SPI object (SPI.begin(sclk, miso, mosi)) and
//     takes the bus for each of its own transactions (`bus_shared`).
//   * SD.begin() calls SPI.begin() WITHOUT arguments, which does nothing while the bus runs, and
//     SD.end() only unmounts the card: it never calls SPI.end().
// So the standard Arduino SD library can use the same SPI object, provided this code
//   - never calls SPI.begin(<pins>) or SPI.end() itself, and
//   - never touches the card while the panel is refreshing. Screen refreshes block the loop, so
//     that holds by construction: card access only happens between refreshes.
//
// PRESENCE. Whether a card is inserted is read from the power chip (PM1 GPIO1, LOW = inserted),
// as the factory firmware does, so no slow mount attempt is made when the slot is empty.
//
// A card that is inserted but can't be mounted is reported as an ERROR, and is tried again only after
// RETRY_MS or when it is taken out and put back. When a mount fails, diagnose() talks to the card
// directly in SPI mode (the same first commands the SD library sends), reads sector 0 and says what it
// found: no answer on the bus, GPT partition table, exFAT / NTFS, blank, or a FAT volume that the library
// still refused. That text is the card's "error" (serial log, web page, SD test).
#pragma once
#include <SD.h>
#include <SPI.h>
#include <M5Unified.h>
#include <M5PM1.h>
#include "sensors.h"
#include "types.h"

namespace sdcard {

constexpr uint32_t SPI_HZ   = 4000000;              // as slow as the panel's own clock: safe on the shared lines
constexpr uint32_t RETRY_MS = 10UL * 60UL * 1000UL; // an unreadable card is tried again this often
constexpr uint8_t  PM1_DET_EN = (uint8_t)M5PM1_GPIO_NUM_4;   // driven HIGH to enable the card-detect switch
constexpr uint8_t  PM1_DET    = (uint8_t)M5PM1_GPIO_NUM_1;   // reads LOW while a card is inserted

struct Info {
  SdState  state     = SD_ABSENT;
  bool     detected  = false;            // the card-detect switch says a card is inserted
  uint64_t cardBytes = 0;                // raw card size (cheap to ask; used/free space is not)
  char     type[8]   = "";               // "SDSC", "SDHC", "MMC" or ""
  char     error[96] = "";               // why an inserted card can't be used (from diagnose())
  uint32_t hz        = 0;                // SPI speed the card was mounted at
};

inline Info&     info()        { static Info i;        return i; }
inline bool&     mountedFlag() { static bool m = false; return m; }
inline uint32_t& lastTryMs()   { static uint32_t t = 0; return t; }
// Counts every mount and unmount: whatever was read from a card (the picture list, ...) is out of date when this changes.
inline uint32_t& mountGeneration() { static uint32_t g = 0; return g; }

// Sets up the two PM1 pins used for card detection (call once, after the PM1 is up).
inline void begin() {
  sensors::pm1().pinMode(PM1_DET_EN, OUTPUT);
  sensors::pm1().digitalWrite(PM1_DET_EN, HIGH);
  sensors::pm1().pinMode(PM1_DET, INPUT_PULLUP);
}

// -1 = the read failed, 0 = no card, 1 = card inserted
inline int readDetect() {
  int v = sensors::pm1().digitalRead(PM1_DET);
  if (v < 0) return -1;
  return v == LOW ? 1 : 0;
}

inline void setType(Info& i, sdcard_type_t t) {
  switch (t) {
    case CARD_MMC:  snprintf(i.type, sizeof(i.type), "MMC");  break;
    case CARD_SD:   snprintf(i.type, sizeof(i.type), "SDSC"); break;
    case CARD_SDHC: snprintf(i.type, sizeof(i.type), "SDHC"); break;
    default:        i.type[0] = 0;                            break;
  }
}

// ============================================================================================
//  diagnose(): why can't this card be mounted?
//  Plain SD-card protocol in SPI mode at 400 kHz, bounded in time (about 1.5 s at the very most).
//  It only uses SPI.beginTransaction / transfer / endTransaction and the card's own chip select, exactly
//  like the SD library: the bus itself is never started or ended here.
// ============================================================================================
constexpr uint32_t DIAG_HZ = 400000;

enum Layout : uint8_t {
  L_BLANK, L_NO_SIGNATURE, L_EXFAT, L_NTFS, L_FAT_VBR, L_MBR_EMPTY, L_GPT, L_MBR_EXFAT_NTFS, L_MBR_FAT, L_MBR_OTHER
};

struct SectorInfo {
  Layout  layout = L_BLANK;
  uint8_t partType = 0;            // MBR partition type, or 0
  char    fat[8] = "";             // "FAT32" / "FAT16" / "FAT12" when known
};

// Looks at sector 0 (512 bytes) of a card. Pure function, no hardware.
inline SectorInfo analyseSector0(const uint8_t* s) {
  SectorInfo r;
  bool allZero = true, allFF = true;
  for (int i = 0; i < 512; i++) { if (s[i] != 0x00) allZero = false; if (s[i] != 0xFF) allFF = false; }
  if (allZero || allFF) { r.layout = L_BLANK; return r; }
  if (s[510] != 0x55 || s[511] != 0xAA) { r.layout = L_NO_SIGNATURE; return r; }

  // A boot sector with no partition table in front of it ("super-floppy") names its file system.
  if (memcmp(s + 3, "EXFAT   ", 8) == 0) { r.layout = L_EXFAT; return r; }
  if (memcmp(s + 3, "NTFS    ", 8) == 0) { r.layout = L_NTFS;  return r; }
  if (memcmp(s + 82, "FAT32   ", 8) == 0) { r.layout = L_FAT_VBR; snprintf(r.fat, sizeof(r.fat), "FAT32"); return r; }
  if (memcmp(s + 54, "FAT16   ", 8) == 0) { r.layout = L_FAT_VBR; snprintf(r.fat, sizeof(r.fat), "FAT16"); return r; }
  if (memcmp(s + 54, "FAT12   ", 8) == 0) { r.layout = L_FAT_VBR; snprintf(r.fat, sizeof(r.fat), "FAT12"); return r; }

  // Otherwise it is a master boot record: look at its four partition entries.
  for (int e = 0; e < 4; e++) {
    uint8_t type = s[446 + 16 * e + 4];
    if (type == 0x00) continue;
    r.partType = type;
    switch (type) {
      case 0x0B: case 0x0C: r.layout = L_MBR_FAT; snprintf(r.fat, sizeof(r.fat), "FAT32"); return r;
      case 0x04: case 0x06: case 0x0E: r.layout = L_MBR_FAT; snprintf(r.fat, sizeof(r.fat), "FAT16"); return r;
      case 0x01: r.layout = L_MBR_FAT; snprintf(r.fat, sizeof(r.fat), "FAT12"); return r;
      case 0xEE: r.layout = L_GPT; return r;
      case 0x07: r.layout = L_MBR_EXFAT_NTFS; return r;
      default:   r.layout = L_MBR_OTHER; return r;
    }
  }
  r.layout = L_MBR_EMPTY;
  return r;
}

// One sentence for the user.
inline void describeSector0(const SectorInfo& r, char* out, size_t n) {
  switch (r.layout) {
    case L_BLANK:         snprintf(out, n, "Card is blank: format it as FAT32"); break;
    case L_NO_SIGNATURE:  snprintf(out, n, "Card has no partition table: format it as FAT32"); break;
    case L_EXFAT:         snprintf(out, n, "Card is exFAT, which isn't supported: format it as FAT32"); break;
    case L_NTFS:          snprintf(out, n, "Card is NTFS, which isn't supported: format it as FAT32"); break;
    case L_FAT_VBR:       snprintf(out, n, "%s volume found, but mounting failed", r.fat); break;
    case L_MBR_EMPTY:     snprintf(out, n, "Partition table is empty: format the card as FAT32"); break;
    case L_GPT:           snprintf(out, n, "GPT partition table (macOS 'GUID' scheme): reformat with 'Master Boot Record'"); break;
    case L_MBR_EXFAT_NTFS: snprintf(out, n, "Partition type 07 (exFAT or NTFS) isn't supported: format as FAT32"); break;
    case L_MBR_FAT:       snprintf(out, n, "%s partition found, but mounting failed", r.fat); break;
    default:              snprintf(out, n, "Partition type 0x%02X isn't FAT: format it as FAT32", r.partType); break;
  }
}

inline bool looksMountable(const SectorInfo& r) { return r.layout == L_FAT_VBR || r.layout == L_MBR_FAT; }

struct Diagnosis {
  bool       answered    = false;      // the card replied to CMD0
  bool       initialised = false;      // it finished starting up
  bool       sector0     = false;      // sector 0 could be read
  SectorInfo sector;
  char       text[96]    = "";
};

// SD-card command in SPI mode. Returns R1 (0xFF = no answer). The chip select is left LOW so the caller can read
// more bytes (the rest of an R3 / R7 answer, or a data block); endCmd() releases it.
inline uint8_t sendCmd(uint8_t cs, uint8_t cmd, uint32_t arg, uint8_t crc) {
  digitalWrite(cs, LOW);
  SPI.transfer(0xFF);                                   // lets the card finish whatever it was doing
  SPI.transfer(0x40 | cmd);
  SPI.transfer((uint8_t)(arg >> 24)); SPI.transfer((uint8_t)(arg >> 16));
  SPI.transfer((uint8_t)(arg >> 8));  SPI.transfer((uint8_t)arg);
  SPI.transfer(crc);
  uint8_t r1 = 0xFF;
  for (int i = 0; i < 10 && (r1 & 0x80); i++) r1 = SPI.transfer(0xFF);
  return r1;
}
inline void endCmd(uint8_t cs) { digitalWrite(cs, HIGH); SPI.transfer(0xFF); }

inline bool readSector(uint8_t cs, uint32_t lba, bool blockAddressing, uint8_t* buf) {
  uint8_t r1 = sendCmd(cs, 17, blockAddressing ? lba : lba * 512UL, 0xFF);
  if (r1 != 0x00) { endCmd(cs); return false; }
  uint8_t token = 0xFF;
  const uint32_t t0 = millis();
  while (token == 0xFF && millis() - t0 < 250) { token = SPI.transfer(0xFF); if (token == 0xFF) delay(1); }
  if (token != 0xFE) { endCmd(cs); return false; }       // 0xFE = "data follows"
  for (int i = 0; i < 512; i++) buf[i] = SPI.transfer(0xFF);
  SPI.transfer(0xFF); SPI.transfer(0xFF);                // the two CRC bytes (not checked in SPI mode)
  endCmd(cs);
  return true;
}

inline Diagnosis diagnose(uint8_t cs) {
  Diagnosis d;
  SPI.beginTransaction(SPISettings(DIAG_HZ, MSBFIRST, SPI_MODE0));
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
  for (int i = 0; i < 10; i++) SPI.transfer(0xFF);       // at least 74 clock cycles with the card deselected

  uint8_t r1 = 0xFF;                                     // CMD0: go to the idle state (answer 0x01)
  for (int tries = 0; tries < 5 && r1 != 0x01; tries++) { r1 = sendCmd(cs, 0, 0, 0x95); endCmd(cs); }
  if (r1 != 0x01) {
    snprintf(d.text, sizeof(d.text), "No answer from the card (0x%02X): check the card and its contacts", r1);
    SPI.endTransaction();
    return d;
  }
  d.answered = true;

  bool v2 = false;                                       // CMD8: card version / voltage (v1 cards call it illegal)
  r1 = sendCmd(cs, 8, 0x1AA, 0x87);
  if (r1 == 0x01) { uint8_t r7[4]; for (int i = 0; i < 4; i++) r7[i] = SPI.transfer(0xFF); v2 = (r7[2] == 0x01 && r7[3] == 0xAA); }
  endCmd(cs);

  bool ready = false;                                    // ACMD41 until the card is ready (up to ~1 s)
  const uint32_t t0 = millis();
  while (!ready && millis() - t0 < 1000) {
    r1 = sendCmd(cs, 55, 0, 0x65); endCmd(cs);
    r1 = sendCmd(cs, 41, v2 ? 0x40000000UL : 0UL, 0x77); endCmd(cs);
    if (r1 == 0x00) ready = true; else delay(10);
  }
  if (!ready) {
    snprintf(d.text, sizeof(d.text), "Card answers but doesn't finish starting up (damaged or incompatible)");
    SPI.endTransaction();
    return d;
  }
  d.initialised = true;

  bool blockAddressing = false;                          // CMD58: is it a high-capacity card?
  r1 = sendCmd(cs, 58, 0, 0xFD);
  if (r1 == 0x00) { uint8_t ocr[4]; for (int i = 0; i < 4; i++) ocr[i] = SPI.transfer(0xFF); blockAddressing = (ocr[0] & 0x40) != 0; }
  endCmd(cs);

  static uint8_t sector[512];
  d.sector0 = readSector(cs, 0, blockAddressing, sector);
  SPI.endTransaction();
  if (!d.sector0) { snprintf(d.text, sizeof(d.text), "Card starts up, but its first sector can't be read"); return d; }

  d.sector = analyseSector0(sector);
  describeSector0(d.sector, d.text, sizeof(d.text));
  return d;
}

inline bool mountAt(uint32_t hz) {
  return SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss), SPI, hz, "/sd", 5, false);
}

inline bool mount() {
  Info& i = info();
  i.error[0] = 0;
  i.hz = 0;
  const uint8_t cs = (uint8_t)M5.getPin(m5::pin_name_t::sd_spi_ss);

  uint32_t usedHz = SPI_HZ;
  bool ok = mountAt(SPI_HZ);
  if (!ok) {
    Diagnosis d = diagnose(cs);                          // find out WHY, and say it
    snprintf(i.error, sizeof(i.error), "%.95s", d.text);
    Serial.printf("SD: mount failed - %s\n", d.text);
    if (d.initialised && d.sector0 && looksMountable(d.sector)) {
      // The card is alive and has a FAT volume: maybe the speed is too high for the shared lines.
      static const uint32_t slower[] = { 1000000, 400000 };
      for (uint32_t hz : slower) {
        ok = mountAt(hz);
        Serial.printf("SD: retry at %lu kHz: %s\n", (unsigned long)(hz / 1000), ok ? "worked" : "failed");
        if (ok) { usedHz = hz; break; }
      }
    }
    if (!ok) return false;
    i.error[0] = 0;
  }
  sdcard_type_t t = SD.cardType();
  if (t == CARD_NONE) {
    SD.end();
    snprintf(i.error, sizeof(i.error), "Card did not answer");
    return false;
  }
  i.cardBytes = SD.cardSize();
  i.hz = usedHz;
  setType(i, t);
  mountedFlag() = true;
  mountGeneration()++;
  return true;
}

inline void unmount() {
  if (mountedFlag()) { SD.end(); mountedFlag() = false; mountGeneration()++; }
}

// Looks at the card slot and mounts / unmounts as needed. Cheap when nothing changed (one I2C read).
// Called before every screen redraw and whenever the web page asks for the status, so a card that
// is put in or taken out is noticed. `force` tries an unreadable card again right away.
inline void refresh(bool force = false) {
  Info& i = info();
  int d = readDetect();
  if (d < 0) {                                       // can't tell: keep what we know (no flapping)
    Serial.println("SD: card-detect read failed");
    return;
  }
  const bool wasDetected = i.detected;
  i.detected = (d == 1);

  if (!i.detected) {                                 // slot empty
    unmount();
    if (wasDetected) Serial.println("SD: card removed");
    i.state = SD_ABSENT;
    i.cardBytes = 0;
    i.hz = 0;
    i.type[0] = 0;
    i.error[0] = 0;
    return;
  }

  if (mountedFlag()) { i.state = SD_OK; return; }    // already mounted

  // A card that was just put in comes from the "absent" state, so it is always tried at once. Only a
  // card that already failed (state ERROR) is left alone until the retry interval has passed.
  const uint32_t now = millis();
  if (!force && i.state == SD_ERROR && (now - lastTryMs()) < RETRY_MS) return;

  lastTryMs() = now;
  if (mount()) {
    i.state = SD_OK;
    Serial.printf("SD: card mounted, %s, %lu MB, %lu kHz\n", i.type, (unsigned long)(i.cardBytes / (1024ULL * 1024ULL)), (unsigned long)(i.hz / 1000));
  } else {
    i.state = SD_ERROR;
    Serial.printf("SD: card inserted but unusable: %s\n", i.error);
  }
}

// ---- self-test (web page, Tests tab) -------------------------------------------------------
struct TestResult {
  bool     ok        = false;
  bool     detected  = false;
  bool     mounted   = false;
  uint32_t cardMB    = 0;
  char     type[8]   = "";
  uint32_t writeKBps = 0;
  uint32_t readKBps  = 0;
  char     error[96] = "";
};

constexpr const char* TEST_FILE   = "/pctest.bin";
constexpr size_t      TEST_BLOCKS = 64;      // 64 x 512 bytes = 32 KB

// Writes a 32 KB file, reads it back, compares and deletes it. Takes well under a second.
inline TestResult selfTest() {
  TestResult r;
  refresh(true);                                     // an unreadable card gets another chance
  const Info& i = info();
  r.detected = i.detected;
  r.mounted  = (i.state == SD_OK);
  r.cardMB   = (uint32_t)(i.cardBytes / (1024ULL * 1024ULL));
  snprintf(r.type, sizeof(r.type), "%.*s", (int)sizeof(r.type) - 1, i.type);

  if (!i.detected)         { snprintf(r.error, sizeof(r.error), "No card detected");       return r; }
  if (i.state != SD_OK)    { snprintf(r.error, sizeof(r.error), "%.*s", (int)sizeof(r.error) - 1, i.error); return r; }

  uint8_t block[512];
  for (size_t k = 0; k < sizeof(block); k++) block[k] = (uint8_t)(k * 7 + 13);

  uint32_t t0 = millis();
  File f = SD.open(TEST_FILE, FILE_WRITE);
  if (!f) { snprintf(r.error, sizeof(r.error), "Can't create a file (write-protected?)"); return r; }
  for (size_t b = 0; b < TEST_BLOCKS; b++) {
    block[0] = (uint8_t)b;                           // every block differs
    if (f.write(block, sizeof(block)) != sizeof(block)) {
      f.close();
      SD.remove(TEST_FILE);
      snprintf(r.error, sizeof(r.error), "Write failed (card full or faulty?)");
      return r;
    }
  }
  f.close();
  uint32_t writeMs = millis() - t0;

  t0 = millis();
  File g = SD.open(TEST_FILE, FILE_READ);
  if (!g) { SD.remove(TEST_FILE); snprintf(r.error, sizeof(r.error), "Can't read the test file back"); return r; }
  uint8_t back[512];
  bool same = true;
  for (size_t b = 0; b < TEST_BLOCKS && same; b++) {
    block[0] = (uint8_t)b;
    if (g.read(back, sizeof(back)) != sizeof(back) || memcmp(back, block, sizeof(block)) != 0) same = false;
  }
  g.close();
  uint32_t readMs = millis() - t0;
  SD.remove(TEST_FILE);
  if (!same) { snprintf(r.error, sizeof(r.error), "Data read back differs (unreliable bus?)"); return r; }

  const uint32_t kb = (uint32_t)(TEST_BLOCKS * sizeof(block) / 1024);
  r.writeKBps = writeMs ? (kb * 1000UL) / writeMs : kb * 1000UL;
  r.readKBps  = readMs  ? (kb * 1000UL) / readMs  : kb * 1000UL;
  r.ok = true;
  Serial.printf("SD self-test OK: write %lu KB/s, read %lu KB/s\n", (unsigned long)r.writeKBps, (unsigned long)r.readKBps);
  return r;
}

}  // namespace sdcard
