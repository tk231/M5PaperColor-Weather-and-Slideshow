// web.h - the web page served while the "Connect to Device" page is up.
//
// Uses the built-in WebServer: it runs inside loop() (call web::loop() often), so
// everything it does (RTC, I2C, LED, settings) happens on the same task as the rest of
// the sketch and M5Unified never has to be thread-safe.
//
// Endpoints (parameters are in the query string, even for POST):
//   GET  /                the page (index_html.h)
//   GET  /api/status      LED, clock, battery, microSD, idle countdown, profile   (passive*)
//   GET  /api/settings    home Wi-Fi name (never the password), location, fetch interval  (passive*)
//   GET  /api/wifi/scan   networks found when the Connect page was opened (passive*)
//   POST /api/led         on=0|1 r g b (0-255) br (0-255)                -> status
//   POST /api/time        epochMs offsetMin tz                            -> status
//   POST /api/sdtest      writes / reads / deletes a 32 KB file on the microSD card   -> test result
//   GET  /api/images              the picture list (also says if there is no card / it can't be read)
//   GET  /api/images/file         id=<id> kind=image|thumb|master      streams the file
//   POST /api/images/enable       id=<id> on=0|1                       -> list
//   POST /api/images/order        ids=<id>,<id>,...                    -> list   (those first, in that order)
//   POST /api/images/delete       id=<id>   removes picture, thumbnail and original   -> list
//   POST /api/images/rescan       reads the card again                 -> list
//   POST /api/images/upload       multipart: png (+ thumb); ?id=<name>[&replace=1]   -> list + "added"
//   POST /api/images/master       multipart: master (JPEG, PNG or TIFF); ?id=<name>  -> list + "added"
//   POST /api/settings    any of: lat lon place | fetchMin | slideMin | shuffle=0|1 | portrait=0|1|2 | ledStatus=0|1 | ledPct=5..100 | ssid [pass] | forgetWifi=1   -> settings
// (*) "passive" = does not count as activity for the idle timeout.
#pragma once
#include <WebServer.h>
#include <M5Unified.h>
#include "sensors.h"
#include "settings.h"
#include "statusled.h"
#include "sdcard.h"
#include "gallery.h"
#include "wifi_scan.h"
#include "index_html.h"

