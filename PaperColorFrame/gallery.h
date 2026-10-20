// gallery.h - the picture library on the microSD card.
//
//   /images/NAME.png    the pictures that are shown: dithered PNGs, 600x400 (or 400x600)
//   /masters/NAME.jpg   the ORIGINAL of each picture, same base name (.jpg .jpeg .png .tif .tiff), kept so it can be edited again
//   /thumbs/NAME.png    small previews, optional (the page falls back to the picture itself)
//   /playlist.json      written by the device: which pictures are on, and in what order
//
// THE FILES ARE THE TRUTH. Every scan lists /images and reconciles /playlist.json with it: pictures that
// are no longer there are dropped from the playlist, new files are appended (switched on), the saved
// order and on/off flags are kept for the rest. So a card edited on a computer can never break the list.
// Anything in /images that is not a usable picture file name is ignored, and counted ("ignored").
//
// A picture's id is its file name without ".png": 1..40 characters of letters, digits, "_" and "-".
// (That also keeps ids safe to use in URLs: no "..", no "/", no spaces.)
//
// Uploads (see the bottom of this file): the page sends the picture (+ an optional thumbnail) in one request
// and the large original JPEG in a second one. Parts are received into /upload/*.tmp and only renamed into
// place when complete and valid, so an interrupted upload never leaves a broken picture. Leftovers of an
// interrupted upload are removed at the next scan.
//
// The first half of this file is pure logic (no hardware). The second half reads and writes the card.
#pragma once
#include <SD.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sdcard.h"

