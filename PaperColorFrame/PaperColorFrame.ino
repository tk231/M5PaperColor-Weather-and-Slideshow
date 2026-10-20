// PaperColorFrame - a picture frame and weather station for the M5Stack PaperColor (ESP32-S3, 600x400 colour e-paper)
//
// Three pages (G10 = next, G9 = previous):  Weather  ->  Slideshow  ->  Connect to Device
//   Weather            the outdoor weather from Open-Meteo (now, morning / afternoon / night), the indoor temperature and humidity
//   Slideshow          the pictures on the microSD card, one after the other (picture editor and gallery in the web page)
//   Connect to Device  the device's own Wi-Fi and a web page: settings, pictures, LED, instructions
//
// Buttons: G9 = UP (previous page), G10 = DOWN (next page).
//          G1 (top button): on the Weather page = update the weather now; on the Slideshow page = next picture.
//          The Power button belongs to the PM1 chip (single click = restart, double click = switch off).
//
// Arduino IDE board settings (Tools menu):
//   Board: ESP32S3 Dev Module | Flash Size: 16MB | PSRAM: OPI PSRAM
//   USB CDC On Boot: Enabled  | Serial monitor: 115200
//
// Libraries: M5Unified, M5GFX, M5PM1
//
// Build profile: BUILD_PROFILE below. PROFILE_TEST has short intervals (for developing), PROFILE_PRODUCTION has intervals of hours.
//
// Files in this sketch folder:
//   PaperColorFrame.ino  setup / loop / page handling
//   types.h              data types (Reading, icons, PowerStatus, ...) and the weather words / icons
//   timeutil.h           calendar math (the RTC keeps UTC, local time = UTC + the offset from the phone)
//   sensors.h            SHT40, RTC, battery / charging, the PM1 chip
//   sdcard.h             microSD card: detect (PM1 pin), mount on the bus shared with the panel, diagnosis
//   weather.h            Open-Meteo answer -> compact forecast -> what the screen shows (no hardware)
//   weather_fetch.h      one weather update: home Wi-Fi, HTTPS request, kinds of failure
//   ntp.h                the RTC corrected from the internet clock during a weather update
//   radio.h              switching the Wi-Fi radio off properly
//   wifi_ap.h, wifi_scan.h  the device's own Wi-Fi (access point) + Wi-Fi QR string; the list of nearby networks
//   settings.h           everything that is kept in flash (Wi-Fi, place, intervals, slideshow, LED)
//   gallery.h            the pictures on the card (/images, /masters, /thumbs, playlist), uploads
//   slideshow.h          which picture next, how to draw it (also turned portrait pictures)
//   battery.h            the "battery empty" rules (screen + switching off)
//   statusled.h          the status light (what the LED shows)
//   idlesleep.h          light sleep between events (battery)
//   ui.h                 all drawing code (header, pages, icons, battery screen)
//   web.h, index_html.h  the web server and the web page (tabs: Device / Weather / Upload Image / Image Gallery / Wi-Fi / LED / Instructions / Tests)
//
// More: ../README.md, the notes and plans in ../docs/FUTURE_IDEAS.md, the automatic tests in ../test.
// Developed with automated tests on a PC; see the README for what has and has not run on a device.

#include <M5Unified.h>
#include "sensors.h"
#include "sdcard.h"
#include "wifi_ap.h"
#include "wifi_scan.h"
#include "settings.h"
#include "weather.h"
#include "weather_fetch.h"
#include "web.h"
#include "slideshow.h"
#include "battery.h"
#include "idlesleep.h"

// ---- tweakables ------------------------------------------------------------
constexpr uint8_t  DISPLAY_ROTATION = 3;       // factory firmware uses 3 (600x400 landscape). Try 1 if upside-down.
constexpr bool     FORCE_SET_RTC    = false;   // true = overwrite RTC with build time on every boot (flash once, then set back to false)
constexpr bool     IDLE_SLEEP       = true;    // light sleep while there is nothing to do (battery only; see idlesleep.h). false = never sleep (for debugging)

// "Connect to Device" page / access point
constexpr const char* AP_SSID     = "M5 PaperColor";   // Wi-Fi name shown on the page and in the QR code
constexpr const char* AP_PASSWORD = "auto";            // "" = open network | "auto" = random password generated on first use
                                                       // and kept in flash | anything else = your own (min. 8 characters)

// ---- build profile -----------------------------------------------------------
// Both profiles fetch the REAL weather. They differ in how patient they are, and in what the
// screen shows while there is no real data (not set up yet, offline, ...):
//   TEST        short intervals; made-up DEMO data is shown instead of the error screen (the
//               "API last updated" line then says "demo data"), so the layout can always be seen
//   PRODUCTION  long intervals; real data only: without it the outdoor field shows
//               "Weather unavailable" with a sad little cloud and the reason
enum BuildProfile { PROFILE_TEST, PROFILE_PRODUCTION };
constexpr BuildProfile BUILD_PROFILE = PROFILE_TEST;   // <-- switch here

