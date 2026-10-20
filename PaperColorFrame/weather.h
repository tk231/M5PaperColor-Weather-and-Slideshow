// weather.h - Open-Meteo answer -> compact cache -> what the screen shows. No hardware here,
// so all of it can be tested on a PC.
//
// What is fetched (one request, see weather_fetch.h):
//   current   = temperature_2m, weather_code, wind_speed_10m, is_day          (a snapshot)
//   hourly    = the same four values for 72 hours, starting at local midnight today
//   timeformat=unixtime, timezone=auto  ->  all times are UTC epochs, plus utc_offset_seconds
//
// Why the whole hourly series is kept: the Weather page is redrawn much more often than the
// forecast is fetched, and with a 12 or 24 hour fetch interval a "current" snapshot would be
// hours old. So "now" is read from the hourly series for the current hour once the snapshot
// is older than 90 minutes, and the Morning / Afternoon / Night slots come from the same data.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "types.h"
#include "timeutil.h"

namespace wx {

constexpr int      MAX_HOURS         = 72;
constexpr int16_t  TEMP_MISSING      = -32768;
constexpr uint8_t  MISSING           = 255;
constexpr uint32_t CACHE_MAGIC       = 0x31585750;   // "PWX1"
constexpr uint16_t CACHE_VERSION     = 1;            // bump when the layout of Cache changes
constexpr int64_t  CURRENT_MAX_AGE_S = 90 * 60;      // use the "current" snapshot only while it is this fresh
constexpr int      TOMORROW_FROM_HOUR = 21;          // from this local hour the slots show TOMORROW

struct Cache {
  uint32_t magic;
  uint16_t version;
  uint16_t size;                  // sizeof(Cache), to notice a changed layout

  float    lat, lon;              // the place this forecast belongs to
  uint32_t fetchedAt;             // UTC epoch seconds when it was fetched (0 = unknown)
  int32_t  utcOffsetSec;          // the place's UTC offset at fetch time

  uint8_t  curValid;              // snapshot ("current")
  uint8_t  curCode, curWind, curIsDay;
  int16_t  curTempX10;
  uint32_t curTime;               // UTC epoch of the snapshot

  uint32_t firstHour;             // UTC epoch of hourly entry 0 (a local hour boundary)
  uint8_t  count;                 // number of hourly entries
  uint8_t  pad[3];
  int16_t  tempX10[MAX_HOURS];    // temperature * 10, TEMP_MISSING if unknown
  uint8_t  code[MAX_HOURS];       // WMO weather code, MISSING if unknown
  uint8_t  wind[MAX_HOURS];       // km/h, MISSING if unknown
  uint8_t  isDay[MAX_HOURS];      // 1 = day, 0 = night
};

inline void clear(Cache& c) { memset(&c, 0, sizeof(c)); }

// Is this a usable cache (right layout, has data)?
inline bool valid(const Cache& c) {
  return c.magic == CACHE_MAGIC && c.version == CACHE_VERSION && c.size == sizeof(Cache) &&
         c.count > 0 && c.count <= MAX_HOURS;
}

inline bool sameLocation(const Cache& c, float lat, float lon) {
  return fabsf(c.lat - lat) < 0.01f && fabsf(c.lon - lon) < 0.01f;
}

// ============================================================================
//  Parsing. Tolerant of spaces / line breaks and of any key order; understands null.
// ============================================================================

// Pointer to the VALUE of "key" (after the colon) inside [from, to), or nullptr.
inline const char* findValue(const char* from, const char* to, const char* key) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const size_t plen = strlen(pat);
  const char* p = from;
  while ((p = strstr(p, pat)) != nullptr && p < to) {
    const char* q = p + plen;
    while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++;
    if (*q == ':') {
      q++;
      while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++;
      return q;
    }
    p += plen;                                // it was a string VALUE equal to the key, keep looking
  }
  return nullptr;
}

// The body of a flat object ("key":{ ... }) : start after '{', end at the closing '}'.
inline const char* findObject(const char* json, const char* key, const char*& end) {
  const char* v = findValue(json, json + strlen(json), key);
  if (!v || *v != '{') return nullptr;
  v++;
  const char* e = strchr(v, '}');
  if (!e) return nullptr;
  end = e;
  return v;
}

