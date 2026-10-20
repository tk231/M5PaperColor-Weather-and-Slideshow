// idlesleep.h - light sleep between events, to make the battery last.
//
// What the device does most of the time is nothing: the e-paper keeps its picture without power, and the next event (a redraw, a weather
// update, the next picture) is hours away. In light sleep the CPU is stopped and everything is kept (memory, the screen, the state of the
// program), and it wakes up by itself when a button is pressed (GPIO 10, 9 and 1, pressed = low) or when the time is up.
//
// It never sleeps while: the charger is connected (a charger is no battery drain, and the serial monitor and the charging light need the device
// awake), the Connect page is open (the Wi-Fi and the web server need it), a button is down, or the LED is showing something. A sleep lasts at most
// MAX_SLEEP_MS (the power state is looked at that often anyway) and ends exactly at the next deadline.
#pragma once
#include <esp_sleep.h>
#include <driver/gpio.h>

namespace idle {

constexpr uint32_t MIN_SLEEP_MS = 100;       // a shorter nap is not worth the cost of falling asleep and waking up
constexpr uint32_t MAX_SLEEP_MS = 5000;      // never longer than this at once
constexpr int      BUTTON_PINS[3] = { 10, 9, 1 };   // G10, G9, G1 (M5Unified: board_M5PaperColor), pressed = low

// How long to sleep now: the shortest of the maximum, the time until the LED may change and the time until the next deadline;
// 0 = do not sleep (nothing worth sleeping for, or something is going to happen at once).
inline uint32_t sleepMs(uint32_t ledQuietMs, uint32_t deadlineMs) {
  uint32_t ms = MAX_SLEEP_MS;
  if (ledQuietMs < ms) ms = ledQuietMs;
  if (deadlineMs < ms) ms = deadlineMs;
  return ms < MIN_SLEEP_MS ? 0 : ms;
}

// Light sleep for `ms`: wakes up at once when one of the three buttons is pressed, or when the time is up.
inline void lightSleep(uint32_t ms) {
  Serial.flush();
  for (int pin : BUTTON_PINS) gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();
}

}  // namespace idle