struct ProfileConfig {
  const char* name;
  bool        demoFallback;         // show made-up data while there is no real weather data
  uint32_t    redrawMs;             // how often the Weather page is redrawn (sensor, clock, battery)
  uint32_t    apIdleTimeoutMin;     // the Connect page closes after this long without activity
  uint16_t    fetchDefaultMin;      // weather fetch interval used until the user picks one
  uint16_t    fetchOptionsMin[4];   // fetch intervals offered on the settings page (minutes)
  uint16_t    slideDefaultMin;      // slideshow interval used until the user picks one
  uint16_t    slideOptionsMin[4];   // slideshow intervals offered on the settings page (minutes)
};
//                                      name          demo   redraw     AP idle  fetch default  fetch options (minutes)    slide default  slide options (minutes)
constexpr ProfileConfig PROFILE_TEST_CFG = { "TEST",       true,  60000UL,   3,       2,        { 1,  2,   5,   10 },        2,           { 1,  2,  5,   10 } };
constexpr ProfileConfig PROFILE_PROD_CFG = { "PRODUCTION", false, 10800000UL, 10,     180,      { 180, 360, 720, 1440 },     60,          { 60, 180, 720, 1440 } };   // fetch: 1 h, 3 h, 12 h, 24 h; slideshow: 15 min, 1 h, 3 h, 24 h
constexpr ProfileConfig CFG = (BUILD_PROFILE == PROFILE_TEST) ? PROFILE_TEST_CFG : PROFILE_PROD_CFG;
constexpr size_t FETCH_OPTION_COUNT = sizeof(CFG.fetchOptionsMin) / sizeof(CFG.fetchOptionsMin[0]);
constexpr size_t SLIDE_OPTION_COUNT = sizeof(CFG.slideOptionsMin) / sizeof(CFG.slideOptionsMin[0]);
// After a FAILED weather update (the retry never comes sooner than the fetch interval says, and never later than these):
constexpr uint32_t RETRY_SERVER_MIN  = 60;   // the server answered with an error / something unusable: try again after an hour
constexpr uint32_t RETRY_NETWORK_MIN = 0;    // the device could not get online (no Wi-Fi, no internet): 0 = do NOT try again by itself (it would only use up the battery):
                                             // it waits until the user connects it to a Wi-Fi network (saves the Wi-Fi / place settings) or presses G1. > 0 = try again after this many minutes
// ----------------------------------------------------------------------------

static M5Canvas canvas(&M5.Display);
#include "ui.h"      // must come after `canvas`

static Page        page               = PAGE_WEATHER;
static uint32_t    lastWeatherRefresh = 0;
static ConnectInfo connectInfo;
static int         apLastStations     = 0;
static bool        webRunning         = false;
static bat::Guard  batGuard;                   // looks at the battery while running (battery.h)
static uint32_t    batLastCheckMs     = 0;
static bool        batHalted          = false;   // the battery screen is up and the device has been told to switch off: nothing else runs
static bool        powerUsb           = true;    // external power was present at the last look (true until the first look: no sleep before we know)

// weather update scheduling
static uint32_t    lastFetchMs        = 0;
static bool        fetchedSinceBoot   = false;
static bool        lastFetchOk        = true;       // outcome of the last attempt
static char        lastFetchError[48] = "";         // its reason when it failed
static bool        forceFetch         = false;      // set when the settings changed
static wxfetch::Fail lastFetchFail    = wxfetch::Fail::NONE;   // what kind of failure the last attempt was (see weather_fetch.h)

// ---------------------------------------------------------------------------
//  Demo data (shown by the TEST profile while there is no real weather data)
// ---------------------------------------------------------------------------
// The label under "OUTDOOR" comes from the settings page (empty = not set yet).
static void applyLocationName() {
  const settings::Data& d = settings::data();
  snprintf(locationName, sizeof(locationName), "%s", (d.locationSet && d.place[0]) ? d.place : "");
}

// Called (from the web page code) after the settings were saved: a different place makes the old
// forecast useless, and any change triggers an update as soon as the Connect page is left.
static void onSettingsChanged() {
  applyLocationName();
  const settings::Data& d = settings::data();
  if (wx::valid(wxfetch::cache()) && !(d.locationSet && wx::sameLocation(wxfetch::cache(), d.lat, d.lon))) {
    wxfetch::cacheClear();
  }
  lastFetchOk = true;                 // an old failure was about the OLD settings
  lastFetchFail = wxfetch::Fail::NONE;
  lastFetchError[0] = 0;
  ntp::state() = ntp::State();        // (and the internet clock may be asked again at the next update)
  forceFetch = true;
}

static bool fetchConfigured() { return settings::data().locationSet && settings::wifiConfigured(); }

