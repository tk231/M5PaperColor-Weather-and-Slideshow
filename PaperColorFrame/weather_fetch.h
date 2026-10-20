// weather_fetch.h - one weather update: join the home Wi-Fi, ask Open-Meteo over HTTPS, keep the
// answer in flash, switch Wi-Fi off again. Everything that needs no hardware lives in weather.h.
//
// The Wi-Fi radio is only on for the few seconds this takes, and never while the device's own
// access point (Connect page) is running.
#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "radio.h"
#include "ntp.h"
#include "settings.h"
#include "weather.h"
#include "sensors.h"

namespace wxfetch {

constexpr uint32_t WIFI_TIMEOUT_MS = 15000;   // how long to wait for the home Wi-Fi
constexpr uint16_t HTTP_TIMEOUT_MS = 10000;   // connect / read timeout of the request

// The copy of the forecast the Weather page works from (same bytes as in flash).
inline wx::Cache& cache() { static wx::Cache c; return c; }

// ---- flash ---------------------------------------------------------------------------
inline bool cacheLoad() {
  Preferences p;
  p.begin("frame", false);
  bool ok = false;
  wx::Cache tmp;
  wx::clear(tmp);
  if (p.getBytesLength("wxcache") == sizeof(wx::Cache)) {
    p.getBytes("wxcache", &tmp, sizeof(tmp));
    ok = wx::valid(tmp);
  }
  p.end();
  if (ok) cache() = tmp;
  else    wx::clear(cache());
  return ok;
}

inline void cacheSave(const wx::Cache& c) {
  Preferences p;
  p.begin("frame", false);
  p.putBytes("wxcache", &c, sizeof(c));
  p.end();
}

inline void cacheClear() {
  wx::clear(cache());
  Preferences p;
  p.begin("frame", false);
  p.remove("wxcache");
  p.end();
}

// ---- Wi-Fi ---------------------------------------------------------------------------
// Joins the home Wi-Fi. Returns nullptr on success, otherwise a short reason. The reason comes
// from what the radio reported while it was trying ("not found" and "password rejected" are
// different answers from the ESP32).
inline const char* connectWifi(const settings::Data& s) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(s.ssid, s.pass[0] ? s.pass : nullptr);

  bool sawNoSsid = false, sawFailed = false;
  const uint32_t t0 = millis();
  while (millis() - t0 < WIFI_TIMEOUT_MS) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) return nullptr;
    if (st == WL_NO_SSID_AVAIL)  sawNoSsid = true;
    if (st == WL_CONNECT_FAILED) sawFailed = true;
    delay(100);
  }
  if (sawNoSsid)  return "Wi-Fi network not found";
  if (sawFailed)  return "Wi-Fi password rejected";
  return "Wi-Fi connection timed out";
}

inline void wifiOff() {
  WiFi.disconnect();          // (no arguments: the signature differs between ESP32 core versions)
  radio::off();               // (and wait until it has really stopped: see radio.h)
}

// Why an update failed, which decides what the device does about it:
//   SETUP    the location or the home Wi-Fi is not set up: nothing can be tried until the user sets it up
//   NETWORK  the device could not join the Wi-Fi, or could not reach the server: it is not online. Trying again by itself would only use up the battery,
//            so it waits until the user connects it to a Wi-Fi network (saving the Wi-Fi settings) or asks for an update (G1)
//   SERVER   the server was reached but answered with an error or with something unusable: a later try may well work
enum class Fail : uint8_t { NONE = 0, SETUP, NETWORK, SERVER };

// ---- the request ---------------------------------------------------------------------
// Returns nullptr on success (answer in `body`), otherwise a short reason written to `reason` and the kind of failure in `kind`.
inline const char* httpGet(float lat, float lon, String& body, char* reason, size_t reasonSize, Fail& kind) {
  char url[400];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,weather_code,wind_speed_10m,is_day"
    "&hourly=temperature_2m,weather_code,wind_speed_10m,is_day"
    "&timezone=auto&timeformat=unixtime&forecast_days=3&wind_speed_unit=kmh",
    (double)lat, (double)lon);

  WiFiClientSecure client;
  client.setInsecure();       // no certificate check: only public weather data travels here, and it
                              // works even if the clock is wrong. (See FUTURE_IDEAS.md.)
  HTTPClient http;
  if (!http.begin(client, String(url))) { snprintf(reason, reasonSize, "Can't start the request"); kind = Fail::NETWORK; return reason; }
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  int code = http.GET();
  if (code <= 0) {
    http.end();
    snprintf(reason, reasonSize, "Can't reach Open-Meteo");
    kind = Fail::NETWORK;
    return reason;
  }
  if (code != 200) {
    http.end();
    snprintf(reason, reasonSize, "Open-Meteo error %d", code);
    kind = Fail::SERVER;
    return reason;
  }
  body = http.getString();
  http.end();
  return nullptr;
}

// ---- one complete update ----------------------------------------------------------------
// On success the new forecast is in cache() and in flash. On failure `reason` says why, `*kindOut` (if given) says what kind of failure it was
// (see Fail), and the old forecast is left untouched. Wi-Fi is always switched off again. Blocks for up to ~35 s.
inline bool update(const settings::Data& s, char* reason, size_t reasonSize, Fail* kindOut = nullptr) {
  reason[0] = 0;
  Fail kind = Fail::NONE;
  if (!s.locationSet)     { snprintf(reason, reasonSize, "Location not set");      if (kindOut) *kindOut = Fail::SETUP; return false; }
  if (s.ssid[0] == 0)     { snprintf(reason, reasonSize, "Home Wi-Fi not set up"); if (kindOut) *kindOut = Fail::SETUP; return false; }

  const uint32_t t0 = millis();
  bool ok = false;

  const char* err = connectWifi(s);
  if (err) {
    snprintf(reason, reasonSize, "%s", err);
    kind = Fail::NETWORK;
  } else {
    ntp::syncRtc();                                    // the clock first (about a second, at most every 12 h): the forecast is read against it
    String body;
    if (httpGet(s.lat, s.lon, body, reason, reasonSize, kind) == nullptr) {
      sensors::LocalTime lt;
      uint32_t fetchedAt = sensors::readLocalTime(lt) && lt.epochUtc > 0 ? (uint32_t)lt.epochUtc : 0;
      wx::Cache fresh;
      const char* perr = wx::parse(body.c_str(), s.lat, s.lon, fetchedAt, fresh);
      if (perr) {
        snprintf(reason, reasonSize, "%s", perr);
        kind = Fail::SERVER;                            // (an answer that can not be used)
      } else {
        cache() = fresh;
        cacheSave(fresh);
        ok = true;
      }
    }
  }
  wifiOff();
  if (kindOut) *kindOut = ok ? Fail::NONE : kind;

  Serial.printf("Weather update %s in %lu ms%s%s\n", ok ? "OK" : "FAILED", (unsigned long)(millis() - t0),
                ok ? "" : ": ", ok ? "" : reason);
  return ok;
}

}  // namespace wxfetch