inline bool readNumber(const char* from, const char* to, const char* key, double& out) {
  const char* v = findValue(from, to, key);
  if (!v || v >= to) return false;
  if (strncmp(v, "null", 4) == 0) return false;
  char* e = nullptr;
  double d = strtod(v, &e);
  if (e == v) return false;
  out = d;
  return true;
}

// Reads "key":[1,2,null,3] into out[] (null -> NaN). Returns the TOTAL number of entries
// (which may be more than maxN), or -1 if the key is missing / malformed.
inline int readArray(const char* from, const char* to, const char* key, double* out, int maxN) {
  const char* p = findValue(from, to, key);
  if (!p || p >= to || *p != '[') return -1;
  p++;
  int n = 0;
  while (p < to) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p == ']') return n;
    double v;
    if (strncmp(p, "null", 4) == 0) { v = NAN; p += 4; }
    else {
      char* e = nullptr;
      v = strtod(p, &e);
      if (e == p) return -1;
      p = e;
    }
    if (n < maxN) out[n] = v;
    n++;
  }
  return -1;                                   // no closing bracket
}

inline int16_t toTenths(double t) {
  if (isnan(t)) return TEMP_MISSING;
  double v = round(t * 10.0);
  if (v < -1000) v = -1000;                    // -100 C .. 100 C is more than enough
  if (v > 1000)  v = 1000;
  return (int16_t)v;
}

inline uint8_t toByte(double v) {
  if (isnan(v) || v < 0) return MISSING;
  if (v > 254) v = 254;
  return (uint8_t)lround(v);
}

// Parses the Open-Meteo JSON into `out`. Returns nullptr on success, or a SHORT reason
// (shown on the screen) on failure; `out` is only touched on success.
inline const char* parse(const char* json, float lat, float lon, uint32_t fetchedAt, Cache& out) {
  if (!json || !*json) return "Empty answer from Open-Meteo";
  const char* jsonEnd = json + strlen(json);

  Cache c;
  clear(c);
  c.magic = CACHE_MAGIC;
  c.version = CACHE_VERSION;
  c.size = (uint16_t)sizeof(Cache);
  c.lat = lat;
  c.lon = lon;
  c.fetchedAt = fetchedAt;

  double v;
  if (!readNumber(json, jsonEnd, "utc_offset_seconds", v)) return "Unexpected answer from Open-Meteo";
  c.utcOffsetSec = (int32_t)v;

  // ---- hourly series ----
  const char* hEnd = nullptr;
  const char* h = findObject(json, "hourly", hEnd);
  if (!h) return "Unexpected answer from Open-Meteo";

  static double tmp[MAX_HOURS];
  int nTime = readArray(h, hEnd, "time", tmp, MAX_HOURS);
  if (nTime < 24) return "Forecast from Open-Meteo too short";
  int count = nTime < MAX_HOURS ? nTime : MAX_HOURS;
  if (isnan(tmp[0]) || tmp[0] < 1e9) return "Unexpected answer from Open-Meteo";
  c.firstHour = (uint32_t)tmp[0];
  if (isnan(tmp[1]) || (int64_t)(tmp[1] - tmp[0]) != 3600) return "Unexpected time step in forecast";
  c.count = (uint8_t)count;

  int n = readArray(h, hEnd, "temperature_2m", tmp, MAX_HOURS);
  if (n < count) return "Incomplete temperatures from Open-Meteo";
  for (int i = 0; i < count; i++) c.tempX10[i] = toTenths(tmp[i]);

  n = readArray(h, hEnd, "weather_code", tmp, MAX_HOURS);
  if (n < count) return "Incomplete weather codes from Open-Meteo";
  for (int i = 0; i < count; i++) c.code[i] = toByte(tmp[i]);

  n = readArray(h, hEnd, "wind_speed_10m", tmp, MAX_HOURS);
  for (int i = 0; i < count; i++) c.wind[i] = (n >= count) ? toByte(tmp[i]) : MISSING;

  n = readArray(h, hEnd, "is_day", tmp, MAX_HOURS);
  for (int i = 0; i < count; i++) c.isDay[i] = (n >= count && !isnan(tmp[i])) ? (tmp[i] > 0.5 ? 1 : 0) : 1;

  // ---- current snapshot (optional: the hourly series can stand in for it) ----
  const char* cEnd = nullptr;
  const char* cur = findObject(json, "current", cEnd);
  if (cur) {
    double t, code, time;
    if (readNumber(cur, cEnd, "time", time) && readNumber(cur, cEnd, "temperature_2m", t) &&
        readNumber(cur, cEnd, "weather_code", code)) {
      double wind = 0, day = 1;
      bool haveWind = readNumber(cur, cEnd, "wind_speed_10m", wind);
      readNumber(cur, cEnd, "is_day", day);
      c.curValid   = 1;
      c.curTime    = (uint32_t)time;
      c.curTempX10 = toTenths(t);
      c.curCode    = toByte(code);
      c.curWind    = haveWind ? toByte(wind) : MISSING;
      c.curIsDay   = day > 0.5 ? 1 : 0;
    }
  }

  out = c;
  return nullptr;
}