// In how many ms the next weather update is due: 0 = now, UINT32_MAX = never by itself (not set up, or waiting for the user).
static uint32_t fetchDueInMs() {
  if (!fetchConfigured()) return UINT32_MAX;
  if (forceFetch || !fetchedSinceBoot) return 0;
  uint32_t intervalMin = settings::data().fetchMin;
  if (!lastFetchOk) {
    uint32_t retryMin = RETRY_SERVER_MIN;
    if (lastFetchFail == wxfetch::Fail::NETWORK) {
      if (RETRY_NETWORK_MIN == 0) return UINT32_MAX;   // not online: wait for the user (settings saved, or G1)
      retryMin = RETRY_NETWORK_MIN;
    }
    if (retryMin < intervalMin) intervalMin = retryMin;
  }
  const uint32_t period = intervalMin * 60000UL, since = millis() - lastFetchMs;
  return since >= period ? 0 : period - since;
}
static bool fetchDue() { return fetchDueInMs() == 0; }

static void doFetch() {
  forceFetch = false;
  fetchedSinceBoot = true;
  Serial.println("Weather: updating...");
  sled::in().fetching = true;                          // blue / green pulses while the update runs (up to ~35 s)
  lastFetchOk = wxfetch::update(settings::data(), lastFetchError, sizeof(lastFetchError), &lastFetchFail);
  sled::in().fetching = false;
  lastFetchMs = millis();
  if (lastFetchOk) lastFetchError[0] = 0;
  else             sled::eventError(millis());         // red / yellow flashes for a few seconds
}

static void setSlot(ForecastSlot& s, WeatherIcon icon, float tempC) {
  s.valid = true;
  s.icon  = icon;
  s.tempC = tempC;
}

static void fillDemoWeather(Reading& r, int hourNow) {
  const bool night = (hourNow < 6 || hourNow >= 20);
  r.wxOk    = true;
  r.wxTempC = 18.6f;
  r.wxIcon  = iconFromWmo(2, night);               // partly cloudy
  snprintf(r.wxDesc, sizeof(r.wxDesc), "%s", describeWeather(2, 12));
  setSlot(r.morning,   iconFromWmo(2,  false), 14);   // partly cloudy
  setSlot(r.afternoon, iconFromWmo(0,  false), 21);   // clear
  setSlot(r.night,     iconFromWmo(61, true),  12);   // rain
  snprintf(r.apiTs, sizeof(r.apiTs), "demo data");
}

// ---------------------------------------------------------------------------
//  Read everything that goes on screen (sensor, battery, clock, weather)
//  The temperature / humidity sensor is only read for a page that shows it: the Connect page does not, so it is left alone there.
// ---------------------------------------------------------------------------
static Reading buildReading(bool withSensor) {
  Reading r;
  if (withSensor) r.shtOk = sensors::readSht40(r.tempC, r.humPct);
  r.power = sensors::readPower();

  sdcard::refresh();                           // a card put in / taken out since the last redraw is noticed here
  r.sd = sdcard::info().state;
  static SdState ledSdBefore = SD_ABSENT;      // a card that can not be read: red / yellow flashes, once, when it is noticed
  if (r.sd == SD_ERROR && ledSdBefore != SD_ERROR) sled::eventError(millis());
  ledSdBefore = r.sd;

  int hourNow = 12;
  sensors::LocalTime lt;                       // RTC holds UTC; this applies the offset sent by the phone
  if (sensors::readLocalTime(lt)) {
    r.rtcOk = true;
    hourNow = lt.hour;
    snprintf(r.date, sizeof(r.date), "%04d-%02d-%02d", lt.year, lt.month, lt.day);
    snprintf(r.sensorTs, sizeof(r.sensorTs), "%04d-%02d-%02d %02d:%02d:%02d",
             lt.year, lt.month, lt.day, lt.hour, lt.minute, lt.second);
  }

  // ---- outdoor weather: worked out from the forecast kept in flash ----
  const settings::Data& sd = settings::data();
  const wx::Cache& wc = wxfetch::cache();
  const bool cacheForHere = wx::valid(wc) && sd.locationSet && wx::sameLocation(wc, sd.lat, sd.lon);
  const bool haveWx = lt.ok && cacheForHere && wx::derive(wc, lt.epochUtc, r);

  if (cacheForHere && wc.fetchedAt) {                  // "API last updated", in the device's local time
    timeutil::Civil f = timeutil::civilFromEpoch((int64_t)wc.fetchedAt + (int64_t)sensors::tzOffsetMin() * 60);
    snprintf(r.apiTs, sizeof(r.apiTs), "%04d-%02d-%02d %02d:%02d:%02d", f.year, f.month, f.day, f.hour, f.minute, f.second);
  }

  if (!haveWx) {                                       // nothing to show: say why
    if (!sd.locationSet)                         snprintf(r.wxError, sizeof(r.wxError), "Location not set");
    else if (!settings::wifiConfigured())        snprintf(r.wxError, sizeof(r.wxError), "Home Wi-Fi not set up");
    else if (!lastFetchOk && lastFetchError[0])  snprintf(r.wxError, sizeof(r.wxError), "%s", lastFetchError);
    else if (!lt.ok)                             snprintf(r.wxError, sizeof(r.wxError), "Clock not set");
    else if (cacheForHere && lt.epochUtc < (int64_t)wc.firstHour)
                                                 snprintf(r.wxError, sizeof(r.wxError), "Clock looks wrong: sync it on Connect page");
    else if (cacheForHere)                       snprintf(r.wxError, sizeof(r.wxError), "Forecast expired");
    else                                         snprintf(r.wxError, sizeof(r.wxError), "No data received yet");
    if (CFG.demoFallback) fillDemoWeather(r, hourNow);   // TEST profile: made-up data instead of the error screen
  }
  if (r.wxOk && !lastFetchOk && lastFetchError[0]) {   // data on screen, but the last update failed
    snprintf(r.fetchNote, sizeof(r.fetchNote), "Update failed: %s", lastFetchError);
  }

  if (!withSensor)    {}                                // (not read on this page)
  else if (r.shtOk)   Serial.printf("SHT40: %.2f C, %.1f %%RH\n", r.tempC, r.humPct);
  else                Serial.println("SHT40 read failed");
  if (r.power.batOk) {
    Serial.printf("Battery: %u mV (~%d%%) | VIN: %u mV | src: %d | usb: %d | charging(inferred): %d\n",
                  r.power.batMv, batteryPercent(r.power.batMv), r.power.vinMv,
                  r.power.src, r.power.usb, r.power.charging);
  } else {
    Serial.println("Battery read failed");
  }
  return r;
}

