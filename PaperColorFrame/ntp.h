// ntp.h - set the RTC from the internet clock (SNTP) while the Wi-Fi is up for a weather update.
//
// The RX8130 RTC is good, but it drifts a little, a phone sync can be a few seconds off, and a new device or a flat backup battery
// leaves it wrong altogether. A weather update has the Wi-Fi up anyway, so one SNTP question there costs about a second of radio.
// The clock is corrected BEFORE the forecast is asked for, so the forecast is read against the right time.
//
// The RTC keeps UTC (see timeutil.h), and SNTP delivers UTC: no time zone is involved here.
#pragma once
#include <Arduino.h>
#include <sys/time.h>
#include "sensors.h"
#include "timeutil.h"

namespace ntp {

constexpr uint32_t TIMEOUT_MS = 3000;                  // how long to wait for an answer
constexpr uint32_t RESYNC_MS  = 12UL * 3600 * 1000;    // after an answer: ask again after this long (and at the first update after a start)
constexpr uint32_t RETRY_MS   = 24UL * 3600 * 1000;    // after NO answer (UDP blocked, no server): not at every update: it would wait 3 s each time for nothing. (When the device is not online at all, the weather
                                                       // update fails by itself and the device waits for the user: see weather_fetch.h, Fail::NETWORK. A new Wi-Fi setting asks again at once.)
constexpr int64_t  MIN_DIFF_S = 2;                     // a smaller difference is within what a one-second clock can measure: the RTC is left alone
constexpr int64_t  EPOCH_MIN  = 1700000000LL;          // 2023-11-14: an "internet time" before this is not real
constexpr int64_t  EPOCH_MAX  = 4102444800LL;          // 2100-01-01

struct State {
  bool     tried      = false;   // asked at least once since the start
  bool     lastFailed = false;   // ... and the last time there was no (usable) answer
  uint32_t lastTryMs  = 0;       // millis() of the last question
};
inline State& state() { static State s; return s; }

// Should the internet clock be asked now? An RTC that is not valid is always worth a try.
inline bool due(const State& s, bool rtcValid, uint32_t nowMs) {
  if (!rtcValid || !s.tried) return true;
  const uint32_t since = nowMs - s.lastTryMs;           // (unsigned: right across the 49-day wrap of millis())
  return since >= (s.lastFailed ? RETRY_MS : RESYNC_MS);
}

inline bool plausible(int64_t epochUtc) { return epochUtc >= EPOCH_MIN && epochUtc < EPOCH_MAX; }

// Set the RTC to the internet time? (A difference below MIN_DIFF_S is noise: the RTC is not rewritten for nothing.)
inline bool shouldSet(bool rtcValid, int64_t rtcEpoch, int64_t ntpEpoch) {
  if (!plausible(ntpEpoch)) return false;
  if (!rtcValid) return true;
  const int64_t d = rtcEpoch - ntpEpoch;
  return d >= MIN_DIFF_S || d <= -MIN_DIFF_S;
}

// ---- the real question --------------------------------------------------------------------------------------
// UTC epoch, or 0 if there was no answer within `timeoutMs`. The system clock is reset to 1970 first, so that "the system clock is
// valid" can only mean "SNTP has just answered" (SNTP keeps a clock running after an earlier answer, and that must not pass as a new one).
inline int64_t querySntp(uint32_t timeoutMs) {
  struct timeval zero = { 0, 0 };
  settimeofday(&zero, nullptr);
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");        // offsets 0: the fields below are UTC
  const uint32_t t0 = millis();
  struct tm t;
  while (millis() - t0 < timeoutMs) {
    if (getLocalTime(&t, 100)) {                                                    // waits up to 100 ms for the time to be set
      return timeutil::epochFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    }
  }
  return 0;
}

// The question can be replaced (the PC tests do that).
typedef int64_t (*QueryFn)(uint32_t timeoutMs);
inline QueryFn& queryHook() { static QueryFn f = querySntp; return f; }

struct Outcome {
  bool    asked    = false;    // the internet clock was asked
  bool    answered = false;    // and it gave a usable answer
  bool    set      = false;    // and the RTC was changed
  int64_t diffS    = 0;        // RTC minus internet time, in seconds (positive: the RTC was fast)
};

// Call it while the Wi-Fi is connected. Takes at most TIMEOUT_MS, and only when due (see due()).
inline Outcome syncRtc() {
  Outcome o;
  State& st = state();
  sensors::LocalTime before;
  const bool rtcValid = sensors::readLocalTime(before) && before.epochUtc > 0;
  if (!due(st, rtcValid, millis())) return o;
  o.asked = true;

  const uint32_t t0 = millis();
  const int64_t e = queryHook()(TIMEOUT_MS);
  st.tried = true;
  st.lastTryMs = millis();
  st.lastFailed = !plausible(e);
  if (st.lastFailed) {
    Serial.printf("NTP: no answer in %lu ms%s\n", (unsigned long)(millis() - t0), rtcValid ? "" : " (and the RTC has never been set)");
    return o;
  }
  o.answered = true;

  sensors::LocalTime now;                                // read again: the question took time, and `e` is the time at its end
  const bool nowValid = sensors::readLocalTime(now) && now.epochUtc > 0;
  o.diffS = nowValid ? now.epochUtc - e : 0;
  if (shouldSet(nowValid, nowValid ? now.epochUtc : 0, e)) {
    o.set = sensors::setRtcFromEpoch(e);
    if (nowValid) Serial.printf("NTP: the RTC was %lld s %s: corrected\n", (long long)(o.diffS < 0 ? -o.diffS : o.diffS), o.diffS > 0 ? "fast" : "slow");
    else          Serial.println("NTP: the RTC was not set: set from the internet clock");
  } else {
    Serial.printf("NTP: the RTC is right (%lld s apart)\n", (long long)o.diffS);
  }
  return o;
}

}  // namespace ntp
