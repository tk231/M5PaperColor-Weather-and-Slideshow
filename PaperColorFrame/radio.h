// radio.h - switching the Wi-Fi radio off between its three uses (the network scan, the device's own access point and the
// home network for the weather).
//
// Starting the radio again at once after it was switched off makes ESP-IDF print
//     E (...) wifi_init_default: netstack cb reg failed with 12308
// 12308 is ESP_ERR_WIFI_STOP_STATE, "WiFi is stopping": the handler that sets the network interface up when the radio starts
// runs while the stop is still being processed, so the interface may not be set up properly. It happened in the weather
// fetch that starts right after the Connect page's access point was stopped (for instance after the settings were changed).
// So: after switching off, wait until the driver reports "off", then a little longer; only then may anything switch it on.
#pragma once
#include <WiFi.h>

namespace radio {

constexpr uint32_t SETTLE_MS = 250;      // extra time after the driver says "off"
constexpr uint32_t STOP_WAIT_MS = 1500;  // how long to wait for the driver at the very most

inline void off() {
  WiFi.mode(WIFI_OFF);
  const uint32_t t0 = millis();
  while (WiFi.getMode() != WIFI_OFF && millis() - t0 < STOP_WAIT_MS) delay(10);
  delay(SETTLE_MS);
}

}  // namespace radio