// ---------------------------------------------------------------------------
//  Screen refresh. BLOCKING on purpose: the loop (and therefore the buttons) does
//  nothing until the panel has finished. Afterwards M5.update() is called once so
//  presses made during the refresh are discarded instead of acting afterwards.
//  (loop() handles only one button press per iteration and returns, so the
//  flushed state is never read by the same iteration.)
// ---------------------------------------------------------------------------
static void pushAndWait() {
  uint32_t t0 = millis();
  sled::in().refreshing = true;     // the status light blinks pink while the panel works (its task keeps running while we wait)
  canvas.pushSprite(0, 0);          // starts ONE panel refresh
  M5.Display.waitDisplay();         // blocks until the panel is done (can take many seconds)
  sled::in().refreshing = false;
  Serial.printf("Refresh took %lu ms\n", (unsigned long)(millis() - t0));
  M5.update();
}

static void renderWeatherPage() {
  Reading r = buildReading(true);
  drawScreen(r);
  pushAndWait();
  lastWeatherRefresh = millis();
}

// ---------------------------------------------------------------------------
//  Slideshow page: the pictures that are switched on, one after the other, each for the chosen time.
//  G1 = the next picture at once. Each picture is drawn on the whole screen (a portrait picture upright, with a border at the sides).
//  A picture made by the page's editor says in its note how to draw it (see slideshow.h): "panel" pictures are dithered by the panel
//  driver (epd_quality), pictures that are six colours already are drawn as they are (epd_fastest). The other pages use epd_text.
//  While the Slideshow page is open the weather is not updated (it is when the Weather page is opened again).
// ---------------------------------------------------------------------------
static slide::Picker slidePicker;
static uint32_t slideLastMs = 0;            // when the last picture (or message) was put on the screen
static uint32_t slidePollMs = 0;            // while a message is showing: when we last looked whether there is something to show now
static bool     slidePicture = false;       // a picture (not a message) is on the screen
static bool     slidePollable = false;      // the message on the screen is about something that can sort itself out (no card, nothing switched on): look again every few seconds
static char     slideShown[gallery::ID_MAX + 1] = "";
static char     slideMsgTitle[24] = "";     // the message on the screen (so that the same message is never drawn twice in a row: it costs a full panel refresh)

static uint32_t slideRandom() { return esp_random(); }

static void slideMessage(const char* title, const char* line1, const char* line2, bool pollable) {
  slideLastMs = slidePollMs = millis();
  slidePollable = pollable;
  if (!slidePicture && strcmp(slideMsgTitle, title) == 0) return;          // already on the screen
  Reading r = buildReading(false);
  drawSlideMessage(r, title, line1, line2);
  pushAndWait();
  slidePicture = false;
  slideShown[0] = 0;
  snprintf(slideMsgTitle, sizeof(slideMsgTitle), "%s", title);
  Serial.printf("Slideshow: %s\n", title);
}

