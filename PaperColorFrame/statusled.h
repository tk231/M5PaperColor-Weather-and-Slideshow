// statusled.h - the status light: what the RGB LED says about the device.
//
//   Everything the LED shows is decided by compute(): a pure function of the inputs (what the device is doing) and the time. A small task
//   calls tick() every 40 ms, so the patterns keep running while the main program is blocked (a panel refresh takes seconds, a weather
//   update up to half a minute). Manual colours from the LED tab of the web page win over everything while they are on.
//
//   Priority (the first that applies wins): error > button > refreshing > fetching > uploading > low battery (a short flash) > access point
//   > charging / charged. Activity is above the ambient states on purpose: with the device on a charger the busy patterns must still show.
#pragma once
#include <math.h>
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "types.h"

namespace sled {

struct Rgb { uint8_t r, g, b; };
constexpr Rgb OFF    = {0,   0,   0};
constexpr Rgb RED    = {255, 0,   0};
constexpr Rgb YELLOW = {255, 170, 0};
constexpr Rgb PINK   = {255, 60,  150};
constexpr Rgb BLUE   = {0,   70,  255};
constexpr Rgb GREEN  = {0,   255, 40};
constexpr Rgb WHITE  = {255, 255, 255};

// ---- timing (ms) ----
constexpr uint32_t ERROR_SHOW_MS   = 10000;    // an error flashes this long after it happened
constexpr uint32_t ERROR_PERIOD    = 600;      //   red 200, dark 100, yellow 200, dark 100
constexpr uint32_t BUTTON_PULSE_MS = 1200;     // one long white pulse per button press
constexpr uint32_t REFRESH_HALF    = 250;      // pink blinks 250 ms on, 250 ms off while the panel refreshes
constexpr uint32_t FETCH_PERIOD    = 2000;     // a blue pulse (1 s), then a green pulse (1 s), while the weather is fetched
constexpr uint32_t UPLOAD_HALF     = 100;      // fast blue blink while a picture is uploaded
constexpr uint32_t UPLOAD_HOLD_MS  = 1500;     //   an upload counts as running this long after its last data
constexpr uint32_t LOWBAT_PERIOD   = 30000;    // low battery: one short red flash every 30 s
constexpr uint32_t LOWBAT_FLASH    = 150;
constexpr uint32_t AP_PERIOD       = 3000;     // access point waiting for a phone: slow white pulse
constexpr uint32_t CHARGE_PERIOD   = 3000;     // charging: slow yellow pulse
constexpr uint32_t TICK_MS         = 40;
constexpr int      LOW_BATTERY_PCT = 15;       // at or below this (and not on USB power) the low battery flash starts
constexpr uint16_t FULL_ON_MV      = 4150;     // on USB power: from this voltage the battery counts as charged ...
constexpr uint16_t FULL_OFF_MV     = 4100;     // ... and counts as charging again only below this (no flicker around the limit)

// An event that lasts a while after it was started. Not set until started (a plain 0 would be a real time after 24 days of uptime).
// It is judged by the time that has passed since the start (right across the 49-day wrap of millis()), and expire() clears it once it is over:
// without that an event that ended days ago would come back to life after 24.8 days of uptime (a "time left" below 2^31 looks like the future).
struct Timer {
  volatile bool     set   = false;
  volatile uint32_t start_ = 0, dur = 0;
  void start(uint32_t now, uint32_t ms) { dur = ms; start_ = now; set = true; }      // (set last: the task never sees a half-written timer as running)
  bool active(uint32_t now) const { return set && (uint32_t)(now - start_) < dur; }
  uint32_t left(uint32_t now) const { return active(now) ? dur - (uint32_t)(now - start_) : 0; }
  void expire(uint32_t now) { if (set && (uint32_t)(now - start_) >= dur) set = false; }
};

struct Inputs {
  // settings (LED tab of the web page)
  bool    enabled = true;
  uint8_t pct     = 15;                          // brightness of the status light, in percent
  // the manual colour of the LED tab: wins over everything while it is on
  bool    manualOn = false;
  uint8_t mr = 0, mg = 0, mb = 255, mbr = 64;
  // events
  Timer error, button, upload;
  // states
  bool refreshing = false, fetching = false, apRunning = false, apClient = false;
  bool charging = false, full = false, lowBattery = false;
};

struct Out { Rgb c; uint8_t br; };

// 0 -> 255 -> 0 over `period` ms, smooth (a raised cosine).
inline uint8_t pulse(uint32_t t, uint32_t period) {
  const float x = (float)(t % period) / (float)period;
  return (uint8_t)(255.0f * (1.0f - cosf(6.2831853f * x)) * 0.5f + 0.5f);
}
inline Rgb scale(Rgb c, uint8_t f) {
  return { (uint8_t)((c.r * f + 127) / 255), (uint8_t)((c.g * f + 127) / 255), (uint8_t)((c.b * f + 127) / 255) };
}
inline uint8_t brightnessFromPct(uint8_t pct) {
  if (pct == 0) return 0;
  if (pct > 100) pct = 100;
  return (uint8_t)((pct * 255 + 50) / 100);                // (1 % is already 3: there is no need for a minimum)
}

// What the LED shows at time `now`.
inline Out compute(const Inputs& in, uint32_t now) {
  if (in.manualOn) return { { in.mr, in.mg, in.mb }, in.mbr };
  Out o = { OFF, brightnessFromPct(in.pct) };
  if (!in.enabled) return o;

  if (in.error.active(now)) {
    const uint32_t p = now % ERROR_PERIOD;
    o.c = p < 200 ? RED : (p >= 300 && p < 500) ? YELLOW : OFF;
  } else if (in.button.active(now)) {
    o.c = scale(WHITE, pulse(BUTTON_PULSE_MS - in.button.left(now), BUTTON_PULSE_MS));
  } else if (in.refreshing) {
    o.c = ((now / REFRESH_HALF) % 2 == 0) ? PINK : OFF;
  } else if (in.fetching) {
    const uint32_t p = now % FETCH_PERIOD;
    o.c = p < FETCH_PERIOD / 2 ? scale(BLUE, pulse(p, FETCH_PERIOD / 2)) : scale(GREEN, pulse(p - FETCH_PERIOD / 2, FETCH_PERIOD / 2));
  } else if (in.upload.active(now)) {
    o.c = ((now / UPLOAD_HALF) % 2 == 0) ? BLUE : OFF;
  } else if (in.lowBattery && (now % LOWBAT_PERIOD) < LOWBAT_FLASH) {
    o.c = RED;
  } else if (in.apRunning) {
    o.c = in.apClient ? WHITE : scale(WHITE, pulse(now, AP_PERIOD));
  } else if (in.charging) {                             // for as long as the charger is connected (a charger is no battery drain)
    o.c = scale(YELLOW, pulse(now, CHARGE_PERIOD));
  } else if (in.full) {
    o.c = GREEN;
  }
  return o;
}

// For how long the LED can be left alone: 0 = it shows something now (or is about to change), a number = the time until the next low battery flash,
// UINT32_MAX = nothing will happen by itself. (The device may only sleep while this is more than 0: the task that animates the LED sleeps too.)
inline uint32_t quietForMs(const Inputs& in, uint32_t now) {
  if (in.manualOn || !in.enabled) return UINT32_MAX;                        // a steady manual colour, or nothing at all: nothing moves
  if (in.error.active(now) || in.button.active(now) || in.upload.active(now)) return 0;
  if (in.refreshing || in.fetching || in.apRunning || in.charging || in.full) return 0;
  if (in.lowBattery) {
    const uint32_t p = now % LOWBAT_PERIOD;
    return p < LOWBAT_FLASH ? 0 : LOWBAT_PERIOD - p;                        // until the next flash starts
  }
  return UINT32_MAX;
}

// ---- the real LED ---------------------------------------------------------------------------------------
inline Inputs& in() { static Inputs i; return i; }

struct Last { int r = -1, g = -1, b = -1, br = -1; };
inline Last& last() { static Last l; return l; }
inline SemaphoreHandle_t& lockHandle() { static SemaphoreHandle_t m = nullptr; return m; }

// Computes what the LED should show and writes it if it changed (nothing is sent on the I2C bus while the LED stays as it is).
// Safe to call from the task and from the web handlers: one writer at a time.
inline void tick(uint32_t now) {
  if (!M5.Led.isEnabled()) return;
  SemaphoreHandle_t m = lockHandle();
  if (m) xSemaphoreTake(m, portMAX_DELAY);
  Inputs& i = in();
  i.error.expire(now); i.button.expire(now); i.upload.expire(now);     // events that are over are cleared (see Timer)
  const Out o = compute(i, now);
  Last& l = last();
  if (o.c.r != l.r || o.c.g != l.g || o.c.b != l.b || o.br != l.br) {
    M5.Led.setBrightness(o.br);
    M5.Led.setAllColor(o.c.r, o.c.g, o.c.b);
    M5.Led.display();
    l.r = o.c.r; l.g = o.c.g; l.b = o.c.b; l.br = o.br;
  }
  if (m) xSemaphoreGive(m);
}

inline void taskMain(void*) {
  for (;;) {
    tick(millis());
    vTaskDelay(pdMS_TO_TICKS(TICK_MS));
  }
}

// Starts the task (once).
inline void begin() {
  static bool started = false;
  if (started) return;
  started = true;
  lockHandle() = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(taskMain, "statusled", 3072, nullptr, 1, nullptr, 0);
}

// ---- what the rest of the program tells it ---------------------------------------------------------------
inline void applySettings(bool on, uint8_t pct) { in().enabled = on; in().pct = pct; }
inline void eventError(uint32_t now)  { in().error.start(now, ERROR_SHOW_MS); }
inline void eventButton(uint32_t now) { in().button.start(now, BUTTON_PULSE_MS); }
inline void eventUpload(uint32_t now) { in().upload.start(now, UPLOAD_HOLD_MS); }

// Power: USB present or not, charging or charged (inferred from the voltage), low battery.
inline void setPower(const PowerStatus& p, uint32_t now) {
  Inputs& i = in();
  const bool full     = p.batOk && p.usb && (p.batMv >= FULL_ON_MV || (i.full && p.batMv >= FULL_OFF_MV));
  const bool charging = p.batOk && p.usb && !full;
  i.charging   = charging;
  i.full       = full;
  i.lowBattery = p.batOk && !p.usb && batteryPercent(p.batMv) <= LOW_BATTERY_PCT;
}

}  // namespace sled
