// battery.h - when the device shows the "battery empty" screen and switches itself off.
// Pure logic, no hardware: the sketch hands it readings, so the PC tests can check every case.
//
// Why: an e-paper keeps showing its last picture after the power is gone, so a device whose battery has died
// looks like a device that has crashed. So before the battery is really empty the device shows one clear
// screen (a crossed-out battery) and switches itself off with the PM1 chip. It does the same when it is
// started (power button) while the battery is still too low, instead of starting up and browning out.
// The factory firmware does the same at start-up (it switches off below 3.1 V).
#pragma once
#include "types.h"

namespace bat {

// Resting-voltage limits in mV (see batteryPercent(): 3300 = 0 %, 3400 = 5 %, 3500 = 10 %). To be tuned on the real device.
constexpr uint16_t STOP_MV     = 3350;       // running on battery: at or below this the device stops (3.1 V is where it would brown out)
constexpr uint16_t BOOT_MIN_MV = 3500;       // starting on battery: below this it does not start. Higher than STOP_MV on purpose:
                                             // after a switch-off the voltage recovers a little, and that must not allow another start
constexpr uint32_t CHECK_MS    = 30000UL;    // how often the battery is looked at while running
constexpr uint8_t  LOW_READINGS = 2;         // this many low readings IN A ROW are needed: one reading can dip while the radio is sending

// Starting up: refuse to start? Only on battery power (with USB the device may start, and charges), and only on a real reading.
inline bool bootRefused(const PowerStatus& p) {
  return p.batOk && !p.usb && p.batMv < BOOT_MIN_MV;
}

// Running: feed it a reading every CHECK_MS; true = stop now.
struct Guard {
  uint8_t low = 0;
  bool update(const PowerStatus& p) {
    if (!p.batOk || p.usb || p.batMv > STOP_MV) { low = 0; return false; }     // no reading, USB power, or a fine voltage: all clear
    if (low < 255) low++;
    return low >= LOW_READINGS;
  }
};

}  // namespace bat