// The whole picture file in memory (PSRAM), or nullptr if it is too big, there is no memory, or it can't be read. The caller frees it.
// (The picture is drawn from memory: drawPngFile(SD, ...) depends on the version of the M5GFX library, drawPng(data, length) does not.)
static uint8_t* slideLoad(const char* path, uint32_t* length) {
  File f = SD.open(path);
  if (!f) return nullptr;
  const size_t size = f.size();
  if (size < 24 || size > slide::MAX_FILE_BYTES) { f.close(); Serial.printf("Slideshow: %s can't be shown (%u bytes)\n", path, (unsigned)size); return nullptr; }
  uint8_t* buf = (uint8_t*)ps_malloc(size);
  if (!buf) { f.close(); Serial.println("Slideshow: no memory to read the picture"); return nullptr; }
  size_t got = 0;
  while (got < size) {
    const size_t k = f.read(buf + got, size - got);
    if (k == 0) break;
    got += k;
  }
  f.close();
  if (got != size) { free(buf); return nullptr; }
  *length = (uint32_t)size;
  return buf;
}

// A portrait picture, drawn turned a quarter turn so that it fills the landscape screen: it is drawn into a buffer of its own (the size
// of the picture) and then copied, turned, into the canvas. A plain copy, so every pixel is exact. TURN_NO_MEMORY: there is no memory for
// the buffer (the caller then draws the picture upright instead); a picture that can't be decoded is simply TURN_FAILED, as it would be upright too.
static slide::TurnResult slideDrawTurned(const uint8_t* data, uint32_t length, const slide::Layout& lay) {
  if (canvas.getColorDepth() != 16) return slide::TURN_NO_MEMORY;
  M5Canvas tmp(&M5.Display);
  tmp.setColorDepth(16);
  if (!tmp.createSprite(lay.dw, lay.dh)) { Serial.println("Slideshow: no memory to turn the picture, showing it upright"); return slide::TURN_NO_MEMORY; }
  tmp.fillScreen(C_WHITE);
  const bool ok = tmp.drawPng(data, length, 0, 0, 0, 0, 0, 0, lay.scale, lay.scale);
  if (ok) {
    const int srcStride = (int)(tmp.bufferLength() / (uint32_t)tmp.height() / 2);          // (from the buffer itself: rows may be padded)
    const int dstStride = (int)(canvas.bufferLength() / (uint32_t)canvas.height() / 2);
    slide::rotateQuarter((const uint16_t*)tmp.getBuffer(), srcStride, lay.dw, lay.dh, lay.clockwise, (uint16_t*)canvas.getBuffer(), dstStride, lay.x, lay.y);
  }
  tmp.deleteSprite();
  return ok ? slide::TURN_OK : slide::TURN_FAILED;
}

// Draws the picture (a PNG file in memory) on the canvas. False if it can't be drawn.
static bool slideDrawData(const uint8_t* data, uint32_t length, uint16_t w, uint16_t h, slide::Layout& lay) {
  lay = slide::layout(w, h, canvas.width(), canvas.height(), settings::data().portrait);
  canvas.fillScreen(C_WHITE);
  if (lay.rotate) {
    const slide::TurnResult tr = slideDrawTurned(data, length, lay);
    if (tr == slide::TURN_OK) return true;
    if (tr == slide::TURN_FAILED) return false;          // (it can't be decoded: nothing is gained by trying again upright)
    lay = slide::layout(w, h, canvas.width(), canvas.height(), slide::PORTRAIT_UPRIGHT);            // not enough memory: upright
    canvas.fillScreen(C_WHITE);
  }
  return canvas.drawPng(data, length, lay.x, lay.y, 0, 0, 0, 0, lay.scale, lay.scale);
}

// Puts one picture on the whole screen. False if its file can't be read or drawn (nothing has been pushed then).
static bool slideDraw(const gallery::Item& it) {
  char path[80];
  gallery::pathFor(gallery::DIR_IMAGES, it.id, "png", path, sizeof(path));
  uint32_t length = 0;
  uint8_t* data = slideLoad(path, &length);
  if (!data) return false;
  uint16_t w = 0, h = 0;
  slide::Layout lay;
  bool ok = gallery::parsePngHeader(data, w, h);
  const slide::DrawMode mode = slide::drawMode(data, length < slide::HEAD_BYTES ? length : slide::HEAD_BYTES);
  if (ok) ok = slideDrawData(data, length, w, h, lay);
  free(data);
  if (!ok) return false;

  M5.Display.setEpdMode(mode == slide::DRAW_FASTEST ? epd_mode_t::epd_fastest : epd_mode_t::epd_quality);
  pushAndWait();
  M5.Display.setEpdMode(epd_mode_t::epd_text);          // the other pages are drawn in this mode
  Serial.printf("Slideshow: %s (%ux%u, %s%s)\n", it.id, (unsigned)w, (unsigned)h, mode == slide::DRAW_FASTEST ? "drawn as it is" : "dithered by the panel",
                lay.rotate ? (lay.clockwise ? ", turned clockwise" : ", turned counter-clockwise") : "");
  return true;
}

