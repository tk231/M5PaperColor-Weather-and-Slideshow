// settings.h - what the user configures on the web page, kept in flash (NVS).
//   home Wi-Fi (name + password), location (lat / lon / label), weather fetch interval.
// The Wi-Fi password is write-only: nothing in the firmware ever sends it back to the page.
// NOTE: NVS is not encrypted, so the password is stored as plain text in the device's flash.
#pragma once
#include <Preferences.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

namespace settings {

struct Data {
  char     ssid[33]     = "";      // home Wi-Fi name (max 32 bytes)
  char     pass[64]     = "";      // home Wi-Fi password (max 63 chars), empty = open network
  bool     locationSet  = false;
  float    lat          = 0;
  float    lon          = 0;
  char     place[40]    = "";      // label shown on the Weather page (printable ASCII only)
  uint16_t fetchMin     = 0;       // weather fetch interval in minutes
  uint16_t slideMin     = 0;       // slideshow: a new picture every this many minutes
  bool     shuffle      = false;   // slideshow: random order instead of the order of the playlist
  uint8_t  portrait     = 0;       // slideshow: portrait pictures: 0 = fill the screen (turn the frame clockwise), 1 = same, counter-clockwise, 2 = keep upright, smaller
  bool     ledStatus    = true;    // the status light (RGB LED) shows what the device is doing
  uint8_t  ledPct       = 15;      // ... at this brightness, in percent (5..100)
};

inline Data& data() { static Data d; return d; }

// Keep printable ASCII only (the e-paper fonts have no other glyphs), collapse runs of
// spaces and trim. Writes at most outSize-1 characters.
inline void asciiOnly(const char* in, char* out, size_t outSize) {
  size_t o = 0;
  bool lastSpace = true;                       // also trims leading spaces
  for (const unsigned char* p = (const unsigned char*)in; *p && o + 1 < outSize; p++) {
    unsigned char c = *p;
    if (c == ' ' || c == '\t') {
      if (!lastSpace) { out[o++] = ' '; lastSpace = true; }
    } else if (c > 0x20 && c < 0x7F) {
      out[o++] = (char)c;
      lastSpace = false;
    }
  }
  while (o > 0 && out[o - 1] == ' ') o--;      // trailing space
  out[o] = 0;
}

inline bool inOptions(uint16_t v, const uint16_t* options, size_t n) {
  for (size_t i = 0; i < n; i++) if (options[i] == v) return true;
  return false;
}

// ---- persistence -------------------------------------------------------------------
inline void save() {
  const Data& d = data();
  Preferences p;
  p.begin("frame", false);
  p.putString("wssid", d.ssid);
  p.putString("wpass", d.pass);
  p.putBool("locset", d.locationSet);
  p.putFloat("lat", d.lat);
  p.putFloat("lon", d.lon);
  p.putString("place", d.place);
  p.putUShort("fetchmin", d.fetchMin);
  p.putUShort("slidemin", d.slideMin);
  p.putBool("shuffle", d.shuffle);
  p.putUChar("portrait", d.portrait);
  p.putBool("ledstat", d.ledStatus);
  p.putUChar("ledpct", d.ledPct);
  p.end();
}

// Loads everything. If the stored fetch interval is not one of the current profile's
// options (e.g. after switching between the test and production profile), the
// profile's default is used instead.
inline void load(uint16_t fetchDefaultMin, const uint16_t* options, size_t nOptions) {
  Data& d = data();
  d = Data();
  Preferences p;
  p.begin("frame", false);
  String s;
  s = p.getString("wssid", "");   snprintf(d.ssid,  sizeof(d.ssid),  "%s", s.c_str());
  s = p.getString("wpass", "");   snprintf(d.pass,  sizeof(d.pass),  "%s", s.c_str());
  s = p.getString("place", "");   snprintf(d.place, sizeof(d.place), "%s", s.c_str());
  d.locationSet = p.getBool("locset", false);
  d.lat = p.getFloat("lat", 0);
  d.lon = p.getFloat("lon", 0);
  d.fetchMin = p.getUShort("fetchmin", 0);
  d.slideMin = p.getUShort("slidemin", 0);
  d.shuffle = p.getBool("shuffle", false);
  d.portrait = p.getUChar("portrait", 0);
  d.ledStatus = p.getBool("ledstat", true);
  d.ledPct = p.getUChar("ledpct", 15);
  p.end();
  if (d.portrait > 2) d.portrait = 0;              // (a damaged value in flash)
  if (d.ledPct < 5 || d.ledPct > 100) d.ledPct = 15;

  if (!inOptions(d.fetchMin, options, nOptions)) d.fetchMin = fetchDefaultMin;
  if (d.locationSet && !(isfinite(d.lat) && isfinite(d.lon))) d.locationSet = false;
}

// ---- validated setters (return false and change nothing on bad input) -------------
inline bool validLatLon(float lat, float lon) {
  return isfinite(lat) && isfinite(lon) && lat >= -90.0f && lat <= 90.0f && lon >= -180.0f && lon <= 180.0f;
}

inline bool setLocation(float lat, float lon, const char* place) {
  if (!validLatLon(lat, lon)) return false;
  Data& d = data();
  d.lat = lat;
  d.lon = lon;
  d.locationSet = true;
  asciiOnly(place ? place : "", d.place, sizeof(d.place));
  if (d.place[0] == 0) snprintf(d.place, sizeof(d.place), "%.2f, %.2f", (double)lat, (double)lon);
  return true;
}

inline bool validSsid(const char* ssid) {
  size_t n = ssid ? strlen(ssid) : 0;
  return n >= 1 && n <= 32;
}

inline bool validPass(const char* pass) {
  size_t n = pass ? strlen(pass) : 0;
  return n == 0 || (n >= 8 && n <= 63);        // WPA2: 8..63 characters (or none for an open network)
}

// pass == nullptr keeps the stored password if the network name is unchanged, and clears
// it if the name changed (an old password cannot belong to a different network).
inline bool setWifi(const char* ssid, const char* pass) {
  if (!validSsid(ssid)) return false;
  if (pass && !validPass(pass)) return false;
  Data& d = data();
  bool sameName = strcmp(d.ssid, ssid) == 0;
  snprintf(d.ssid, sizeof(d.ssid), "%s", ssid);
  if (pass)           snprintf(d.pass, sizeof(d.pass), "%s", pass);
  else if (!sameName) d.pass[0] = 0;
  return true;
}

inline void forgetWifi() {
  Data& d = data();
  d.ssid[0] = 0;
  d.pass[0] = 0;
}

inline bool setFetchMin(uint16_t v, const uint16_t* options, size_t n) {
  if (!inOptions(v, options, n)) return false;
  data().fetchMin = v;
  return true;
}

// The slideshow interval must be one of the current profile's options too (otherwise the profile's default is used): call after load().
inline void fixSlideMin(uint16_t defaultMin, const uint16_t* options, size_t n) {
  if (!inOptions(data().slideMin, options, n)) data().slideMin = defaultMin;
}
inline bool setSlideMin(uint16_t v, const uint16_t* options, size_t n) {
  if (!inOptions(v, options, n)) return false;
  data().slideMin = v;
  return true;
}
inline void setShuffle(bool on) { data().shuffle = on; }
inline bool setPortrait(uint8_t v) { if (v > 2) return false; data().portrait = v; return true; }
inline void setLedStatus(bool on) { data().ledStatus = on; }
inline bool setLedPct(int v) { if (v < 5 || v > 100) return false; data().ledPct = (uint8_t)v; return true; }

inline bool wifiConfigured() { return data().ssid[0] != 0; }

}  // namespace settings
