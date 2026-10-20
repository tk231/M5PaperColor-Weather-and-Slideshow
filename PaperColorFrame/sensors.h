// sensors.h - onboard SHT40 (temp/humidity) and RX8130 RTC helpers
//
// SHT40 read sequence adapted from M5Stack's PaperColor-UserDemo (hal.cpp, MIT),
// with a CRC check added.
#pragma once
#include <M5Unified.h>
#include <M5PM1.h>   // Library Manager: "M5PM1" by M5Stack
#include <time.h>
#include <Preferences.h>
#include "types.h"
#include "timeutil.h"

namespace sensors {

constexpr uint8_t  SHT4X_ADDR       = 0x44;
constexpr uint8_t  SHT4X_CMD_HI_PRE = 0xFD;  // high precision, ~8.3 ms
constexpr uint32_t I2C_FREQ         = 400000;

// Sensirion CRC-8: poly 0x31, init 0xFF
inline uint8_t crc8(const uint8_t* d, size_t n) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

// Returns false on I2C error or CRC mismatch.
inline bool readSht40(float& tempC, float& humPct) {
  uint8_t cmd = SHT4X_CMD_HI_PRE;
  uint8_t buf[6] = {};         // (zeroed: if the read fails, the checksum test below must not look at leftovers)

  if (!M5.In_I2C.start(SHT4X_ADDR, false, I2C_FREQ)) return false;
  M5.In_I2C.write(&cmd, 1);
  M5.In_I2C.stop();

  delay(10);

  if (!M5.In_I2C.start(SHT4X_ADDR, true, I2C_FREQ)) return false;
  M5.In_I2C.read(buf, 6);
  M5.In_I2C.stop();

  if (crc8(buf, 2) != buf[2] || crc8(buf + 3, 2) != buf[5]) return false;

  uint16_t rawT = ((uint16_t)buf[0] << 8) | buf[1];
  uint16_t rawH = ((uint16_t)buf[3] << 8) | buf[4];

  tempC  = -45.0f + 175.0f * rawT / 65535.0f;
  humPct = -6.0f + 125.0f * rawH / 65535.0f;
  if (humPct > 100.0f) humPct = 100.0f;
  if (humPct < 0.0f) humPct = 0.0f;
  return true;
}

// Print every responding I2C address (expect 0x32 RTC and 0x44 SHT40, plus the PM1).
inline void scanI2C() {
  Serial.print("I2C devices:");
  for (uint8_t a = 0x08; a < 0x78; a++) {
    if (M5.In_I2C.scanID(a, 100000)) Serial.printf(" 0x%02X", a);
  }
  Serial.println();
}

// ---- Time zone + local time --------------------------------------------------
// The RTC keeps UTC. The local time zone is a plain offset (minutes east of UTC) that
// the phone sends when it connects; it is stored in flash. Changes of daylight saving
// time are NOT followed automatically - the next phone sync corrects the offset.

inline int& tzOffsetMin() { static int v = 0; return v; }
inline char* tzName() { static char n[48] = ""; return n; }

inline void loadTz() {
  Preferences p;
  p.begin("frame", false);
  tzOffsetMin() = p.getInt("tzoff", 0);
  String n = p.getString("tzname", "");
  p.end();
  snprintf(tzName(), 48, "%s", n.c_str());
}

inline void saveTz(int offsetMin, const char* name) {
  tzOffsetMin() = offsetMin;
  snprintf(tzName(), 48, "%s", name ? name : "");
  Preferences p;
  p.begin("frame", false);
  p.putInt("tzoff", offsetMin);
  p.putString("tzname", tzName());
  p.end();
}

struct LocalTime {
  bool    ok       = false;
  int64_t epochUtc = 0;          // seconds since 1970-01-01 UTC (what the RTC holds)
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;   // LOCAL time
};

// Reads the RTC (UTC) and applies the stored offset.
inline bool readLocalTime(LocalTime& lt) {
  lt = LocalTime();
  if (!M5.Rtc.isEnabled()) return false;
  m5::rtc_date_t d;
  m5::rtc_time_t t;
  if (!M5.Rtc.getDateTime(&d, &t)) return false;
  if (d.year < 2000 || d.month < 1 || d.month > 12 || d.date < 1 || d.date > 31) return false;   // RTC not set / garbage

  lt.epochUtc = timeutil::epochFromCivil(d.year, d.month, d.date, t.hours, t.minutes, t.seconds);
  timeutil::Civil c = timeutil::civilFromEpoch(lt.epochUtc + (int64_t)tzOffsetMin() * 60);
  lt.year = c.year; lt.month = c.month; lt.day = c.day;
  lt.hour = c.hour; lt.minute = c.minute; lt.second = c.second;
  lt.ok = true;
  return true;
}

// Writes a UTC epoch into the RTC.
inline bool setRtcFromEpoch(int64_t epochUtc) {
  if (!M5.Rtc.isEnabled()) return false;
  timeutil::Civil c = timeutil::civilFromEpoch(epochUtc);
  struct tm t = {};
  t.tm_year = c.year - 1900;
  t.tm_mon  = c.month - 1;
  t.tm_mday = c.day;
  t.tm_hour = c.hour;
  t.tm_min  = c.minute;
  t.tm_sec  = c.second;
  t.tm_wday = c.weekday;
  M5.Rtc.setDateTime(&t);
  return true;
}

// ---- RTC fallback from the build time ----------------------------------------

// Parse __DATE__ / __TIME__ ("Oct 15 2026" / "15:13:20") into a tm.
inline bool buildTime(struct tm& t) {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int d = 0, y = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(__DATE__, "%3s %d %d", mon, &d, &y) != 3) return false;
  if (sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss) != 3) return false;
  const char* p = strstr(months, mon);
  if (!p) return false;
  t = {};
  t.tm_year = y - 1900;
  t.tm_mon  = (int)((p - months) / 3);
  t.tm_mday = d;
  t.tm_hour = hh;
  t.tm_min  = mm;
  t.tm_sec  = ss;
  mktime(&t);  // normalises and fills tm_wday
  return true;
}