namespace web {

// Everything the profile (test / production) decides, handed over once in begin().
struct Config {
  const char*     profileName      = "";
  uint32_t        idleTimeoutMs    = 600000;
  const uint16_t* fetchOptionsMin  = nullptr;     // intervals offered on the settings page
  const uint16_t* slideOptionsMin  = nullptr;     // slideshow intervals offered on the settings page
  uint8_t         slideOptionCount = 0;
  uint8_t         fetchOptionCount = 0;
  void          (*onSettingsChanged)() = nullptr; // called after the settings were saved
};

struct LedState {
  bool    on         = false;
  uint8_t r = 0, g = 0, b = 255;
  uint8_t brightness = 64;
};

inline WebServer&  server()        { static WebServer s(80); return s; }
inline Config&     cfg()           { static Config c; return c; }
inline LedState&   led()           { static LedState l; return l; }
inline uint32_t&   lastActivity()  { static uint32_t t = 0; return t; }
inline bool&       registered()    { static bool b = false; return b; }

// Something the user did: restarts the idle countdown.
inline void touch() { lastActivity() = millis(); }

// ---- LED ---------------------------------------------------------------------
// The colour of the LED tab is the MANUAL colour of the status light (statusled.h): while it is on it wins over every status.
inline void applyLed() {
  sled::Inputs& in = sled::in();
  const LedState& l = led();
  in.manualOn = l.on;
  in.mr = l.r; in.mg = l.g; in.mb = l.b; in.mbr = l.brightness;
  sled::tick(millis());                                 // at once (the task would do it within 40 ms)
}

inline void ledOff() {
  led().on = false;
  applyLed();
}

// ---- helpers -------------------------------------------------------------------
// Escape a string for use inside a JSON string literal ("  \  and control characters).
inline void jsonEscape(const char* in, char* out, size_t cap) {
  size_t o = 0;
  for (const unsigned char* p = (const unsigned char*)in; *p && o + 2 < cap; p++) {
    unsigned char c = *p;
    if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
    else if (c < 0x20)         { out[o++] = '?'; }
    else                       { out[o++] = (char)c; }
  }
  out[o] = 0;
}

inline int clampInt(const String& s, int lo, int hi, int fallback) {
  if (s.length() == 0) return fallback;
  long v = s.toInt();
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return (int)v;
}

// Strict number parsing: the whole text must be a number ("abc", "12x" and "" are refused).
inline bool parseFloatStrict(const String& text, float& out) {
  const char* c = text.c_str();
  char* end = nullptr;
  float v = strtof(c, &end);
  if (end == c) return false;
  while (*end == ' ') end++;
  if (*end != 0) return false;
  out = v;
  return true;
}

// A whole number of 1 to 5 digits and nothing else ("5abc", "1e1", "-1", " 5", "" are refused; toInt() would read "5abc" as 5).
inline bool parseMinutesStrict(const String& text, int& out) {
  const char* c = text.c_str();
  size_t n = strlen(c);
  if (n < 1 || n > 5) return false;
  int v = 0;
  for (size_t i = 0; i < n; i++) { if (c[i] < '0' || c[i] > '9') return false; v = v * 10 + (c[i] - '0'); }
  out = v;
  return true;
}

// ---- JSON: status ------------------------------------------------------------------
inline void sendStatus() {
  const LedState& l = led();

  sensors::LocalTime lt;
  bool timeOk = sensors::readLocalTime(lt);
  char local[48] = "";                                  // "YYYY-MM-DD HH:MM:SS" (roomy enough for any int the compiler could imagine)
  if (timeOk) {
    snprintf(local, sizeof(local), "%04d-%02d-%02d %02d:%02d:%02d",
             lt.year, lt.month, lt.day, lt.hour, lt.minute, lt.second);
  }

  PowerStatus p = sensors::readPower();
  int pct = p.batOk ? batteryPercent(p.batMv) : 0;

  sdcard::refresh();                                   // a card put in / taken out is noticed here too
  const sdcard::Info& sd = sdcard::info();
  char sdErr[200];
  jsonEscape(sd.error, sdErr, sizeof(sdErr));
  const char* sdState = sd.state == SD_OK ? "ok" : sd.state == SD_ERROR ? "error" : "absent";

  uint32_t elapsed = millis() - lastActivity();
  uint32_t remainingSec = elapsed >= cfg().idleTimeoutMs ? 0 : (cfg().idleTimeoutMs - elapsed) / 1000;

  char buf[1100];
  snprintf(buf, sizeof(buf),
    "{\"profile\":\"%s\","
    "\"led\":{\"on\":%d,\"r\":%u,\"g\":%u,\"b\":%u,\"br\":%u},"
    "\"time\":{\"ok\":%d,\"epoch\":%lu,\"offsetMin\":%d,\"tz\":\"%s\",\"local\":\"%s\"},"
    "\"battery\":{\"ok\":%d,\"mv\":%u,\"pct\":%d,\"charging\":%d},"
    "\"sd\":{\"state\":\"%s\",\"mb\":%lu,\"type\":\"%s\",\"error\":\"%s\"},"
    "\"idle\":{\"timeoutMin\":%lu,\"remainingSec\":%lu}}",
    cfg().profileName,
    l.on ? 1 : 0, l.r, l.g, l.b, l.brightness,
    timeOk ? 1 : 0, (unsigned long)lt.epochUtc, sensors::tzOffsetMin(), sensors::tzName(), local,
    p.batOk ? 1 : 0, p.batMv, pct, p.charging ? 1 : 0,
    sdState, (unsigned long)(sd.cardBytes / (1024ULL * 1024ULL)), sd.type, sdErr,
    (unsigned long)(cfg().idleTimeoutMs / 60000UL), (unsigned long)remainingSec);

  server().sendHeader("Cache-Control", "no-store");
  server().send(200, "application/json", buf);
}

// ---- JSON: settings (the Wi-Fi password is NEVER included) ------------------------------
inline void sendSettings() {
  const settings::Data& d = settings::data();
  char ssid[80], place[96], opts[64] = "", sopts[64] = "";

  jsonEscape(d.ssid, ssid, sizeof(ssid));
  jsonEscape(d.place, place, sizeof(place));
  size_t o = 0;
  for (uint8_t i = 0; i < cfg().fetchOptionCount && o + 8 < sizeof(opts); i++) {
    o += snprintf(opts + o, sizeof(opts) - o, "%s%u", i ? "," : "", (unsigned)cfg().fetchOptionsMin[i]);
  }

  size_t so = 0;
  for (uint8_t i = 0; i < cfg().slideOptionCount && so + 8 < sizeof(sopts); i++) {
    so += snprintf(sopts + so, sizeof(sopts) - so, "%s%u", i ? "," : "", (unsigned)cfg().slideOptionsMin[i]);
  }

  char buf[704];
  snprintf(buf, sizeof(buf),
    "{\"wifi\":{\"ssid\":\"%s\",\"passSet\":%d},"
    "\"location\":{\"set\":%d,\"lat\":%.5f,\"lon\":%.5f,\"place\":\"%s\"},"
    "\"fetch\":{\"min\":%u,\"options\":[%s]},"
    "\"slide\":{\"min\":%u,\"options\":[%s],\"shuffle\":%d,\"portrait\":%u},"
    "\"statusLed\":{\"on\":%d,\"pct\":%u}}",
    ssid, d.pass[0] ? 1 : 0,
    d.locationSet ? 1 : 0, (double)d.lat, (double)d.lon, place,
    (unsigned)d.fetchMin, opts, (unsigned)d.slideMin, sopts, d.shuffle ? 1 : 0, (unsigned)d.portrait,
    d.ledStatus ? 1 : 0, (unsigned)d.ledPct);

  server().sendHeader("Cache-Control", "no-store");
  server().send(200, "application/json", buf);
}

// ---- JSON: Wi-Fi scan result --------------------------------------------------------------
inline void sendWifiScan() {
  const wifiscan::Result& r = wifiscan::result();
  uint32_t ageSec = r.done ? (millis() - r.atMs) / 1000 : 0;

  String out;
  out.reserve(2048);
  char head[64];
  snprintf(head, sizeof(head), "{\"done\":%d,\"ageSec\":%lu,\"networks\":[", r.done ? 1 : 0, (unsigned long)ageSec);
  out += head;
  for (int i = 0; i < r.n; i++) {
    char ssid[80], item[160];
    jsonEscape(r.e[i].ssid, ssid, sizeof(ssid));
    snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%d,\"ch\":%u}",
             i ? "," : "", ssid, (int)r.e[i].rssi, r.e[i].open ? 1 : 0, (unsigned)r.e[i].channel);
    out += item;
  }
  out += "]}";