// The next picture. (A picture that can't be drawn is skipped; if none can be, or there is none, or no card, a message says so.)
static void slideNext() {
  slideLastMs = millis();
  if (!gallery::ensureLoaded()) {
    if (sdcard::info().state == SD_ERROR) slideMessage("SD card error", "The card can't be read.", "See Connect to Device.", true);
    else                                  slideMessage("No SD card", "Put a card with pictures", "into the frame.", true);
    return;
  }
  for (int attempt = 0; attempt < 5; attempt++) {
    int k = slidePicker.next(gallery::lib(), settings::data().shuffle, slideRandom);
    if (k < 0) { slideMessage("No pictures", "Open Connect to Device,", "tab Images, to add some.", true); return; }
    const gallery::Item& it = gallery::lib().items[k];
    if (slidePicture && strcmp(it.id, slideShown) == 0) return;               // the only picture there is: it stays
    if (slideDraw(it)) {
      snprintf(slideShown, sizeof(slideShown), "%s", it.id);
      slidePicture = true;
      return;
    }
    Serial.printf("Slideshow: can't draw %s, skipping it\n", it.id);
  }
  slideMessage("Can't show them", "The pictures could not be", "read from the card.", false);       // tried again at the next interval, or with G1: not every few seconds
}

static uint32_t slideIntervalMs() {
  const uint32_t m = settings::data().slideMin ? settings::data().slideMin : CFG.slideDefaultMin;     // (never 0: that would redraw all the time)
  return m * 60000UL;
}

// The interval in the serial log, worded like the web page: "2 min", "1 h", "24 h".
static void slideIntervalText(char* out, size_t n) {
  const unsigned long m = slideIntervalMs() / 60000UL;
  if (m >= 60 && m % 60 == 0) snprintf(out, n, "%lu h", m / 60);
  else                        snprintf(out, n, "%lu min", m);
}

static void renderConnectPage() {
  Reading r = buildReading(false);                // no sensor reading: this page does not show it
  drawConnectScreen(r, connectInfo);
  pushAndWait();
}

// G1 (the top button, M5.BtnC): update the weather right now, then redraw once.
// Works only on the Weather page: on the Connect page the device's own Wi-Fi is on, and the radio is
// never used for that and for the home network at the same time. If nothing is set up yet, the update
// is still "attempted" so that its reason ("Location not set", ...) shows up on the screen.
static void manualWeatherUpdate() {
  if (page != PAGE_WEATHER) {
    Serial.println("G1 ignored: the weather can't be updated while the Connect page (own Wi-Fi) is open");
    return;
  }
  Serial.println("G1: weather update requested");
  doFetch();                          // also restarts the normal schedule from now
  renderWeatherPage();
}

// ---------------------------------------------------------------------------
//  Page handling
// ---------------------------------------------------------------------------
static void leavePage(Page p) {
  if (p == PAGE_CONNECT) {
    sled::in().apRunning = false;
    sled::in().apClient = false;
    if (webRunning) { web::end(); webRunning = false; }   // also switches the LED off
    ap::stop();
  }
}

// The battery is (nearly) empty: show the crossed-out battery and switch the device off (an e-paper keeps its last picture, so without this
// a dead battery looks like a crashed device). The caller has already stopped the web server / access point if they were running.
static void lowBatteryStop(const char* why) {
  Serial.printf("Battery empty (%s): showing the battery screen and switching off\n", why);
  sled::in().enabled = false;                          // no light while the battery is empty (the loop does not run again, so this stays; a manual colour is already off: leavePage() ended the web server)
  sled::tick(millis());
  drawLowBatteryScreen();
  pushAndWait();
  batHalted = true;
  if (!sensors::powerOff()) Serial.println("Power-off did not work (USB power?): waiting here");
}

// While running: look at the battery every 30 s; true = the device has just stopped.
static bool batteryGuardTick() {
  if (millis() - batLastCheckMs < bat::CHECK_MS) return false;
  batLastCheckMs = millis();
  const PowerStatus p = sensors::readPower();
  const bool stop = batGuard.update(p);
  if (batGuard.low > 0) Serial.printf("Battery low: %u mV (reading %u of %u)\n", (unsigned)p.batMv, (unsigned)batGuard.low, (unsigned)bat::LOW_READINGS);
  if (!stop) return false;
  leavePage(page);                                     // the web server and the access point use power too
  lowBatteryStop("battery low while running");
  return true;
}

// Every 5 s: what the power is doing, for the status light (USB, charging, charged, low battery).
static uint32_t ledPowerMs = 0;
static void ledPowerTick() {
  if (ledPowerMs != 0 && millis() - ledPowerMs < 5000) return;
  ledPowerMs = millis();
  const PowerStatus p = sensors::readPower();
  powerUsb = p.usb;
  sled::setPower(p, millis());
}

// ---- light sleep while idle (idlesleep.h) --------------------------------------------------------------------
static uint32_t msUntil(uint32_t since, uint32_t period) {          // how long until `period` ms have passed since `since` (0 = they have)
  const uint32_t el = millis() - since;
  return el >= period ? 0 : period - el;
}

