// wifi_scan.h - list of nearby Wi-Fi networks for the settings page.
//
// The device can't scan while its own access point is running (the radio would leave the
// AP's channel and drop the phone), so the scan is done ONCE, just before the AP is
// started when the Connect page is opened, and the result is kept in memory. To get a
// fresh list, leave the Connect page and open it again.
//
// The ESP32 only sees 2.4 GHz networks.
#pragma once
#include <WiFi.h>
#include <string.h>
#include "radio.h"

namespace wifiscan {

struct Entry {
  char    ssid[33];
  int8_t  rssi;
  bool    open;        // no password needed
  uint8_t channel;
};

constexpr int MAX_ENTRIES = 20;   // kept for the page
constexpr int MAX_TMP     = 64;   // looked at while cleaning up the raw result

struct Result {
  Entry    e[MAX_ENTRIES];
  int      n      = 0;
  bool     done   = false;
  uint32_t atMs   = 0;     // millis() when the scan finished
  uint32_t tookMs = 0;
};

inline Result& result() { static Result r; return r; }

// Blocking: takes a few seconds. Switches the radio to station mode, so call it BEFORE
// starting the access point.
inline void run() {
  Result& r = result();
  r.n = 0;
  r.done = false;
  uint32_t t0 = millis();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int16_t found = WiFi.scanNetworks(false, false);     // blocking, hidden networks not listed
  if (found < 0) found = 0;

  static Entry tmp[MAX_TMP];
  int m = 0;
  for (int i = 0; i < found; i++) {
    String name = WiFi.SSID(i);
    if (name.length() == 0 || name.length() > 32) continue;          // hidden / invalid
    int rssi = WiFi.RSSI(i);

    int k = 0;                                                       // same name already listed?
    for (; k < m; k++) if (strcmp(tmp[k].ssid, name.c_str()) == 0) break;
    if (k < m) {
      if (rssi > tmp[k].rssi) {                                      // keep the strongest access point
        tmp[k].rssi = (int8_t)rssi;
        tmp[k].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        tmp[k].channel = (uint8_t)WiFi.channel(i);
      }
    } else if (m < MAX_TMP) {
      snprintf(tmp[m].ssid, sizeof(tmp[m].ssid), "%s", name.c_str());
      tmp[m].rssi = (int8_t)rssi;
      tmp[m].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      tmp[m].channel = (uint8_t)WiFi.channel(i);
      m++;
    }
  }
  WiFi.scanDelete();
  radio::off();                                                      // the scan is over: stop the radio properly before the access point starts it

  for (int a = 1; a < m; a++) {                                      // strongest first (insertion sort)
    Entry x = tmp[a];
    int b = a - 1;
    while (b >= 0 && tmp[b].rssi < x.rssi) { tmp[b + 1] = tmp[b]; b--; }
    tmp[b + 1] = x;
  }

  r.n = m < MAX_ENTRIES ? m : MAX_ENTRIES;
  for (int i = 0; i < r.n; i++) r.e[i] = tmp[i];
  r.done   = true;
  r.atMs   = millis();
  r.tookMs = r.atMs - t0;
  Serial.printf("Wi-Fi scan: %d raw, %d listed, %lu ms\n", (int)found, r.n, (unsigned long)r.tookMs);
}

}  // namespace wifiscan
