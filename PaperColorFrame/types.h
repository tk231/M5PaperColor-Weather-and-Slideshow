// types.h - plain data types shared by the sketch, sensors.h and ui.h
// (no hardware dependencies, so the UI can also be rendered on a PC)
#pragma once
#include <stdint.h>
#include <stddef.h>

// ---- Weather icons ----------------------------------------------------------
enum WeatherIcon : uint8_t {
  ICON_NONE = 0,       // no data -> "?"
  ICON_CLEAR_DAY,
  ICON_CLEAR_NIGHT,
  ICON_PARTLY_DAY,
  ICON_PARTLY_NIGHT,
  ICON_CLOUDY,
  ICON_FOG,
  ICON_DRIZZLE,
  ICON_RAIN,
  ICON_SNOW,
  ICON_THUNDER,
  ICON_ERROR,          // sad little cloud, shown when there is no weather data
  ICON_WIND,           // windy: three wind streaks (see isWindy())
};

// Open-Meteo "weather_code" (WMO) -> icon. `night` picks the moon variants.
inline WeatherIcon iconFromWmo(int code, bool night) {
  switch (code) {
    case 0: case 1:                                   return night ? ICON_CLEAR_NIGHT : ICON_CLEAR_DAY;
    case 2:                                           return night ? ICON_PARTLY_NIGHT : ICON_PARTLY_DAY;
    case 3:                                           return ICON_CLOUDY;
    case 45: case 48:                                 return ICON_FOG;
    case 51: case 53: case 55: case 56: case 57:      return ICON_DRIZZLE;
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82:                        return ICON_RAIN;
    case 71: case 73: case 75: case 77:
    case 85: case 86:                                 return ICON_SNOW;
    case 95: case 96: case 99:                        return ICON_THUNDER;
    default:                                          return ICON_NONE;
  }
}

// WMO codes have no wind category, so "windy" is an override: the sky codes 0-3 (clear ... cloudy) with a wind at or above
// WINDY_KMH are "windy" (rain, fog, snow and thunder keep their own icon and word). One rule for the word and the icon.
constexpr float WINDY_KMH = 40.0f;
inline bool isWindy(int code, float windKmh, float windyKmh = WINDY_KMH) {
  return code >= 0 && code <= 3 && windKmh >= windyKmh;
}

// The icon for the current conditions: the wind icon when it is windy, otherwise the one for the WMO code.
inline WeatherIcon iconFor(int code, float windKmh, bool night) {
  return isWindy(code, windKmh) ? ICON_WIND : iconFromWmo(code, night);
}

// Is this one of the "sky" icons (the ones that the wind icon replaces)?
inline bool isSkyIcon(WeatherIcon i) {
  return i == ICON_CLEAR_DAY || i == ICON_CLEAR_NIGHT || i == ICON_PARTLY_DAY || i == ICON_PARTLY_NIGHT || i == ICON_CLOUDY;
}

// Short text for the current conditions ("Clear", "Cloudy", "Windy", ...).
inline const char* describeWeather(int code, float windKmh = 0, float windyKmh = WINDY_KMH) {
  if (isWindy(code, windKmh, windyKmh)) return "Windy";
  switch (code) {
    case 0:                                           return "Clear";
    case 1:                                           return "Mostly clear";
    case 2:                                           return "Partly cloudy";
    case 3:                                           return "Cloudy";
    case 45: case 48:                                 return "Fog";
    case 51: case 53: case 55: case 56: case 57:      return "Drizzle";
    case 61: case 63: case 65: case 66: case 67:      return "Rain";
    case 80: case 81: case 82:                        return "Showers";
    case 71: case 73: case 75: case 77:               return "Snow";
    case 85: case 86:                                 return "Snow showers";
    case 95: case 96: case 99:                        return "Thunderstorm";
    default:                                          return "";
  }
}

// ---- Forecast / power / reading --------------------------------------------
struct ForecastSlot {
  bool        valid = false;
  WeatherIcon icon  = ICON_NONE;
  float       tempC = 0;
};

struct PowerStatus {
  bool     batOk    = false;
  uint16_t batMv    = 0;
  uint16_t vinMv    = 0;
  int      src      = -1;     // raw M5PM1 power-source value (0 = USB 5VIN, 2 = battery), -1 = read failed
  bool     usb      = false;  // external power present
  bool     charging = false;  // INFERRED: USB present and battery not yet full
};

// Rough LiPo voltage -> percent. Only an estimate (varies with load and
// temperature, and reads high while USB is charging).
inline int batteryPercent(uint16_t mv) {
  static const struct { uint16_t mv; uint8_t pct; } table[] = {
    {4200, 100}, {4100, 90}, {4000, 80}, {3900, 65}, {3800, 50},
    {3700, 35},  {3600, 20}, {3500, 10}, {3400, 5},  {3300, 0},
  };
  if (mv >= table[0].mv) return 100;
  for (size_t i = 1; i < sizeof(table) / sizeof(table[0]); i++) {
    if (mv >= table[i].mv) {
      float f = (float)(mv - table[i].mv) / (float)(table[i - 1].mv - table[i].mv);
      return (int)(table[i].pct + f * (table[i - 1].pct - table[i].pct) + 0.5f);
    }
  }
  return 0;
}

// ---- microSD card ---------------------------------------------------------------
enum SdState : uint8_t {
  SD_ABSENT = 0,     // no card inserted
  SD_OK,             // card inserted and mounted
  SD_ERROR,          // card inserted, but it can't be read (not formatted FAT32, damaged, ...)
};

// One snapshot of everything shown on screen.
struct Reading {
  // indoor sensor (SHT40)
  bool  shtOk  = false;
  float tempC  = 0;
  float humPct = 0;

  // power
  PowerStatus power;

  // microSD card
  SdState sd = SD_ABSENT;

  // outdoor weather (Open-Meteo; step 3)
  bool         wxOk    = false;
  float        wxTempC = 0;
  WeatherIcon  wxIcon  = ICON_NONE;
  char         wxDesc[24] = "";          // "Cloudy", "Clear", "Windy", ...
  char         wxError[48] = "";         // why there is no data ("No Wi-Fi", ...); shown when !wxOk
  ForecastSlot morning, afternoon, night;
  bool         wxTomorrow = false;       // the Morning / Afternoon / Night slots are TOMORROW's
  char         fetchNote[48] = "";       // last update failed, but older data is still shown

  // time (from RTC)
  bool rtcOk = false;
  char date[16]     = "";        // YYYY-MM-DD            (header)
  char sensorTs[24] = "";        // YYYY-MM-DD HH:MM:SS   (this sensor reading)
  char apiTs[24]    = "never";   // YYYY-MM-DD HH:MM:SS   (last weather API update)
};

// ---- Pages (G9 = previous, G10 = next) ----------------------------------------
enum Page { PAGE_WEATHER, PAGE_SLIDESHOW, PAGE_CONNECT, PAGE_COUNT };      // the order G10 / G9 cycle through

// ---- "Connect to Device" page ------------------------------------------------
struct ConnectInfo {
  bool ok         = false;       // access point is up
  char ssid[40]   = "";
  char password[72] = "";        // empty = open network
  char url[48]    = "";          // e.g. http://192.168.4.1
  char wifiQr[256] = "";         // WIFI:T:WPA;S:<ssid>;P:<password>;;
  int  timeoutMin = 10;          // shown in the instructions
};