// The next thing the page itself has to do, in ms from now. (The battery check every 30 s and the look at the power every 5 s need no deadline of
// their own: a sleep is never longer than 5 s, see idle::MAX_SLEEP_MS.)
static_assert(idle::MAX_SLEEP_MS <= 5000, "the power look (every 5 s) and the battery check (every 30 s) rely on the device waking up at least that often");
static uint32_t nextDeadlineMs() {
  uint32_t d = UINT32_MAX;
  auto soonest = [&](uint32_t x) { if (x < d) d = x; };
  if (page == PAGE_WEATHER) {
    soonest(msUntil(lastWeatherRefresh, CFG.redrawMs));
    soonest(fetchDueInMs());
  } else if (page == PAGE_SLIDESHOW) {
    if (slidePicture || !slidePollable) soonest(msUntil(slideLastMs, slideIntervalMs()));
    else                                soonest(msUntil(slidePollMs, 5000));       // a message about "nothing to show" looks again every 5 s
  }
  return d;
}

// At the end of an idle pass of loop(): sleep (true) until the next deadline, a button press, or at most 5 s; false = stay awake (see idlesleep.h).
static bool idleSleepNow() {
  if (!IDLE_SLEEP || powerUsb || page == PAGE_CONNECT || batHalted) return false;
  if (M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnC.isPressed()) return false;
  const uint32_t ms = idle::sleepMs(sled::quietForMs(sled::in(), millis()), nextDeadlineMs());
  if (ms == 0) return false;
  idle::lightSleep(ms);
  return true;
}

// After the battery screen nothing else runs. Normally the PM1 has cut the power by now. If it could not, the device waits here (trying
// again), and starts up again when USB power is there.
static void batHaltedTick() {
  delay(1000);
  if (sensors::readPower().usb) { Serial.println("USB power: starting again"); ESP.restart(); return; }
  sensors::powerOff();
}

static void enterPage(Page p) {
  page = p;
  if (p == PAGE_WEATHER) {
    Serial.println("Page: Weather");
    if (fetchDue()) doFetch();                         // before drawing, so the panel is refreshed only once
    renderWeatherPage();
  } else if (p == PAGE_SLIDESHOW) {
    { char iv[16]; slideIntervalText(iv, sizeof(iv)); Serial.printf("Page: Slideshow (interval %s%s)\n", iv, settings::data().shuffle ? ", shuffled" : ""); }
    slideShown[0] = 0;                                 // (the screen was something else: the picture, or the message, must be drawn again)
    slideMsgTitle[0] = 0;
    slideNext();
  } else if (p == PAGE_CONNECT) {
    Serial.println("Page: Connect to Device");
    String pw = ap::resolvePassword(AP_PASSWORD);

    wifiscan::run();                                   // BEFORE the AP: scanning would drop the phone's connection
    bool apOk = ap::start(AP_SSID, pw, connectInfo);
    connectInfo.timeoutMin = CFG.apIdleTimeoutMin;
    apLastStations = 0;
    if (apOk) {
      web::Config wc;
      wc.profileName       = CFG.name;
      wc.idleTimeoutMs     = CFG.apIdleTimeoutMin * 60000UL;
      wc.fetchOptionsMin   = CFG.fetchOptionsMin;
      wc.fetchOptionCount  = FETCH_OPTION_COUNT;
      wc.slideOptionsMin   = CFG.slideOptionsMin;
      wc.slideOptionCount  = SLIDE_OPTION_COUNT;
      wc.onSettingsChanged = onSettingsChanged;
      web::begin(wc);
      webRunning = true;
      sled::in().apRunning = true;                     // the status light pulses white while the access point waits for a phone
    }
    renderConnectPage();             // blocks until the panel is done; the screen then stays unchanged while the AP is up
    web::touch();                    // the idle countdown starts only now that the screen is ready
  }
}