  server().sendHeader("Cache-Control", "no-store");
  server().send(200, "application/json", out);
}

// ---- handlers --------------------------------------------------------------------------
inline void handleRoot() {
  touch();
  server().sendHeader("Cache-Control", "no-store");
  server().send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

inline void handleLed() {
  touch();
  LedState& l = led();
  l.on         = clampInt(server().arg("on"), 0, 1, l.on ? 1 : 0) != 0;
  l.r          = (uint8_t)clampInt(server().arg("r"),  0, 255, l.r);
  l.g          = (uint8_t)clampInt(server().arg("g"),  0, 255, l.g);
  l.b          = (uint8_t)clampInt(server().arg("b"),  0, 255, l.b);
  l.brightness = (uint8_t)clampInt(server().arg("br"), 0, 255, l.brightness);
  applyLed();
  Serial.printf("LED: %s rgb(%u,%u,%u) brightness %u\n", l.on ? "on" : "off", l.r, l.g, l.b, l.brightness);
  sendStatus();
}

inline void handleTime() {
  touch();
  // epochMs is the phone's clock in milliseconds since 1970 (UTC); offsetMin is minutes east of UTC.
  long long epochMs = atoll(server().arg("epochMs").c_str());
  int offsetMin = clampInt(server().arg("offsetMin"), -14 * 60, 14 * 60, 0);

  // sanity: between 2024-01-01 and 2100-01-01
  if (epochMs < 1704067200000LL || epochMs > 4102444800000LL) {
    server().send(400, "text/plain", "bad time");
    return;
  }

  // keep only harmless characters of the IANA time zone name (e.g. "Europe/Amsterdam")
  String tzIn = server().arg("tz");
  char tz[48];
  size_t n = 0;
  for (size_t i = 0; i < tzIn.length() && n < sizeof(tz) - 1; i++) {
    char c = tzIn[i];
    if (isalnum((unsigned char)c) || c == '/' || c == '_' || c == '+' || c == '-') tz[n++] = c;
  }
  tz[n] = 0;

  sensors::setRtcFromEpoch(epochMs / 1000);
  sensors::saveTz(offsetMin, tz);
  Serial.printf("Time synced from phone: epoch %lu, offset %+d min, zone \"%s\"\n",
                (unsigned long)(epochMs / 1000), offsetMin, tz);
  sendStatus();
}

// Card self-test: write, read back, compare, delete. Deliberately NOT passive: the user asked for it.
inline void handleSdTest() {
  touch();
  sdcard::TestResult t = sdcard::selfTest();
  char err[200];
  jsonEscape(t.error, err, sizeof(err));
  char buf[520];
  snprintf(buf, sizeof(buf),
    "{\"ok\":%d,\"detected\":%d,\"mounted\":%d,\"mb\":%lu,\"type\":\"%s\",\"writeKBps\":%lu,\"readKBps\":%lu,\"error\":\"%s\"}",
    t.ok ? 1 : 0, t.detected ? 1 : 0, t.mounted ? 1 : 0, (unsigned long)t.cardMB, t.type,
    (unsigned long)t.writeKBps, (unsigned long)t.readKBps, err);
  server().sendHeader("Cache-Control", "no-store");
  server().send(200, "application/json", buf);
}

// Several groups of settings can arrive in one request; everything is validated first and
// nothing is changed unless ALL the given groups are valid.
inline void handleSettingsPost() {
  touch();
  WebServer& s = server();

  bool haveLoc = s.hasArg("lat") && s.hasArg("lon");
  float lat = 0, lon = 0;
  if (haveLoc && !(parseFloatStrict(s.arg("lat"), lat) && parseFloatStrict(s.arg("lon"), lon) &&
                   settings::validLatLon(lat, lon))) {
    s.send(400, "text/plain", "bad location");
    return;
  }

  bool haveFetch = s.hasArg("fetchMin");
  int fetchMin = 0;
  if (haveFetch && !(parseMinutesStrict(s.arg("fetchMin"), fetchMin) && fetchMin > 0 && fetchMin < 65536 &&
      settings::inOptions((uint16_t)fetchMin, cfg().fetchOptionsMin, cfg().fetchOptionCount))) {
    s.send(400, "text/plain", "bad interval");
    return;
  }

  bool haveSlide = s.hasArg("slideMin");
  int slideMin = 0;
  if (haveSlide && !(parseMinutesStrict(s.arg("slideMin"), slideMin) && slideMin > 0 && slideMin < 65536 &&
      settings::inOptions((uint16_t)slideMin, cfg().slideOptionsMin, cfg().slideOptionCount))) {
    s.send(400, "text/plain", "bad slideshow interval");
    return;
  }
  bool haveShuffle = s.hasArg("shuffle");
  if (haveShuffle && s.arg("shuffle") != "0" && s.arg("shuffle") != "1") {
    s.send(400, "text/plain", "bad shuffle");
    return;
  }

  bool havePortrait = s.hasArg("portrait");
  if (havePortrait && s.arg("portrait") != "0" && s.arg("portrait") != "1" && s.arg("portrait") != "2") {
    s.send(400, "text/plain", "bad portrait mode");
    return;
  }

  bool haveLedStatus = s.hasArg("ledStatus");
  if (haveLedStatus && s.arg("ledStatus") != "0" && s.arg("ledStatus") != "1") {
    s.send(400, "text/plain", "bad status light");
    return;
  }
  bool haveLedPct = s.hasArg("ledPct");
  int ledPct = 0;
  if (haveLedPct && !(parseMinutesStrict(s.arg("ledPct"), ledPct) && ledPct >= 5 && ledPct <= 100)) {
    s.send(400, "text/plain", "bad brightness");
    return;
  }

  bool forget = s.hasArg("forgetWifi");
  bool haveWifi = !forget && s.hasArg("ssid");
  String ssid = haveWifi ? s.arg("ssid") : String();
  bool havePass = haveWifi && s.hasArg("pass");
  String pass = havePass ? s.arg("pass") : String();
  if (haveWifi && !(settings::validSsid(ssid.c_str()) && (!havePass || settings::validPass(pass.c_str())))) {
    s.send(400, "text/plain", "bad wifi");
    return;
  }

  // ---- all valid: apply ----
  if (haveLoc)   settings::setLocation(lat, lon, s.arg("place").c_str());
  if (haveFetch) settings::setFetchMin((uint16_t)fetchMin, cfg().fetchOptionsMin, cfg().fetchOptionCount);
  if (haveSlide) settings::setSlideMin((uint16_t)slideMin, cfg().slideOptionsMin, cfg().slideOptionCount);
  if (haveShuffle) settings::setShuffle(s.arg("shuffle") == "1");
  if (havePortrait) settings::setPortrait((uint8_t)(s.arg("portrait")[0] - '0'));
  if (haveLedStatus) settings::setLedStatus(s.arg("ledStatus") == "1");
  if (haveLedPct) settings::setLedPct(ledPct);
  if (forget)    settings::forgetWifi();
  if (haveWifi)  settings::setWifi(ssid.c_str(), havePass ? pass.c_str() : nullptr);
  settings::save();

  const settings::Data& d = settings::data();     // (the password is deliberately not logged)
  Serial.printf("Settings saved: location %s (%.4f, %.4f), fetch every %u min, slideshow every %u min%s, portrait mode %u, Wi-Fi \"%s\"%s\n",
                d.locationSet ? d.place : "not set", (double)d.lat, (double)d.lon, (unsigned)d.fetchMin,
                (unsigned)d.slideMin, d.shuffle ? " (shuffled)" : "", (unsigned)d.portrait, d.ssid, d.pass[0] ? " (password set)" : "");
  // only the weather / Wi-Fi settings make the device fetch the weather again: changing the slideshow must not
  if (cfg().onSettingsChanged && (haveLoc || haveFetch || forget || haveWifi)) cfg().onSettingsChanged();
  sendSettings();
}

// deliberately no touch(): the page reads these when it opens or when a tab is shown, and
// reading must not keep the Wi-Fi alive
inline void handleStatus()    { sendStatus(); }
inline void handleSettings()  { sendSettings(); }
inline void handleWifiScan()  { sendWifiScan(); }

// ---- pictures (see gallery.h) -----------------------------------------------------------
// All of these are real user actions (browsing / editing the pictures), so they count as activity.
inline void sendGalleryList(const char* added = nullptr) {
  bool ok = gallery::ensureLoaded();
  const sdcard::Info& sd = sdcard::info();
  char err[200];
  jsonEscape(sd.error, err, sizeof(err));
  const char* state = ok ? "ok" : (sd.state == SD_ERROR ? "error" : "absent");
  String out;
  gallery::libraryJson(gallery::lib(), state, ok ? "" : err, out);
  if (added) {                                          // tell the page which picture the upload produced
    out.remove(out.length() - 1);
    out += ",\"added\":\"";
    out += added;                                       // ids are plain [A-Za-z0-9_-]
    out += "\"}";
  }
  server().sendHeader("Cache-Control", "no-store");
  server().send(200, "application/json", out);
}

inline bool galleryReady() {                            // the actions need a card
  if (gallery::ensureLoaded()) return true;
  server().send(409, "text/plain", "no card");
  return false;
}

// Reads the "id" argument: safe characters only (so it can never reach outside the picture folders) and it must exist.
inline bool galleryPickId(int& idx) {
  String id = server().arg("id");
  if (!gallery::validId(id.c_str())) { server().send(400, "text/plain", "bad id"); return false; }
  idx = gallery::find(gallery::lib(), id.c_str());
  if (idx < 0) { server().send(404, "text/plain", "no such picture"); return false; }
  return true;
}

inline void handleImages() { touch(); sendGalleryList(); }

inline void handleImagesRescan() { touch(); gallery::rescan(); sendGalleryList(); }

inline void handleImageFile() {
  touch();
  if (!galleryReady()) return;
  int idx;
  if (!galleryPickId(idx)) return;
  const gallery::Item& it = gallery::lib().items[idx];

  String kind = server().arg("kind");
  if (kind.length() == 0) kind = "image";
  char path[80];
  const char* type = "image/png";
  if (kind == "image") {
    gallery::pathFor(gallery::DIR_IMAGES, it.id, "png", path, sizeof(path));
  } else if (kind == "thumb") {
    if (!it.thumb) { server().send(404, "text/plain", "no thumbnail"); return; }
    gallery::pathFor(gallery::DIR_THUMBS, it.id, "png", path, sizeof(path));
  } else if (kind == "master") {
    if (!it.master) { server().send(404, "text/plain", "no original"); return; }
    gallery::pathFor(gallery::DIR_MASTERS, it.id, gallery::masterExt(it.master), path, sizeof(path));
    type = gallery::masterType(it.master);
  } else {
    server().send(400, "text/plain", "bad kind");
    return;
  }

  File f = SD.open(path);
  if (!f) { server().send(404, "text/plain", "file missing"); return; }
  server().sendHeader("Cache-Control", "private, max-age=600");   // the page adds ?v=<size> so a changed file is fetched again
  server().streamFile(f, type);
  f.close();
}

inline void handleImageEnable() {
  touch();
  if (!galleryReady()) return;
  int idx;
  if (!galleryPickId(idx)) return;
  String on = server().arg("on");
  if (on != "0" && on != "1") { server().send(400, "text/plain", "bad on"); return; }
  gallery::lib().items[idx].on = (on == "1");
  if (!gallery::savePlaylist(gallery::lib())) {
    gallery::rescan();                                  // back to what is really on the card
    server().send(500, "text/plain", "can't save the playlist");
    return;
  }
  sendGalleryList();
}

inline void handleImageOrder() {
  touch();
  if (!galleryReady()) return;
  if (!gallery::setOrder(gallery::lib(), server().arg("ids").c_str())) { server().send(400, "text/plain", "bad order"); return; }
  if (!gallery::savePlaylist(gallery::lib())) {
    gallery::rescan();
    server().send(500, "text/plain", "can't save the playlist");
    return;
  }
  sendGalleryList();
}

inline void handleImageDelete() {
  touch();
  if (!galleryReady()) return;
  int idx;
  if (!galleryPickId(idx)) return;
  char id[gallery::ID_MAX + 1];
  snprintf(id, sizeof(id), "%s", gallery::lib().items[idx].id);
  if (!gallery::removeFiles(id)) { server().send(500, "text/plain", "can't delete"); return; }
  gallery::removeAt(gallery::lib(), idx);
  if (!gallery::savePlaylist(gallery::lib())) {
    gallery::rescan();
    server().send(500, "text/plain", "can't save the playlist");
    return;
  }
  Serial.printf("Gallery: deleted %s\n", id);
  sendGalleryList();
}

// ---- uploads -------------------------------------------------------------------------------
// The server calls the first function for every file part (START, WRITE..., END, or ABORTED when the phone
// goes away) and the second one when the whole request has arrived. See gallery.h.
inline void handleUploadData() {
  HTTPUpload& up = server().upload();
  if (up.status != UPLOAD_FILE_ABORTED) sled::eventUpload(millis());     // the status light blinks blue while data arrives
  if (up.status == UPLOAD_FILE_START) {
    touch();
    gallery::uploadBegin(up.name.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    touch();                                            // a long upload must not run into the idle timeout
    gallery::uploadWrite(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    gallery::uploadEnd();
    touch();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    gallery::uploadAbort();
  }
}

inline void handleImageUploadDone() {
  touch();
  gallery::Outcome o;
  if (!gallery::commitPicture(server().arg("id").c_str(), server().arg("replace") == "1", o)) { server().send(o.code, "text/plain", o.message); return; }
  Serial.printf("Gallery: added %s\n", o.id);
  sendGalleryList(o.id);
}

inline void handleImageMasterDone() {
  touch();
  gallery::Outcome o;
  if (!gallery::commitMaster(server().arg("id").c_str(), o)) { server().send(o.code, "text/plain", o.message); return; }
  Serial.printf("Gallery: original stored for %s\n", o.id);
  sendGalleryList(o.id);
}

inline void handleNotFound() {
  server().send(404, "text/plain", "Not found");
}

// ---- lifecycle ---------------------------------------------------------------------------
inline void begin(const Config& config) {
  cfg() = config;
  if (!registered()) {
    server().on("/",              HTTP_GET,  handleRoot);
    server().on("/api/status",    HTTP_GET,  handleStatus);
    server().on("/api/settings",  HTTP_GET,  handleSettings);
    server().on("/api/wifi/scan", HTTP_GET,  handleWifiScan);
    server().on("/api/led",       HTTP_POST, handleLed);
    server().on("/api/time",      HTTP_POST, handleTime);
    server().on("/api/sdtest",    HTTP_POST, handleSdTest);
    server().on("/api/images",         HTTP_GET,  handleImages);
    server().on("/api/images/file",    HTTP_GET,  handleImageFile);
    server().on("/api/images/enable",  HTTP_POST, handleImageEnable);
    server().on("/api/images/order",   HTTP_POST, handleImageOrder);
    server().on("/api/images/delete",  HTTP_POST, handleImageDelete);
    server().on("/api/images/rescan",  HTTP_POST, handleImagesRescan);
    server().on("/api/images/upload",  HTTP_POST, handleImageUploadDone, handleUploadData);
    server().on("/api/images/master",  HTTP_POST, handleImageMasterDone, handleUploadData);
    server().on("/api/settings",  HTTP_POST, handleSettingsPost);
    server().onNotFound(handleNotFound);
    registered() = true;
  }
  server().begin();
  touch();
  Serial.println("Web server started");
}

inline void end() {
  server().stop();
  ledOff();                   // never leave the LED on after leaving the page
  Serial.println("Web server stopped");
}

inline void loop() { server().handleClient(); }

}  // namespace web
