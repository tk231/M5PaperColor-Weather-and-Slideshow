// timeutil.h - calendar math with no hardware or timezone-database dependencies.
// The RTC keeps UTC; the local time zone is a plain offset in minutes that the
// phone sends when it connects. (Algorithms: Howard Hinnant's civil-date functions.)
#pragma once
#include <stdint.h>

namespace timeutil {

struct Civil {
  int year, month, day;      // month 1..12, day 1..31
  int hour, minute, second;
  int weekday;               // 0 = Sunday
};

// Days since 1970-01-01 for a proleptic Gregorian date.
inline int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;                                     // [0, 399]
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;    // [0, 365]
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;             // [0, 146096]
  return era * 146097 + doe - 719468;
}

inline void civilFromDays(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;                                              // [0, 146096]
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;         // [0, 399]
  const int64_t yy  = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);                       // [0, 365]
  const int64_t mp  = (5 * doy + 2) / 153;                                           // [0, 11]
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yy + (m <= 2));
}

// UTC date/time -> seconds since 1970-01-01 00:00:00 UTC.
inline int64_t epochFromCivil(int y, int mo, int d, int h, int mi, int s) {
  return daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

// Seconds since the epoch -> date/time fields (no time zone applied).
inline Civil civilFromEpoch(int64_t t) {
  int64_t days = t / 86400;
  int64_t rem  = t % 86400;
  if (rem < 0) { rem += 86400; days -= 1; }
  Civil c;
  civilFromDays(days, c.year, c.month, c.day);
  c.hour    = (int)(rem / 3600);
  c.minute  = (int)((rem % 3600) / 60);
  c.second  = (int)(rem % 60);
  int64_t wd = (days + 4) % 7;             // 1970-01-01 was a Thursday
  c.weekday = (int)(wd < 0 ? wd + 7 : wd);
  return c;
}

}  // namespace timeutil