static void gotoPage(Page p) {
  if (p == page) return;
  leavePage(page);
  enterPage(p);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  auto cfg = M5.config();
  cfg.clear_display = false;
  M5.begin(cfg);

  M5.Display.setRotation(DISPLAY_ROTATION);
  M5.Display.setEpdMode(epd_mode_t::epd_text);   // crisp text; use epd_quality for photos later

  canvas.setPsram(true);
  if (!canvas.createSprite(M5.Display.width(), M5.Display.height())) {
    Serial.println("canvas.createSprite FAILED - is PSRAM set to OPI PSRAM?");
    while (true) delay(1000);
  }
  initColors();

  Serial.printf("Build profile: %s (redraw every %lu s, fetch default %u min, AP idle %lu min, demo fallback %s)\n",
                CFG.name, (unsigned long)(CFG.redrawMs / 1000), (unsigned)CFG.fetchDefaultMin, (unsigned long)CFG.apIdleTimeoutMin,
                CFG.demoFallback ? "on" : "off");
  Serial.printf("Board id: %d, display %dx%d\n", (int)M5.getBoard(), M5.Display.width(), M5.Display.height());
  sensors::scanI2C();

  if (!sensors::initBattery()) Serial.println("PM1 (battery) init failed");

  {                                                    // started (power button) with a nearly empty battery: do not start up and brown out
    const PowerStatus p0 = sensors::readPower();
    if (p0.batOk) Serial.printf("Start-up battery: %u mV, USB %s\n", (unsigned)p0.batMv, p0.usb ? "yes" : "no");
    if (bat::bootRefused(p0)) { lowBatteryStop("too low to start"); return; }
  }

  sdcard::begin();                             // card-detect pins on the PM1; the first refresh() (before the first screen) mounts a card

  settings::load(CFG.fetchDefaultMin, CFG.fetchOptionsMin, FETCH_OPTION_COUNT);
  settings::fixSlideMin(CFG.slideDefaultMin, CFG.slideOptionsMin, SLIDE_OPTION_COUNT);
  applyLocationName();
  wxfetch::cacheLoad();
  if (wx::valid(wxfetch::cache()) && !(settings::data().locationSet && wx::sameLocation(wxfetch::cache(), settings::data().lat, settings::data().lon))) {
    wxfetch::cacheClear();                             // a forecast for some other place is of no use
  }
  Serial.printf("Weather cache: %s\n", wx::valid(wxfetch::cache()) ? "loaded from flash" : "none");
  Serial.printf("Settings: location %s, fetch every %u min, Wi-Fi \"%s\"%s\n",
                settings::data().locationSet ? settings::data().place : "not set", (unsigned)settings::data().fetchMin,
                settings::data().ssid, settings::data().pass[0] ? " (password set)" : "");

  sensors::loadTz();
  Serial.printf("Time zone offset: %+d min \"%s\"\n", sensors::tzOffsetMin(), sensors::tzName());

  if (!M5.Rtc.isEnabled()) {
    Serial.println("RTC not detected");
  } else if (sensors::ensureRtcTime(FORCE_SET_RTC)) {
    Serial.println("RTC was invalid/stale -> set from build time");
  }

  sled::applySettings(settings::data().ledStatus, settings::data().ledPct);
  { const PowerStatus p = sensors::readPower(); powerUsb = p.usb; sled::setPower(p, millis()); }
  sled::begin();                                       // the status light (a small task: its patterns keep running while we wait for the panel)

  enterPage(PAGE_WEATHER);
}

void loop() {
  if (batHalted) { batHaltedTick(); return; }
  M5.update();
  if (batteryGuardTick()) return;
  sled::applySettings(settings::data().ledStatus, settings::data().ledPct);
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) sled::eventButton(millis());     // one long white pulse
  ledPowerTick();

  // G9 = UP = previous page (M5.BtnB), G10 = DOWN = next page (M5.BtnA).
  // One press per iteration, then return (see pushAndWait()).
  if (M5.BtnB.wasPressed()) { gotoPage((Page)((page + PAGE_COUNT - 1) % PAGE_COUNT)); return; }
  if (M5.BtnA.wasPressed()) { gotoPage((Page)((page + 1) % PAGE_COUNT)); return; }
  if (M5.BtnC.wasPressed()) {                                                  // G1: the next picture on the Slideshow page, a weather update on the Weather page
    if (page == PAGE_SLIDESHOW) { Serial.println("G1: next picture"); slideNext(); }
    else                        manualWeatherUpdate();
    return;
  }

  if (page == PAGE_WEATHER) {
    if (fetchDue()) {                                  // fetch, then draw (blocks for a few seconds)
      doFetch();
      renderWeatherPage();
    } else if (millis() - lastWeatherRefresh >= CFG.redrawMs) {
      renderWeatherPage();
    }

  } else if (page == PAGE_SLIDESHOW) {
    if (slidePicture || !slidePollable) {
      if (millis() - slideLastMs >= slideIntervalMs()) slideNext();
    } else if (millis() - slidePollMs >= 5000UL) {      // a message about "nothing to show" is up: has a card been put in, or a picture switched on, meanwhile?
      slidePollMs = millis();
      if (gallery::ensureLoaded() && slide::anyToShow(gallery::lib())) slideNext();
    }

  } else if (page == PAGE_CONNECT) {
    if (webRunning) web::loop();     // serve the web page

    // Activity = something the user did on the page (see web.h) or a phone joining/leaving.
    int n = ap::stationCount();
    sled::in().apClient = (n > 0);                   // ... and shines steadily white once a phone is connected
    if (n != apLastStations) {
      Serial.printf("AP clients: %d -> %d\n", apLastStations, n);
      apLastStations = n;
      web::touch();
    }
    if (millis() - web::lastActivity() >= CFG.apIdleTimeoutMin * 60000UL) {
      Serial.println("AP idle timeout -> back to Weather page");
      gotoPage(PAGE_WEATHER);
      return;
    }
    delay(2);                        // keep the web server responsive
    return;
  }

  if (!idleSleepNow()) delay(20);                      // nothing to do for now: sleep until something has to happen (on a charger: just wait a moment)
}