// If the RTC is missing a plausible time (year earlier than the build year),
// or force==true, set it from the compile time (your PC's local time).
// Returns true if the RTC was (re)set.
inline bool ensureRtcTime(bool force = false) {
  if (!M5.Rtc.isEnabled()) return false;
  struct tm b;
  if (!buildTime(b)) return false;

  m5::rtc_date_t date;
  m5::rtc_time_t time;
  bool ok = M5.Rtc.getDateTime(&date, &time);
  bool stale = !ok || date.year < (b.tm_year + 1900) || M5.Rtc.getVoltLow();

  if (force || stale) {
    M5.Rtc.setDateTime(&b);   // the build time is the PC's local time: store it as-is and use offset 0
    saveTz(0, "");
    return true;
  }
  return false;
}

// ---- Battery (via the M5PM1 power-management chip) ------------------------
// Same approach as the factory firmware's /api/battery endpoint.

inline M5PM1& pm1() {
  static M5PM1 instance;
  return instance;
}

inline bool initBattery() {
  if (pm1().begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK) return false;
  pm1().setI2cConfig(0);          // same call the factory firmware makes after begin()
  pm1().setChargeEnable(true);    // factory firmware does this too; CHG_EN auto-clears on reset, so do it every boot
  return true;
}

// Battery voltage in mV. Returns false if the read failed.
inline bool readBatteryMv(uint16_t& mv) {
  mv = 0;
  return pm1().readVbat(&mv) == M5PM1_OK && mv > 0;
}

// ---- Combined power status (battery + USB + charging guess) ---------------
// The PM1 exposes no charge-status register we can read, so "charging" is a
// heuristic: USB present and battery voltage below ~4.15 V. Serial output logs
// the raw values so the thresholds can be tuned on real hardware.
inline PowerStatus readPower() {
  PowerStatus p;
  p.batOk = readBatteryMv(p.batMv);

  m5pm1_pwr_src_t src;
  bool srcOk = (pm1().getPowerSource(&src) == M5PM1_OK);
  if (srcOk) p.src = (int)src;

  uint16_t vin = 0;
  if (pm1().readVin(&vin) == M5PM1_OK) p.vinMv = vin;

  p.usb = (srcOk && src == M5PM1_PWR_SRC_5VIN) || p.vinMv > 4000;
  p.charging = p.usb && p.batOk && p.batMv < 4150;
  return p;
}

// Switches the whole device off: the PM1 cuts the power of the ESP32 (the factory firmware's Hal::powerOff does the same).
// Returns only if that did not work, which is what happens while USB power keeps the device running.
inline bool powerOff() {
  return pm1().shutdown() == M5PM1_OK;
}

}  // namespace sensors