// ============================================================================
//  Deriving what the screen shows at a given moment
// ============================================================================

// Picks the icon for a 6-hour slot from its weather codes.
//   thunderstorm in any hour       -> thunder
//   rain / drizzle / snow in 2+ hours -> the most frequent of those (snow wins ties, then rain)
//   otherwise                      -> the most frequent sky (clear / partly / cloudy / fog),
//                                     the cloudier one wins ties
// A single wet hour in an otherwise dry slot is ignored on purpose.
// The icon of a Morning / Afternoon / Night slot: pickSlotIcon(), but a sky icon becomes the wind icon when the AVERAGE wind of the hours
// that have a wind value is at or above WINDY_KMH (one gusty hour in a calm slot does not count).
inline WeatherIcon pickSlotIcon(const uint8_t* codes, int n, bool night);
inline WeatherIcon slotIcon(const uint8_t* codes, const uint8_t* winds, int n, bool night) {
  const WeatherIcon ic = pickSlotIcon(codes, n, night);
  if (!isSkyIcon(ic)) return ic;
  int sum = 0, cnt = 0;
  for (int i = 0; i < n; i++) if (winds[i] != MISSING) { sum += winds[i]; cnt++; }
  return (cnt > 0 && (float)sum / (float)cnt >= WINDY_KMH) ? ICON_WIND : ic;
}

inline WeatherIcon pickSlotIcon(const uint8_t* codes, int n, bool night) {
  int clear_ = 0, partly = 0, cloudy = 0, fog = 0, drizzle = 0, rain = 0, snow = 0, thunder = 0, valid = 0;
  for (int i = 0; i < n; i++) {
    if (codes[i] == MISSING) continue;
    valid++;
    switch (iconFromWmo(codes[i], false)) {
      case ICON_CLEAR_DAY:   clear_++;  break;
      case ICON_PARTLY_DAY:  partly++;  break;
      case ICON_CLOUDY:      cloudy++;  break;
      case ICON_FOG:         fog++;     break;
      case ICON_DRIZZLE:     drizzle++; break;
      case ICON_RAIN:        rain++;    break;
      case ICON_SNOW:        snow++;    break;
      case ICON_THUNDER:     thunder++; break;
      default: break;
    }
  }
  if (valid == 0) return ICON_NONE;
  if (thunder >= 1) return ICON_THUNDER;
  if (drizzle + rain + snow >= 2) {
    if (snow >= rain && snow >= drizzle) return ICON_SNOW;
    return rain >= drizzle ? ICON_RAIN : ICON_DRIZZLE;
  }
  // sky: the most frequent condition wins; on a tie the "heavier" one wins (fog > cloudy > partly > clear)
  struct Sky { int count; int rank; WeatherIcon icon; };
  const Sky sky[4] = { { clear_, 1, ICON_CLEAR_DAY }, { partly, 2, ICON_PARTLY_DAY }, { cloudy, 3, ICON_CLOUDY }, { fog, 4, ICON_FOG } };
  Sky best = sky[0];
  for (int i = 1; i < 4; i++) {
    if (sky[i].count > best.count || (sky[i].count == best.count && sky[i].rank > best.rank)) best = sky[i];
  }
  if (best.count == 0) return ICON_NONE;
  WeatherIcon icon = best.icon;
  if (night) {
    if (icon == ICON_CLEAR_DAY)  return ICON_CLEAR_NIGHT;
    if (icon == ICON_PARTLY_DAY) return ICON_PARTLY_NIGHT;
  }
  return icon;
}