namespace gallery {

constexpr int MAX_ITEMS = 120;
constexpr int ID_MAX    = 40;

constexpr const char* DIR_IMAGES  = "/images";
constexpr const char* DIR_THUMBS  = "/thumbs";
constexpr const char* DIR_MASTERS = "/masters";
constexpr const char* DIR_UPLOAD  = "/upload";
constexpr const char* PLAYLIST    = "/playlist.json";
constexpr const char* PLAYLIST_TMP = "/playlist.tmp";

struct Item {
  char     id[ID_MAX + 1];
  bool     on;              // included in the slideshow
  bool     ok;              // the file starts like a PNG and its size could be read
  uint16_t w, h;            // pixel size from the PNG header (0 if not readable)
  uint32_t bytes;           // size of the PNG file
  bool     thumb;           // /thumbs/<id>.png exists
  uint8_t  master;          // 0 = no original, else which file: see masterExt() (1 jpg, 2 jpeg, 3 png, 4 tif, 5 tiff)
};

struct Library {
  Item     items[MAX_ITEMS];
  int      n          = 0;
  int      ignored    = 0;       // files in /images that are not usable pictures (or beyond MAX_ITEMS)
  bool     loaded     = false;
  uint32_t generation = 0;       // sdcard::mountGeneration() this list was read under
};

inline Library& lib() { static Library l; return l; }

struct Saved { char id[ID_MAX + 1]; bool on; };

// ============================================================================
//  Pure logic
// ============================================================================
inline bool validId(const char* s) {
  if (!s) return false;
  size_t n = strlen(s);
  if (n < 1 || n > (size_t)ID_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

// Splits "P0001.PNG" into "P0001" and "png" (lower case). Fails if there is no extension or something is too long.
inline bool splitName(const char* name, char* base, char* ext) {
  const char* dot = strrchr(name, '.');
  if (!dot || dot == name) return false;
  size_t nb = (size_t)(dot - name), ne = strlen(dot + 1);
  if (nb > (size_t)ID_MAX || ne < 1 || ne > 4) return false;
  memcpy(base, name, nb); base[nb] = 0;
  for (size_t i = 0; i < ne; i++) { char c = dot[1 + i]; ext[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
  ext[ne] = 0;
  return true;
}

// "P0001.png" -> id "P0001". Other extensions, odd characters (also macOS "._P0001.png"), over-long names: false.
inline bool idFromPngName(const char* name, char* id) {
  char ext[8];
  if (!splitName(name, id, ext)) return false;
  return strcmp(ext, "png") == 0 && validId(id);
}

inline uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

// First 24 bytes of a PNG file: signature, then the IHDR chunk with width and height.
inline bool parsePngHeader(const uint8_t* b, uint16_t& w, uint16_t& h) {
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
  if (memcmp(b, sig, 8) != 0 || memcmp(b + 12, "IHDR", 4) != 0) return false;
  uint32_t W = be32(b + 16), H = be32(b + 20);
  if (W == 0 || H == 0 || W > 65535 || H > 65535) return false;
  w = (uint16_t)W; h = (uint16_t)H;
  return true;
}

// The panel is 600x400, mounted either way round.
inline bool fitsPanel(uint16_t w, uint16_t h) { return (w == 600 && h == 400) || (w == 400 && h == 600); }

// ---- originals: the kinds of file that may be kept in /masters ------------------------------------
constexpr int MASTER_KINDS = 6;
inline const char* masterExt(uint8_t code) {
  static const char* e[MASTER_KINDS] = { "", "jpg", "jpeg", "png", "tif", "tiff" };
  return code < MASTER_KINDS ? e[code] : "";
}
inline uint8_t masterCode(const char* ext) {                 // 0 if this is not an extension we keep
  for (uint8_t c = 1; c < MASTER_KINDS; c++) if (strcmp(ext, masterExt(c)) == 0) return c;
  return 0;
}
inline const char* masterType(uint8_t code) {
  switch (code) {
    case 1: case 2: return "image/jpeg";
    case 3:         return "image/png";
    case 4: case 5: return "image/tiff";
    default:        return "application/octet-stream";
  }
}
// What a file really is, judged by its first bytes (never by its name): 1 = JPEG, 3 = PNG, 4 = TIFF, 0 = anything else.
inline uint8_t masterKindOfBytes(const uint8_t* h, size_t n) {
  if (n >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF) return 1;
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
  if (n >= 8 && memcmp(h, sig, 8) == 0) return 3;
  if (n >= 4 && ((h[0] == 'I' && h[1] == 'I' && h[2] == 0x2A && h[3] == 0x00) || (h[0] == 'M' && h[1] == 'M' && h[2] == 0x00 && h[3] == 0x2A))) return 4;
  return 0;
}

inline int find(const Library& l, const char* id) {
  for (int i = 0; i < l.n; i++) if (strcmp(l.items[i].id, id) == 0) return i;
  return -1;
}

// FAT ignores capitals, so "p0001" and "P0001" are the same file: used where a new name must not collide.
inline int findNoCase(const Library& l, const char* id) {
  for (int i = 0; i < l.n; i++) if (strcasecmp(l.items[i].id, id) == 0) return i;
  return -1;
}

inline int enabledCount(const Library& l) {
  int c = 0;
  for (int i = 0; i < l.n; i++) if (l.items[i].on) c++;
  return c;
}

inline void sortById(Library& l) {                      // plain insertion sort by id (the list is short)
  for (int a = 1; a < l.n; a++) {
    Item x = l.items[a];
    int b = a - 1;
    while (b >= 0 && strcmp(l.items[b].id, x.id) > 0) { l.items[b + 1] = l.items[b]; b--; }
    l.items[b + 1] = x;
  }
}

// Reconciles a freshly scanned library with the saved playlist:
//   - pictures named in `saved` come first, in that order, with their saved on/off;
//   - pictures that are not in `saved` (new files) follow, sorted by name, switched on;
//   - saved entries without a file are dropped (and repeated ids are used once).
// Returns true if the result differs from `saved`, i.e. the playlist file should be rewritten.
inline bool applyPlaylist(Library& l, const Saved* saved, int nSaved) {
  static Item out[MAX_ITEMS];
  static bool placed[MAX_ITEMS];
  sortById(l);
  for (int i = 0; i < l.n; i++) placed[i] = false;
  int k = 0;
  bool changed = false;
  for (int s = 0; s < nSaved; s++) {
    int i = find(l, saved[s].id);
    if (i < 0 || placed[i]) { changed = true; continue; }
    placed[i] = true;
    out[k] = l.items[i];
    out[k].on = saved[s].on;
    k++;
  }
  for (int i = 0; i < l.n; i++) {
    if (placed[i]) continue;
    out[k] = l.items[i];
    out[k].on = true;
    k++;
    changed = true;
  }
  for (int i = 0; i < k; i++) l.items[i] = out[i];
  return changed;
}

inline bool setEnabled(Library& l, const char* id, bool on) {
  int i = find(l, id);
  if (i < 0) return false;
  l.items[i].on = on;
  return true;
}

// ids = "P0003,P0001": those pictures first, in that order; the others keep their order after them.
// Every id must exist and none may repeat, otherwise nothing changes and false is returned.
inline bool setOrder(Library& l, const char* ids) {
  static Item out[MAX_ITEMS];
  static bool placed[MAX_ITEMS];
  for (int i = 0; i < l.n; i++) placed[i] = false;
  int k = 0;
  const char* p = ids;
  while (p && *p) {
    const char* comma = strchr(p, ',');
    size_t len = comma ? (size_t)(comma - p) : strlen(p);
    char id[ID_MAX + 1];
    if (len < 1 || len > (size_t)ID_MAX) return false;
    memcpy(id, p, len); id[len] = 0;
    int i = find(l, id);
    if (i < 0 || placed[i]) return false;
    placed[i] = true;
    out[k++] = l.items[i];
    p = comma ? comma + 1 : nullptr;
  }
  if (k == 0) return false;
  for (int i = 0; i < l.n; i++) if (!placed[i]) out[k++] = l.items[i];
  for (int i = 0; i < k; i++) l.items[i] = out[i];
  return true;
}

inline void removeAt(Library& l, int idx) {
  if (idx < 0 || idx >= l.n) return;
  for (int i = idx; i + 1 < l.n; i++) l.items[i] = l.items[i + 1];
  l.n--;
}

// ---- JSON ---------------------------------------------------------------------------
// The picture list for the web page. Ids are plain [A-Za-z0-9_-], so they need no escaping.
inline void libraryJson(const Library& l, const char* sdState, const char* sdErrorEscaped, String& out) {
  char buf[420];                                           // the header holds the (possibly long) card error text
  out = "";
  out.reserve(160 + (size_t)l.n * 110);
  snprintf(buf, sizeof(buf), "{\"sd\":\"%s\",\"error\":\"%s\",\"max\":%d,\"ignored\":%d,\"total\":%d,\"enabled\":%d,\"items\":[",
           sdState, sdErrorEscaped, MAX_ITEMS, l.ignored, l.n, enabledCount(l));
  out += buf;
  for (int i = 0; i < l.n; i++) {
    const Item& it = l.items[i];
    snprintf(buf, sizeof(buf), "%s{\"id\":\"%s\",\"on\":%d,\"ok\":%d,\"w\":%u,\"h\":%u,\"fits\":%d,\"bytes\":%lu,\"thumb\":%d,\"master\":%d,\"mext\":\"%s\"}",
             i ? "," : "", it.id, it.on ? 1 : 0, it.ok ? 1 : 0, (unsigned)it.w, (unsigned)it.h,
             (it.ok && fitsPanel(it.w, it.h)) ? 1 : 0, (unsigned long)it.bytes, it.thumb ? 1 : 0, (int)it.master, masterExt(it.master));
    out += buf;
  }
  out += "]}";
}

// ---- playlist.json: {"v":1,"items":[{"id":"P0001","on":1},...]} --------------------------------
inline void playlistJson(const Library& l, String& out) {
  char buf[96];
  out = "{\"v\":1,\"items\":[";
  for (int i = 0; i < l.n; i++) {
    snprintf(buf, sizeof(buf), "%s{\"id\":\"%s\",\"on\":%d}", i ? "," : "", l.items[i].id, l.items[i].on ? 1 : 0);
    out += buf;
  }
  out += "]}";
}

// Finds the value of "key" (after the colon) inside [from, to), tolerant of spaces and line breaks.
inline const char* jsonValue(const char* from, const char* to, const char* key) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const size_t plen = strlen(pat);
  const char* p = from;
  while ((p = strstr(p, pat)) != nullptr && p < to) {
    const char* q = p + plen;
    while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++;
    if (*q == ':') {
      q++;
      while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++;
      return q;
    }
    p += plen;
  }
  return nullptr;
}

// Reads the saved playlist (also if it was edited by hand). Entries with an invalid id are skipped.
inline int parsePlaylist(const char* text, Saved* out, int maxN) {
  const char* end = text + strlen(text);
  int n = 0;
  const char* pos = text;
  while (n < maxN) {
    const char* v = jsonValue(pos, end, "id");
    if (!v) break;
    const char* afterId = v;
    char id[ID_MAX + 2];
    bool idOk = false;
    if (*v == '"') {
      const char* e = strchr(v + 1, '"');
      if (e && (size_t)(e - v - 1) <= (size_t)ID_MAX) {
        size_t len = (size_t)(e - v - 1);
        memcpy(id, v + 1, len); id[len] = 0;
        idOk = validId(id);
        afterId = e + 1;
      }
    }
    const char* nextId = jsonValue(afterId, end, "id");
    const char* limit = nextId ? nextId : end;
    bool on = true;
    const char* ov = jsonValue(afterId, limit, "on");
    if (ov && ov < limit) on = !(*ov == '0' || strncmp(ov, "false", 5) == 0);
    if (idOk) { size_t len = strlen(id); memcpy(out[n].id, id, len); out[n].id[len] = 0; out[n].on = on; n++; }   // validId() guarantees len <= ID_MAX
    pos = afterId;
    if (pos >= end) break;
  }
  return n;
}

// ============================================================================
//  The card
// ============================================================================
template <typename F>
inline void forEachFile(const char* dir, F f) {
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) return;
  for (File e = d.openNextFile(); e; e = d.openNextFile()) {
    if (!e.isDirectory()) {
      const char* nm = e.name();
      const char* slash = strrchr(nm, '/');
      if (slash) nm = slash + 1;
      f(nm, (uint32_t)e.size());
    }
    e.close();
  }
  d.close();
}

inline void ensureFolders() {
  const char* dirs[4] = { DIR_IMAGES, DIR_THUMBS, DIR_MASTERS, DIR_UPLOAD };
  for (const char* d : dirs) if (!SD.exists(d)) SD.mkdir(d);
}

inline void pathFor(const char* dir, const char* id, const char* ext, char* out, size_t n) {
  snprintf(out, n, "%s/%s.%s", dir, id, ext);
}


// ---- upload state ------------------------------------------------------------------------
constexpr size_t   MAX_PNG_BYTES    = 3UL * 1024 * 1024;
constexpr size_t   MAX_THUMB_BYTES  = 256UL * 1024;
constexpr size_t   MAX_MASTER_BYTES = 32UL * 1024 * 1024;
constexpr size_t   UPLOAD_BUF       = 8192;      // received data is collected into blocks this big before it is written

enum PartKind : uint8_t { PART_NONE = 0, PART_PNG, PART_THUMB, PART_MASTER, PART_COUNT };

struct Part {
  bool    seen = false;        // a part with this name started
  bool    done = false;        // ... and arrived completely and was written
  bool    tooBig = false;
  bool    ioError = false;
  size_t  bytes = 0;
  uint8_t head[24] = {};       // the first bytes (to recognise PNG / JPEG and read the size)
  size_t  headLen = 0;
};

struct Upload {
  Part     parts[PART_COUNT];
  PartKind current = PART_NONE;
  bool     active = false;     // a request is being received
  File     file;
  uint8_t  buf[UPLOAD_BUF];
  size_t   fill = 0;
};
inline Upload& upload() { static Upload u; return u; }

inline const char* tempPath(PartKind k) {
  switch (k) {
    case PART_PNG:    return "/upload/png.tmp";
    case PART_THUMB:  return "/upload/thumb.tmp";
    case PART_MASTER: return "/upload/master.tmp";
    default:          return "";
  }
}
inline PartKind partKind(const char* field) {
  if (strcmp(field, "png") == 0)    return PART_PNG;
  if (strcmp(field, "thumb") == 0)  return PART_THUMB;
  if (strcmp(field, "master") == 0) return PART_MASTER;
  return PART_NONE;
}
inline size_t partLimit(PartKind k) { return k == PART_PNG ? MAX_PNG_BYTES : (k == PART_THUMB ? MAX_THUMB_BYTES : MAX_MASTER_BYTES); }

// Whatever is left in /upload is the remains of an upload that never finished (power cut, phone gone).
inline void cleanupUploadDir() {
  if (upload().active) return;
  char names[8][40];
  int n = 0;
  forEachFile(DIR_UPLOAD, [&](const char* name, uint32_t) { if (n < 8) snprintf(names[n++], 40, "%s", name); });
  for (int i = 0; i < n; i++) { char path[64]; snprintf(path, sizeof(path), "%s/%.39s", DIR_UPLOAD, names[i]); SD.remove(path); }
}

inline int loadPlaylist(Saved* out, int maxN) {
  File f = SD.open(PLAYLIST);
  if (!f) return 0;
  static char text[12288];
  size_t len = f.size();
  if (len == 0 || len >= sizeof(text)) { f.close(); return 0; }
  size_t got = 0;
  while (got < len) {
    size_t k = f.read((uint8_t*)text + got, len - got);
    if (k == 0) break;
    got += k;
  }
  f.close();
  text[got] = 0;
  return parsePlaylist(text, out, maxN);
}

// Writes the playlist to a temporary file and renames it. (FAT can't rename over an existing file, so the
// old one is removed first; a power cut in between only loses the on/off flags and order: the next scan
// rebuilds the playlist from the files.)
inline bool savePlaylist(const Library& l) {
  String json;
  playlistJson(l, json);
  SD.remove(PLAYLIST_TMP);
  File f = SD.open(PLAYLIST_TMP, FILE_WRITE);
  if (!f) return false;
  size_t w = f.write((const uint8_t*)json.c_str(), json.length());
  f.close();
  if (w != json.length()) { SD.remove(PLAYLIST_TMP); return false; }
  SD.remove(PLAYLIST);
  if (SD.rename(PLAYLIST_TMP, PLAYLIST)) return true;
  File g = SD.open(PLAYLIST, FILE_WRITE);                // the rename failed: write the playlist in place instead
  bool ok = g && g.write((const uint8_t*)json.c_str(), json.length()) == json.length();
  g.close();
  SD.remove(PLAYLIST_TMP);
  return ok;
}

inline void scan(Library& l) {
  l.n = 0;
  l.ignored = 0;
  ensureFolders();
  cleanupUploadDir();

  forEachFile(DIR_IMAGES, [&](const char* name, uint32_t size) {
    char id[ID_MAX + 1];
    if (!idFromPngName(name, id) || l.n >= MAX_ITEMS || find(l, id) >= 0) { l.ignored++; return; }
    Item& it = l.items[l.n++];
    memset(&it, 0, sizeof(it));
    snprintf(it.id, sizeof(it.id), "%s", id);
    it.on = true;
    it.bytes = size;
    char path[80];
    pathFor(DIR_IMAGES, id, "png", path, sizeof(path));
    File f = SD.open(path);
    uint8_t head[24];
    size_t got = 0;
    while (f && got < sizeof(head)) { const size_t k = f.read(head + got, sizeof(head) - got); if (k == 0) break; got += k; }      // (a read may hand over less than asked for)
    if (got == sizeof(head)) it.ok = parsePngHeader(head, it.w, it.h);
    f.close();
  });

  forEachFile(DIR_THUMBS, [&](const char* name, uint32_t) {
    char base[ID_MAX + 1], ext[8];
    int i;
    if (splitName(name, base, ext) && strcmp(ext, "png") == 0 && (i = find(l, base)) >= 0) l.items[i].thumb = true;
  });
  forEachFile(DIR_MASTERS, [&](const char* name, uint32_t) {
    char base[ID_MAX + 1], ext[8];
    int i;
    if (!splitName(name, base, ext) || (i = find(l, base)) < 0) return;
    uint8_t code = masterCode(ext);
    if (code && (l.items[i].master == 0 || code < l.items[i].master)) l.items[i].master = code;      // if there are several, the lowest code wins
  });

  static Saved saved[MAX_ITEMS];
  int nSaved = loadPlaylist(saved, MAX_ITEMS);
  bool changed = applyPlaylist(l, saved, nSaved);
  if (changed) savePlaylist(l);                      // new / missing pictures, or no readable playlist yet

  Serial.printf("Gallery: %d picture(s), %d on, %d file(s) ignored\n", l.n, enabledCount(l), l.ignored);
}

// True when a card is mounted and the list is current; scans again after the card was changed.
inline bool ensureLoaded() {
  sdcard::refresh();
  Library& l = lib();
  if (sdcard::info().state != SD_OK) { l.n = 0; l.ignored = 0; l.loaded = false; return false; }
  if (!l.loaded || l.generation != sdcard::mountGeneration()) {
    scan(l);
    l.generation = sdcard::mountGeneration();
    l.loaded = true;
  }
  return true;
}

inline bool rescan() {
  lib().loaded = false;
  return ensureLoaded();
}

// Deletes the picture, its thumbnail and its original from the card. False if the picture file itself can't be removed.
inline bool removeFiles(const char* id) {
  char path[80];
  pathFor(DIR_IMAGES, id, "png", path, sizeof(path));
  if (SD.exists(path) && !SD.remove(path)) return false;
  pathFor(DIR_THUMBS, id, "png", path, sizeof(path));   SD.remove(path);
  for (uint8_t c = 1; c < MASTER_KINDS; c++) { pathFor(DIR_MASTERS, id, masterExt(c), path, sizeof(path)); SD.remove(path); }
  return true;
}

// ============================================================================
//  Receiving an upload
//  The web server hands over every file part as START, a number of WRITEs, END (or ABORTED if the phone
//  goes away). Then the request's "done" handler runs commitPicture() or commitMaster().
// ============================================================================
inline void uploadReset() {                              // forget everything and delete the temporary files
  Upload& u = upload();
  if (u.file) u.file.close();
  u.file = File();
  if (sdcard::info().state == SD_OK) for (int k = PART_PNG; k < PART_COUNT; k++) SD.remove(tempPath((PartKind)k));
  for (int k = 0; k < PART_COUNT; k++) u.parts[k] = Part();
  u.current = PART_NONE;
  u.active = false;
  u.fill = 0;
}

inline void uploadFlush(Upload& u, Part& p) {
  if (u.fill == 0) return;
  if (!u.file || u.file.write(u.buf, u.fill) != u.fill) p.ioError = true;
  u.fill = 0;
}

inline void uploadBegin(const char* field) {
  Upload& u = upload();
  const PartKind want = partKind(field);
  // The picture ("png") is always the FIRST part of its request. So if it starts while anything is pending, the
  // earlier request died without a word: forget it. (A request for an original only reads its own part.)
  const bool pending = u.parts[PART_PNG].seen || u.parts[PART_THUMB].seen || u.parts[PART_MASTER].seen;
  if (u.active && want == PART_PNG && pending) uploadReset();
  if (!u.active) {                                        // the first part of a new request (nothing is pending: every request ends with a reset)
    ensureLoaded();                                       // (may scan the card: do that before any temporary file exists)
    u.active = true;
  }
  u.current = want;
  if (want == PART_NONE) return;                          // not one of ours: its data is ignored
  Part& p = u.parts[want];
  p = Part();
  p.seen = true;
  u.fill = 0;
  if (sdcard::info().state != SD_OK) { p.ioError = true; return; }       // no card: the request is answered with "no card"
  if (!SD.exists(DIR_UPLOAD)) SD.mkdir(DIR_UPLOAD);
  SD.remove(tempPath(want));
  u.file = SD.open(tempPath(want), FILE_WRITE);
  if (!u.file) p.ioError = true;
}

inline void uploadWrite(const uint8_t* data, size_t n) {
  Upload& u = upload();
  if (u.current == PART_NONE) return;
  Part& p = u.parts[u.current];
  if (p.ioError || p.tooBig) return;
  size_t i = 0;
  while (p.headLen < sizeof(p.head) && i < n) p.head[p.headLen++] = data[i++];
  p.bytes += n;
  if (p.bytes > partLimit(u.current)) {                   // too big: stop writing, throw the partial file away
    p.tooBig = true;
    if (u.file) u.file.close();
    SD.remove(tempPath(u.current));
    u.fill = 0;
    return;
  }
  size_t off = 0;
  while (off < n) {
    size_t room = UPLOAD_BUF - u.fill;
    size_t take = (n - off < room) ? n - off : room;
    memcpy(u.buf + u.fill, data + off, take);
    u.fill += take;
    off += take;
    if (u.fill == UPLOAD_BUF) { uploadFlush(u, p); if (p.ioError) return; }
  }
}

inline void uploadEnd() {
  Upload& u = upload();
  if (u.current == PART_NONE) return;
  Part& p = u.parts[u.current];
  if (!p.ioError && !p.tooBig) uploadFlush(u, p);
  if (u.file) u.file.close();
  p.done = !p.ioError && !p.tooBig;
  if (!p.done) SD.remove(tempPath(u.current));
  u.current = PART_NONE;
  u.fill = 0;
}

inline void uploadAbort() { uploadReset(); }

// ---- committing ---------------------------------------------------------------------------
struct Outcome {
  int  code = 200;                   // HTTP status
  char message[96] = "";
  char id[ID_MAX + 1] = "";          // the picture that was added / changed
};

inline bool fail(Outcome& o, int code, const char* msg) {
  o.code = code;
  snprintf(o.message, sizeof(o.message), "%s", msg);
  uploadReset();
  return false;
}

// "name" if free, else "name-2", "name-3", ... (capitals do not count: FAT would treat them as the same file)
inline bool uniqueId(const Library& l, const char* base, char* out) {
  if (findNoCase(l, base) < 0) { snprintf(out, ID_MAX + 1, "%s", base); return true; }
  for (int n = 2; n < 1000; n++) {
    char suf[8];
    int sl = snprintf(suf, sizeof(suf), "-%d", n);
    size_t keep = strlen(base);
    if (keep + (size_t)sl > (size_t)ID_MAX) keep = (size_t)(ID_MAX - sl);
    char cand[ID_MAX + 2];
    memcpy(cand, base, keep);
    memcpy(cand + keep, suf, (size_t)sl + 1);
    if (findNoCase(l, cand) < 0) { snprintf(out, ID_MAX + 1, "%.40s", cand); return true; }
  }
  return false;
}

inline uint8_t masterOnCard(const char* id) {            // an original that is already there (lowest code wins, as in a scan)
  char path[80];
  for (uint8_t c = 1; c < MASTER_KINDS; c++) { pathFor(DIR_MASTERS, id, masterExt(c), path, sizeof(path)); if (SD.exists(path)) return c; }
  return 0;
}

// The request carried the picture (field "png") and perhaps a thumbnail (field "thumb").
inline bool commitPicture(const char* wanted, bool replace, Outcome& o) {
  Upload& u = upload();
  if (!ensureLoaded()) return fail(o, 409, "no card");
  Library& l = lib();
  const Part& png = u.parts[PART_PNG];
  if (!png.seen)    return fail(o, 400, "no picture in the upload");
  if (png.tooBig)   return fail(o, 413, "the picture is too big (3 MB at most)");
  if (png.ioError)  return fail(o, 500, "can't write to the card");
  if (!png.done)    return fail(o, 400, "the picture did not arrive completely");
  uint16_t w = 0, h = 0;
  if (png.headLen < sizeof(png.head) || !parsePngHeader(png.head, w, h)) return fail(o, 400, "that is not a PNG file");
  if (!fitsPanel(w, h)) {
    char msg[96];
    snprintf(msg, sizeof(msg), "wrong size %ux%u: needs 600x400 or 400x600", (unsigned)w, (unsigned)h);
    return fail(o, 400, msg);
  }
  if (!validId(wanted)) return fail(o, 400, "bad name (letters, digits, _ and - only, 40 at most)");

  char id[ID_MAX + 1];
  int existing = findNoCase(l, wanted);
  if (existing >= 0 && replace) snprintf(id, sizeof(id), "%s", l.items[existing].id);        // keep the capitals it has
  else {
    existing = -1;
    if (!uniqueId(l, wanted, id)) return fail(o, 409, "that name is taken");
  }
  const bool replacing = existing >= 0;
  if (!replacing && l.n >= MAX_ITEMS) return fail(o, 409, "the card already holds 120 pictures");

  const Part& th = u.parts[PART_THUMB];
  uint16_t tw = 0, tl = 0;
  bool haveThumb = th.seen && th.done && th.headLen >= sizeof(th.head) && parsePngHeader(th.head, tw, tl);   // a bad thumbnail is simply left out

  char to[80];
  pathFor(DIR_THUMBS, id, "png", to, sizeof(to));
  SD.remove(to);                                          // an old (or orphaned) thumbnail does not belong to the new picture
  if (haveThumb && !SD.rename(tempPath(PART_THUMB), to)) haveThumb = false;
  pathFor(DIR_IMAGES, id, "png", to, sizeof(to));
  if (replacing) SD.remove(to);
  if (!SD.rename(tempPath(PART_PNG), to)) {
    if (haveThumb) { char t[80]; pathFor(DIR_THUMBS, id, "png", t, sizeof(t)); SD.remove(t); }
    return fail(o, 500, "can't store the picture on the card");
  }

  Item* it;
  if (replacing) it = &l.items[existing];
  else { it = &l.items[l.n++]; memset(it, 0, sizeof(*it)); snprintf(it->id, sizeof(it->id), "%s", id); it->on = true; it->master = masterOnCard(id); }
  it->ok = true; it->w = w; it->h = h; it->bytes = (uint32_t)png.bytes; it->thumb = haveThumb;
  if (!savePlaylist(l)) rescan();                         // the picture is on the card: the next scan lists it anyway

  snprintf(o.id, sizeof(o.id), "%s", id);
  o.code = 200;
  o.message[0] = 0;
  uploadReset();
  return true;
}

// The request carried the original (field "master": a JPEG, PNG or TIFF) for a picture that is already on the card.
inline bool commitMaster(const char* id, Outcome& o) {
  Upload& u = upload();
  if (!ensureLoaded()) return fail(o, 409, "no card");
  Library& l = lib();
  if (!validId(id)) return fail(o, 400, "bad name");
  int idx = find(l, id);
  if (idx < 0) return fail(o, 404, "no such picture");
  const Part& m = u.parts[PART_MASTER];
  if (!m.seen)    return fail(o, 400, "no original in the upload");
  if (m.tooBig)   return fail(o, 413, "the original is too big (32 MB at most)");
  if (m.ioError)  return fail(o, 500, "can't write to the card");
  if (!m.done)    return fail(o, 400, "the original did not arrive completely");
  const uint8_t kind = masterKindOfBytes(m.head, m.headLen);
  if (kind == 0) return fail(o, 400, "that is not a JPEG, PNG or TIFF file");

  char path[80];
  for (uint8_t c = 1; c < MASTER_KINDS; c++) { pathFor(DIR_MASTERS, l.items[idx].id, masterExt(c), path, sizeof(path)); SD.remove(path); }     // only ONE original per picture
  pathFor(DIR_MASTERS, l.items[idx].id, masterExt(kind), path, sizeof(path));
  if (!SD.rename(tempPath(PART_MASTER), path)) return fail(o, 500, "can't store the original on the card");
  l.items[idx].master = kind;

  snprintf(o.id, sizeof(o.id), "%s", l.items[idx].id);
  o.code = 200;
  o.message[0] = 0;
  uploadReset();
  return true;
}

}  // namespace gallery
