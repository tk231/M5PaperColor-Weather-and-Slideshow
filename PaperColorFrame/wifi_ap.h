// wifi_ap.h - the device's own Wi-Fi access point ("Connect to Device" page)
// Only the AP is used here; the device never runs AP and station mode together.
#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <esp_system.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#endif
#include "types.h"
#include "radio.h"

namespace ap {

// Escape the characters that have a special meaning inside a Wi-Fi QR code.
inline String escapeField(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') o += '\\';
    o += c;
  }
  return o;
}

// Password setting:
//   ""      -> open network (no password)
//   "auto"  -> random 10-character password, generated once on first use and kept in flash
//   other   -> used as given (WPA2 needs at least 8 characters)
inline String resolvePassword(const char* setting) {
  String s(setting);
  if (s.length() == 0) return "";
  if (s != "auto") return s;

  Preferences prefs;
  prefs.begin("frame", false);
  String pw = prefs.getString("appass", "");
  if (pw.length() < 8) {
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";   // no look-alikes (i, l, o, 0, 1)
    pw = "";
    for (int i = 0; i < 10; i++) pw += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    prefs.putString("appass", pw);
  }
  prefs.end();
  return pw;
}

// Start the access point and fill `ci` (ci.ok == false if it could not start).
inline bool start(const char* ssid, const String& password, ConnectInfo& ci) {
  ci = ConnectInfo();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(ssid, password.length() ? password.c_str() : nullptr, 1, 0, 4);
  ci.ok = ok;

  snprintf(ci.ssid, sizeof(ci.ssid), "%s", ssid);
  snprintf(ci.password, sizeof(ci.password), "%s", password.c_str());
  snprintf(ci.url, sizeof(ci.url), "http://%s", WiFi.softAPIP().toString().c_str());

  String qr = "WIFI:";
  if (password.length()) qr += "T:WPA;S:" + escapeField(ssid) + ";P:" + escapeField(password) + ";;";
  else                   qr += "T:nopass;S:" + escapeField(ssid) + ";;";
  snprintf(ci.wifiQr, sizeof(ci.wifiQr), "%s", qr.c_str());

  Serial.printf("AP %s: ssid \"%s\", %s, url %s\n", ok ? "started" : "FAILED to start", ssid,
                password.length() ? "WPA2" : "open", ci.url);
  return ok;
}

inline void stop() {
  WiFi.softAPdisconnect(true);
  radio::off();                // waits until the radio has really stopped: whatever comes next (the weather fetch) starts it again
  Serial.println("AP stopped");
}

inline int stationCount() { return WiFi.softAPgetStationNum(); }

}  // namespace ap