inline int64_t floorDiv(int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }

// Fills r.wxOk / wxTempC / wxIcon / wxDesc / morning / afternoon / night / wxTomorrow for the
// moment `nowUtc`. Returns false (and leaves r alone) if the cache does not cover this moment.
inline bool derive(const Cache& c, int64_t nowUtc, Reading& r) {
  if (!valid(c)) return false;
  const int64_t idx = floorDiv(nowUtc - (int64_t)c.firstHour, 3600);
  if (idx < 0 || idx >= c.count) return false;                         // forecast does not reach this moment

  // ---- now ----
  int16_t tempX10; uint8_t code, wind, isDay;
  const bool snapshotFresh = c.curValid && nowUtc >= (int64_t)c.curTime && (nowUtc - (int64_t)c.curTime) <= CURRENT_MAX_AGE_S &&
                             c.curTempX10 != TEMP_MISSING && c.curCode != MISSING;
  if (snapshotFresh) {
    tempX10 = c.curTempX10; code = c.curCode; wind = c.curWind; isDay = c.curIsDay;
  } else {
    int i = (int)idx;
    if (c.tempX10[i] == TEMP_MISSING || c.code[i] == MISSING) {          // look at the neighbouring hours
      if (i > 0 && c.tempX10[i - 1] != TEMP_MISSING && c.code[i - 1] != MISSING) i = i - 1;
      else if (i + 1 < c.count && c.tempX10[i + 1] != TEMP_MISSING && c.code[i + 1] != MISSING) i = i + 1;
      else return false;
    }
    tempX10 = c.tempX10[i]; code = c.code[i]; wind = c.wind[i]; isDay = c.isDay[i];
  }

  // ---- Morning / Afternoon / Night: today until TOMORROW_FROM_HOUR local, then tomorrow ----
  const int64_t localNow   = nowUtc + c.utcOffsetSec;
  const int     localHour  = (int)(floorDiv(localNow, 3600) - floorDiv(localNow, 86400) * 24);
  const bool    tomorrow   = localHour >= TOMORROW_FROM_HOUR;
  const int64_t dayStartUtc = floorDiv(localNow, 86400) * 86400 - c.utcOffsetSec + (tomorrow ? 86400 : 0);

  static const int startHour[3] = { 6, 12, 18 };
  ForecastSlot* slots[3] = { &r.morning, &r.afternoon, &r.night };
  for (int s = 0; s < 3; s++) {
    uint8_t codes[6], winds[6]; int nCodes = 0; int tempSum = 0, tempN = 0;
    for (int h = 0; h < 6; h++) {
      int64_t i = floorDiv(dayStartUtc + (int64_t)(startHour[s] + h) * 3600 - (int64_t)c.firstHour, 3600);
      if (i < 0 || i >= c.count) continue;                               // outside the forecast
      if (c.tempX10[i] != TEMP_MISSING) { tempSum += c.tempX10[i]; tempN++; }
      winds[nCodes] = c.wind[i];
      codes[nCodes++] = c.code[i];
    }
    ForecastSlot& f = *slots[s];
    f = ForecastSlot();
    if (tempN > 0) {
      f.valid = true;
      f.tempC = (float)tempSum / (float)tempN / 10.0f;
      f.icon  = slotIcon(codes, winds, nCodes, s == 2);
      if (f.icon == ICON_NONE) f.valid = false;
    }
  }

  r.wxOk       = true;
  r.wxTempC    = tempX10 / 10.0f;
  r.wxIcon     = iconFor(code, wind == MISSING ? 0.0f : (float)wind, !isDay);
  snprintf(r.wxDesc, sizeof(r.wxDesc), "%s", describeWeather(code, wind == MISSING ? 0.0f : (float)wind));
  r.wxTomorrow = tomorrow;
  return true;
}

}  // namespace wx
