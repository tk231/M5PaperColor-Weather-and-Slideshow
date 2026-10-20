# PaperColorFrame - future ideas (NOT being built yet)

Notes so nothing gets lost. None of this is implemented; the current focus is
the build order below.

## Planned features, deliberately postponed

### 1. Status LEDs (blinking patterns)
Show device state with the onboard RGB LED:

| State          | Idea (not decided)                       |
|----------------|------------------------------------------|
| Charging       | slow pulse / steady dim color            |
| Fetching API   | fast blink while the request is running  |
| Button press   | short flash as feedback                  |
| Low battery    | slow red blink                           |

Things to look at when we get there:
- The factory firmware already drives the LED through `M5.Led` (see
  `main/apps/app_manager/app_manager.cpp`, the `M5.Led.display()` calls). M5Unified
  powers the LED rail through the PM1 (`setLDOOutput(true)` in the PaperColor
  init), so check that the rail stays on.
- Make it a small non-blocking state machine driven by `millis()` (no `delay()`),
  with a priority order when several states are active at once.
- The LED draws power. If the device later sleeps / powers off between updates,
  blinking only makes sense while awake. Decide whether low-battery blinking is
  worth the drain.

### 2. Button actions
**Done:** G1 (the top button, `M5.BtnC`) updates the weather right now and redraws once; it works
on the Weather page only (on the Connect page the device's own Wi-Fi is on). The normal schedule
restarts from the press. If nothing is set up yet, the press still reports why.

Still free for more actions (each would need a decision): a long press or double-click of G1,
G9 or G10 (`wasHold()`, `wasDoubleClicked()`), for example "redraw the screen without fetching",
or "fetch and redraw" on another button. Mapping per M5Unified: `M5.BtnA` = G10 (GPIO10),
`M5.BtnB` = G9 (GPIO9), `M5.BtnC` = G1 (GPIO1). Presses made while a screen refresh or a weather
update is running are discarded on purpose.

### 3. microSD presence icon + degraded mode
**Done:** a small card symbol sits to the left of the date in the header (white card = mounted,
yellow card with "!" = a card is inserted but can't be read, outlined card struck through in red =
no card). The weather, the sensor and the screen do not depend on the card at all (the forecast
lives in internal flash), so everything keeps working without one.

**Still to do when those pages exist:** the gallery and slideshow pages must say "No SD card" /
"Card can't be read" instead of failing, and show used / free space (see the note on cost below).

### 4. README for the project
Write a `README.md` for the whole project (to do later). It should cover:
- what the device does (weather page, Connect-to-Device page, buttons G9 / G10 / G1, status icons);
- the Arduino IDE setup: board settings (ESP32S3 Dev Module, 16 MB flash, OPI PSRAM, USB CDC On Boot),
  the libraries to install (M5Unified, M5GFX, M5PM1; WebServer / WiFi / SD / Preferences come with the
  ESP32 core), and the file list of the sketch folder;
- the tweakables at the top of `PaperColorFrame.ino` (display rotation, AP name and password, TEST /
  PRODUCTION profile and what each changes);
- first-time setup from a phone (join the device's Wi-Fi from the QR code, open the page, sync the clock,
  set location and home Wi-Fi) and what the screen shows when something is missing;
- the microSD card formats (see the note below: FAT32 / FAT16 yes, exFAT no) and how to test the card;
- how the project was checked (the test suites) and the known limits.

## Web page: two tabs for pictures (replaces the single "Images" tab)
- **Upload Image**: the photo chooser and the editor, always visible (the "+" button is gone). **Image Gallery**: the picture list (the
  "In slideshow" / "Hidden" switch of a picture is how pictures are selected), the slideshow settings (interval, shuffle, portrait mode), Rescan.
  Tabs: Device, Weather, Upload Image, Image Gallery, Wi-Fi, LED, Tests. Addresses `#upload` and `#gallery`; the old `#images` opens the gallery.
- Flow: Edit (in the gallery) opens the editor in the Upload Image tab. A finished save goes to the Image Gallery with "Added ..." / "Saved ..." -
  but only if the person is still on the Upload Image tab (if they moved to another tab during the save they are left there, and the message
  is waiting in the gallery). Cancel of a new photo goes back to the photo chooser; Cancel of an edit goes back to the gallery. Switching tabs
  while editing keeps the edit. The gallery list is only requested when the Image Gallery tab is opened.
- The result of something the person did (a "saved" message) is not wiped by the gallery's own refresh; only status lines it wrote itself
  (no card, empty list) are taken away by the next refresh (`galStatus` / `galSay` in the page script).
- Not built (asked, then "keep the switch for now"): "select all / select none" for the slideshow.

## Web page: Instructions tab (for the end user)
- A tab "Instructions" (before Tests): plain static text, no device requests: how the screen behaves (e-paper keeps its picture), the three
  pages, the G10 / G9 / G1 buttons, the Power button (single = restart, double = off, long = programming mode, any press when off = on),
  battery and the crossed-out battery, setting it up from a phone, adding / choosing pictures, the SD card, what to do when something fails.
- It must be kept in sync with the code. Two checks guard that: the page suite (every tab name and button label the text mentions, marked
  `<b class="t">` / `<b class="u">`, must exist exactly in the page) and `check_instructions.py` (the claims about buttons, page order, G1,
  the weather during the slideshow ... are compared with the sketch). Change a button or a label -> these fail until the text is updated.
- The Power-button text comes from the PM1 datasheet; single and double click were confirmed on the device by the user, the LONG PRESS (programming
  mode, and leaving it by double click + one press) is NOT yet confirmed on the device.
- The README (item 4 above) is still to do; the Instructions tab covers the end-user part of it.

## Picture editor: Floyd-Steinberg is the default colour method (changed Oct 2026, by request)
- A new photo starts with "Colours: Floyd-Steinberg" (it was "Panel"). The list is Floyd-Steinberg (default), Panel (as the factory page), Nearest colour.
  The default lives in one place: `sanitize()` in the PCImg library of the page: a missing or unknown value becomes "fs"; an explicit "panel", "nearest" or
  "fs" (for instance in the note of a picture edited earlier) is KEPT. "Reset" in the editor goes back to Floyd-Steinberg as well.
- Effect on the device: the saved picture is already reduced to the six pure colours in the browser and its note says "fs", so the slideshow draws it as it is
  (`epd_fastest`), not through the panel driver's own dithering (that is what "Panel" pictures get: `epd_quality`).
- The editor has to run the error diffusion for the final preview and for saving (240 000 pixels): it is done after a short pause, not at every slider step.
  If it feels slow on the phone, that is where to look.
- Pictures that already are on the card keep their own method (it is stored in their note); only pictures without a note, or a note without a method, get the new default.

## Timings, light sleep and failures - BUILT (Oct 2026; not yet measured on the device)
Battery: 1250 mAh. Asked for by the user: nothing below an hour (an e-ink display gains nothing from it), everything in hours, the ESP32 still responsive.
- PRODUCTION timings (the TEST build keeps its short ones for developing): the Weather page changes every **3 h** (`redrawMs`); the weather is updated every
  **3 h** (default), 6 h, 12 h or 24 h; the pictures change every **1 h** (default), 3 h, 12 h or 24 h; the Connect page closes after 10 min. The web page shows
  all of them in hours ("3 h"). A stored interval that is not one of the new options becomes the default (old 15 min / 10 min settings do).
  `check_profile.py` pins these numbers as literals.
- LIGHT SLEEP (`idlesleep.h`): at the end of an idle pass of loop() the device falls into light sleep (CPU stopped, memory and screen kept) for at most 5 s and
  never past the next deadline (a redraw, an update, the next picture, a low-battery flash). It wakes at once on a button (GPIO 10, 9, 1: pressed = low, checked in
  M5Unified for board_M5PaperColor) or when the time is up. It NEVER sleeps: on a charger (the serial monitor and the charging light need it awake), on the Connect
  page, while a button is held, or while the LED shows something (the task that animates it would sleep too: `sled::quietForMs()`). `IDLE_SLEEP` at the top of the
  sketch switches it off (for debugging). The sleep is at most 5 s because the power look (5 s) and the battery check (30 s) rely on it (`static_assert`).
  With the status light switched off the charger / Connect-page checks in the code are what keep it awake (tested separately: the lights alone hid them).
- NOT yet tried on the device (all from the ESP-IDF documentation, the PC tests use stand-ins): `gpio_wakeup_enable()` + `esp_sleep_enable_gpio_wakeup()` +
  `esp_light_sleep_start()` with the OPI PSRAM, `millis()` continuing through the sleep (it does in ESP-IDF), the e-paper and the SD card keeping their state, the
  LED task and the M5 I2C bus after a wake. Things to look at first if it misbehaves: buttons that react late (the wake level), a frozen LED, a slow wake.
- BATTERY ESTIMATE (assumptions, to be replaced by a measurement): light sleep ~2 mA (1-4 mA possible) = 48 mAh/day; a wake every 5 s ~1.4 mAh/day; a weather update every
  3 h ~0.35 mAh each = 2.8 mAh/day; a picture every hour ~0.13 mAh each = 3.2 mAh/day. About 55 mAh/day, so about 3 weeks (1 mA: ~6 weeks, 4 mA: ~12 days).
  The sleep current is what decides. If it is too high: power the ESP32 completely off between updates (the factory way, see "Longer battery life" above:
  months, but the buttons then only work after a Power press).
- WHEN THE WEATHER UPDATE FAILS (`wxfetch::Fail`): SETUP (no place / no Wi-Fi set up), NETWORK (could not join the Wi-Fi, or could not reach the server: not online),
  SERVER (the server answered with an error or something unusable). NETWORK: the device does NOT try again by itself (`RETRY_NETWORK_MIN = 0`; it would only use up
  the battery) and sleeps until the user connects it to a Wi-Fi network (saving the Wi-Fi / place settings) or presses G1; the screen keeps the red note with the
  reason. SERVER: tried again after an hour (`RETRY_SERVER_MIN = 60`, never sooner than the fetch interval says). A fresh start always tries once. Set
  `RETRY_NETWORK_MIN` to e.g. 60 to let the device try hourly after a power cut (the router is often slower to start than the frame: with 0 the frame waits for the user).
- THE INTERNET CLOCK (NTP) is asked at the start of an update; no answer = not asked again for 24 h (it would wait 3 s each time); it does not decide whether the
  device is online (some networks block it): the weather request does. Saved Wi-Fi / place settings reset it.
- THE CHARGING LIGHT glows for as long as the charger is connected (yellow pulse while charging, steady green when charged); on a charger the device does not sleep.

## Wind icon - BUILT (Oct 2026)
- `ICON_WIND` (three blue wind streaks with curls, drawn in `drawWeatherIcon`). One rule for the word and the icon: `isWindy()` in types.h = a SKY code
  (0-3: clear ... cloudy) with a wind of at least `WINDY_KMH` (40 km/h). Rain, fog, snow and thunder keep their own icon and word.
- Now: `iconFor(code, wind, night)`. Morning / Afternoon / Night: `wx::slotIcon()`: a sky icon becomes the wind icon when the AVERAGE wind of the hours
  that have a value is at least 40 km/h (a single gusty hour does not count). Tests: 76 in the weather logic (the word and the icon agree for every code and wind).

## Clock from the internet (NTP) - BUILT (Oct 2026)
- `ntp.h`: during a weather update, right after the home Wi-Fi is up and BEFORE the forecast is requested (so the forecast is read against the right
  time), the device asks SNTP (pool.ntp.org, then time.google.com, time.cloudflare.com) for the UTC time and corrects the RTC (which keeps UTC).
- When: at the first update after a start, then at most every 12 h; after NO answer it waits 24 h before asking again (so a network that blocks SNTP does not
  slow every update by 3 s), and a new Wi-Fi / place setting resets that at once; an RTC that was never set is tried at every update. At most 3 s of waiting. A difference below 2 s is left alone (the RTC
  is not rewritten for nothing); an answer that is not a real time (before 2023-11-14, 2100 or later) is ignored.
- The system clock is reset to 1970 before each question, so that "the clock is valid" can only mean "SNTP has just answered".
- Serial log: `NTP: the RTC was 12 s fast: corrected` / `NTP: the RTC is right (1 s apart)` / `NTP: no answer in 3000 ms`.
- NOT tried on the device yet: `configTime()`, `getLocalTime()` and `settimeofday()` come from the Arduino core (stand-ins on the PC). Test programs that
  trigger a weather update must replace the question (`ntp::queryHook()`), otherwise the first update waits 3 s.

## Status LEDs - BUILT (Oct 2026; not yet seen on the device)
Files: `statusled.h` (the whole module), hooks in the sketch and in web.h, two settings (`ledStatus`, `ledPct`), the "Status light" part of the LED tab, the
"The status light" part of the Instructions tab.
- Patterns, as chosen by the user (colours are RGB at full intensity, then scaled by the brightness setting, default 15 %):
  | state | pattern |
  | weather update failed, or an SD card that can not be read | RED and YELLOW flashing in turns (200 ms red, 100 dark, 200 yellow, 100 dark) for 10 s |
  | button press (G10, G9, G1) | ONE long white pulse (1.2 s) |
  | the panel refreshes | PINK blinking, 250 ms on / 250 ms off |
  | the weather is fetched | BLUE pulse (1 s), then GREEN pulse (1 s), over and over |
  | a picture is uploaded | fast BLUE blink (100 ms), until 1.5 s after the last data |
  | battery low (<= 15 %, not on USB) | one short RED flash (150 ms) every 30 s |
  | the Connect page (own Wi-Fi) | slow WHITE pulse (3 s); steady WHITE while a phone is connected |
  | charging (USB) | slow YELLOW pulse (3 s); steady GREEN when charged; both for as long as the charger is connected (changed Oct 2026: it was the first 10 minutes) |
- PRIORITY (first that applies): error > button > refreshing > fetching > uploading > low battery > access point > charging/charged. Activity is above the
  ambient states on purpose (the earlier proposal had charging above the activity: then nothing would show on a charger). A manual colour from the LED tab
  wins over everything while it is on ("Off" gives the status light back).
- HOW: a small FreeRTOS task (core 0, priority 1) calls `sled::tick()` every 40 ms, so the patterns keep running while the main program is BLOCKED (a panel
  refresh, a weather update). `compute()` is a pure function of the inputs and the time. The LED is only written when its colour or brightness changed (no
  I2C traffic while it stays as it is). The factory firmware also uses a separate task for its LED states.
- Events are timers judged by the time that has passed (right across the 49-day wrap of `millis()`), and every tick clears the ones that are over: an
  earlier version compared "time left" as a signed number and an old event would have come back to life after 24.8 days of uptime (found by the tests).
- Settings: `ledStatus` (on/off) and `ledPct` (5..100 %), strict like the others (400 for bad values, all-or-nothing, no weather refetch), saved in flash.
- Charging / charged is INFERRED from the voltage (USB present and below 4150 mV = charging; from 4150 mV = charged; back to charging only below 4100 mV, no
  flicker). The same caveat as the plug symbol on the screen.
- The light is switched off when the battery is empty (before the battery screen) and the task is not even started when the start-up is refused.
- NOT yet seen on the real device: the colours as the LED really shows them (the pink may look reddish at low brightness), the smoothness of the pulses (the
  LED is updated 25 times a second over I2C while it pulses), and that the LED task and the main program can share the I2C bus without trouble (the factory
  firmware does the same). Tune the colours/timings at the top of statusled.h.

## Serial log
- Entering the Slideshow page prints `Page: Slideshow (interval 2 min)` (also "1 h", "3 h", "24 h"; ", shuffled" is added when shuffle is on).
  G1 prints just `G1: next picture` (no interval there, by request).

## Power, battery and shutdown (asked for in Oct 2026)

### 1. Empty-battery screen - BUILT, run on the device by the user (Oct 2026): works
- `battery.h` (pure logic), `drawLowBatteryScreen()` in ui.h (a big battery crossed out in red, nothing else), `sensors::powerOff()` = `pm1().shutdown()`
  (the factory firmware's `Hal::powerOff`), and in the sketch `lowBatteryStop()`, `batteryGuardTick()`, `batHaltedTick()`.
- While running (every 30 s): two low readings IN A ROW (<= 3350 mV, on battery, readable) -> the web server / access point are stopped, the screen
  is drawn, the PM1 is told to switch the device off. USB power or an unreadable battery never stops it.
- At start-up (Power button): on battery and below 3500 mV -> it does NOT start (no Wi-Fi, no fetch): it draws the screen and switches off
  again. 3500 is higher than 3350 on purpose: after a switch-off the voltage recovers a little. With USB power it always starts (and charges).
  The factory firmware has the same gate (it switches off below 3100 mV right after PM1 init). Our sketch had NONE, which very probably is why the
  device "did not boot properly" after a low-power shutdown: it started at a nearly flat battery and browned out while starting Wi-Fi.
- If the PM1 can not cut the power (USB after all) the device waits, tries again every second, and restarts when USB power is seen.
- Confirmed on the device by the user (Oct 2026). The thresholds are the constants at the top of battery.h, if they ever need tuning.

### 2. Weather screen with more colours - BUILT, run on the device by the user (Oct 2026): works
- Icons: the sun has red rays and a red outline round the yellow disc; clouds are white with a BLUE outline; "cloudy" has a solid blue cloud behind
  a white one; the storm cloud is solid blue with the yellow bolt; fog lines and the moon's stars are blue; the sad "no data" cloud stays black.
- Layout: the titles OUTDOOR (blue) and INDOOR (red) match the rule under them; Morning / Afternoon / Night sit on colour chips (yellow, red, blue);
  the TOMORROW tag is blue; (a humidity gauge was tried and removed again: not wanted); temperature digits are blue at or below 0 C and red at or above 30 C, black otherwise (a colour only where it says something).
- All six inks are used (black, white, yellow, red, blue, green). Yellow is only used for fills (a yellow letter on white is hard to read).
- Easy to change: `tempColor()` (limits), the chip colours in the three `drawForecastSlot(...)` calls, the colours in `drawWeatherIcon()`.

### 3. Longer battery life (it dies within 24 h today, also in PRODUCTION)
- Cause: `loop()` never sleeps (`delay(20)` at 240 MHz, forever, plus an I2C poll each pass). Nothing is switched off between events.
- BEST (what the factory firmware does): power the ESP32 COMPLETELY OFF between updates and let the RTC wake the PM1 chip.
    * Before switching off: `pm1.gpioSetFunc(M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_WAKE)`, `gpioSetPull(.., M5PM1_GPIO_PULL_UP)`,
      `gpioSetWakeEdge(.., M5PM1_GPIO_WAKE_FALLING)`, `gpioSetWakeEnable(.., true)`; set the RX8130 alarm `M5.Rtc.setAlarmIRQ(&tm)` (a time, one
      minute resolution: `tm_min += interval`); clear old flags (`pm1.getWakeSource(&src, M5PM1_CLEAN_ALL); M5.Rtc.clearIRQ(); pm1.timerClear()`);
      then `pm1.sysCmd(M5PM1_SYS_CMD_OFF)` / `shutdown()`. The PM1 itself keeps running (microamps).
    * At start: `pm1.getWakeSource(&src, M5PM1_CLEAN_NONE)`: `M5PM1_WAKE_SRC_EXT_WAKE` (0x20) = the RTC alarm -> do one cycle (draw the next
      picture / fetch the weather and draw), schedule the next alarm, switch off again. `M5PM1_WAKE_SRC_PWRBTN` (0x04) = Power button, `_VIN` (0x02)
      = USB plugged in, `_TIM` (0x01) = PM1 timer -> "interactive" start: stay awake while the person uses it, and switch off after an idle time
      (factory: 60 s without activity and no phone on the access point).
    * Wake-up cost: about 5 s at ~60 mA = ~0.08 mAh. Hourly: ~2 mAh/day; every 15 min: ~8 mAh/day; EVERY MINUTE (a clock): ~115 mAh/day. So the
      Weather page's clock/sensor redraw (`redrawMs`) is what decides the battery life. Decide how often the Weather page really has to change
      (a clock on e-paper wants a wake per minute: probably not worth it; 15-60 min is).
    * Consequence: the G9/G10/G1 buttons only work while the device is awake (after a Power press, or at an update). The slideshow then changes
      at the alarm times instead of by a timer in loop(). Needs: a boot-reason switch in setup(), "one-shot" cycles, an idle timeout, and the
      current screen state kept in NVS/RTC (what was shown last, the shuffle bag) because RAM is lost at every power-off.
- CHEAPER, smaller step: light sleep in the idle part of loop() (CPU stopped, RAM kept, buttons wake it by GPIO; ~1-3 mA instead of ~15-40).
  Only on battery (USB-serial drops during light sleep, so never with USB). Read each button pin's idle level before sleeping and wake on the
  OPPOSITE level (no assumption about polarity). Sleep at most until the next deadline (redraw, fetch, slideshow, poll). Not for the Connect page.
- Quick wins to try: a longer `redrawMs` in PRODUCTION; `setCpuFrequencyMhz(80)` while idle; fewer I2C polls.
- FIRST MEASURE: a USB power meter in each state (Weather idle, a fetch, the access point, a refresh) so that the effort goes where the current is.

### 4. A real shutdown / start-up sequence for the Power button  (explained; the "switched off" page is NOT planned for now)
- WHAT THE BUTTON DOES TODAY (hardware, in the PM1 chip; datasheet section "PWR_BTN"; our firmware never looks at it): single click = RESET
  (reboot), double click = POWER OFF, long press (1 s default, `BTN_CFG_1`) = DOWNLOAD MODE; while off, any press powers on. On an e-paper none of
  these shows anything: a reset on the Weather page redraws the same page, a power-off keeps the last picture, download mode looks frozen.
- LOW-VOLTAGE PROTECTION IN THE PM1: `BATT_LVP` (0x08) defaults to 0x40 = 2000 + 64 x 7.81 = 2.5 V. Below it the chip cuts the power, and it
  powers the device on AGAIN as soon as the battery is 100 mV higher (or 5V is plugged in). Between ~2.6 V and ~3.0 V the ESP32 can start but
  browns out: a boot loop, which is the "does not boot properly after a low-power shutdown" of the first versions. Idea (one call at start-up, like
  setChargeEnable): `pm1().setBatteryLvp(3300)`: the chip itself then cuts at 3.3 V and restarts at ~3.4 V, a hardware back-up for battery.h.
  A shutdown that the firmware asks for (`shutdown()`) stays off until a button press / USB; only the chip's own LVP shutdown restarts by itself.
- A "SWITCHED OFF" PAGE (wanted: text on how to switch on and what the buttons do). Mock: `poweroff_page_mock.png` (power symbol; "Switched off";
  "To switch on: press the Power button once."; the buttons G10 / G9 / G1 / Power double-click). No date or battery on it: they would be stale.
  How: the chip cuts the power the moment it recognises a double click, so the firmware must take that over:
    1. `pm1().setDoubleOffDisable(true)` (the chip no longer powers off by itself),
    2. notice the double click in firmware, draw the page, then `sensors::powerOff()`.
  Noticing it: the IRQ status registers (0x40-0x42) are cleared by the chip unless some PM1 GPIO is configured as the IRQ pin (datasheet), so the
  simple way is polling `btnGetState()` (BTN_STATUS 0x48) every 20 ms in loop() and timing the presses ourselves.
  THE RISK: the chip's single-click RESET can fire while we are still deciding "single or double" (how the chip's click/double windows in
  BTN_CFG_1 are measured is not clear from the datasheet). Two ways out: (a) also `setSingleResetDisable(true)` and handle ALL clicks in firmware
  (a single click could then do something visible, e.g. redraw the page) - but then the button can no longer rescue a HUNG firmware (only the
  long press into download mode, and a USB reflash resets it), so add a software watchdog first so that a hang reboots itself; or (b) measure the
  real timing and keep the hardware reset.
  Also: a press during a long blocking step (a panel refresh of up to ~5 s, a Wi-Fi fetch of up to ~20 s) is only noticed afterwards, and a
  double click can not be rebuilt from the "was pressed" flag alone.
  DECISION (Oct 2026): the "switched off" page is not planned for now, and there is no diagnostic build. The Instructions tab explains what each press does.
- The factory firmware also does `i2cBusRecovery()` when `esp_reset_reason() != ESP_RST_POWERON`, and sets `pm1.setBoostEnable(true)` and the EPD enable
  pin; we do neither. If odd boots after a brown-out remain, look there.

## Lessons about what a host test can NOT see (check these when something fails on the real compiler)
- The Arduino IDE writes a prototype for every function of the .ino near the top of the file. A function must not use, in its signature, a type
  that is defined in the .ino (put it in a header) and must not have default arguments. `check_ino.py` checks this (it flagged the failure
  at the very line the real compiler reported). The PC tests compile the sketch as plain C++ and never see it.
- The M5GFX library on the PC (read from GitHub) is not the version installed on the user's computer. Prefer calls that exist in every version
  (`drawPng(data, length, ...)`, `createSprite`, `getBuffer`) over version-dependent templates (`drawPngFile(fs, ...)`). When a call is
  new, say so and ask the user to compile before building more on it.

## Open decisions
- **Wi-Fi flow: DECIDED (option C).** The device joins the home network by itself
  to fetch Open-Meteo, and runs its own access point only while the user is on the
  "Connect to Device" page. Never both at once. All image work (decode, crop,
  rotate, dither) happens on the phone.
- **Connect page: DECIDED.** The e-paper stays on the Connect page (QR code +
  numbered instructions) the whole time the AP is up, so there are no refreshes in
  AP mode. Instructions: 1) join Wi-Fi "<AP_SSID>" (scan the QR), 2) open the URL.
  AP name, password and URL are tweakables. Leaving: any button, or an inactivity
  timeout (default 10 min since the last HTTP request, tweakable).
- **Buttons: DECIDED.** G9 = UP (previous page, `M5.BtnB`), G10 = DOWN (next page,
  `M5.BtnA`). A screen refresh blocks the button loop; button state is flushed
  after the refresh so a held button doesn't trigger a second switch.
- **Location and time come from the phone (DECIDED, diagnostics confirmed).** The
  device never looks up a location. Diagnostics on the real iPhone (iOS 18.7, Firefox
  for iOS, which uses WebKit like Safari):
    - the AP page is plain http, so it is NOT a secure context and geolocation is
      refused (code 1) - automatic "use my location" is not possible;
    - Open-Meteo's weather API and geocoding (city search) are reachable from the
      page, with browser (CORS) access, WHILE joined to the device's Wi-Fi (the
      iPhone sends internet traffic over mobile data).
  So the settings page sets the location by city search (Open-Meteo geocoding, needs
  mobile data on the phone) or by typing latitude / longitude (works offline), plus a
  label. The device just stores lat / lon / label.
- **Option A is now proven possible on this iPhone** (the phone could download the
  forecast and push it to the device), but we stay on option C: the device fetches by
  itself over the home Wi-Fi, so it keeps refreshing without anyone opening the page.
  A could still be added later as a fallback.
- **Web server library (step 3): DECIDED.** The built-in `WebServer` (no extra
  library). It runs inside `loop()`, so M5Unified, I2C and the LED are only ever
  touched from one task. Revisit ESPAsyncWebServer only if the gallery / upload
  steps turn out too slow.
- **Clock (step 3): DECIDED.** The RTC holds UTC; the phone sends its UTC offset
  (minutes) and zone name, stored in flash; local time = UTC + offset. Daylight-saving
  changes are not followed automatically until the next phone sync (idea for later:
  take `utc_offset_seconds` from each Open-Meteo response).
- **Idle timeout counts only real interaction** (opening the page, clock sync, LED
  changes, a phone joining/leaving). The page's own status polling does not, so a
  page left open in a pocket cannot keep the Wi-Fi alive. The LED is switched off
  whenever the page is left.
- **Web page layout: DECIDED.** Five tabs (Device / Weather / Wi-Fi / LED / Tests), one
  section each; the open tab is kept in the URL (#weather, ...). Room for an Images tab later.
- **Wi-Fi list: DECIDED.** The device scans ONCE when the Connect page is opened, just
  before its own access point starts (scanning while the AP runs would drop the phone).
  The result is kept in memory and shown on the Wi-Fi tab (strongest first, duplicates
  merged, hidden networks skipped, 20 max). To refresh the list: leave the Connect page
  and open it again. Cost: a few seconds before the Connect page appears (the serial
  log prints "Wi-Fi scan: ... ms"). The ESP32 only sees 2.4 GHz networks. A hidden
  network can be typed in by hand.
- **Home Wi-Fi credentials:** cannot be tested while the AP is up (the radio is busy).
  They are used after leaving the Connect page; failures will be reported on the Weather
  page ("Weather unavailable" + reason). The password is write-only (never sent back to
  the page, never logged) but is stored as plain text in the device's flash (NVS is not encrypted).
- **Profiles: DECIDED.** One block in the sketch (`PROFILE_TEST_CFG` / `PROFILE_PROD_CFG`)
  holds everything that differs: demo weather on/off, Weather page redraw interval, AP idle
  timeout, default fetch interval and the fetch intervals offered on the settings page.
    TEST        demo data | redraw 60 s | AP idle 3 min | fetch 1 / 2 / 5 / 10 min (default 2)
    PRODUCTION  real data | redraw 10 min | AP idle 10 min | fetch 15 / 30 / 60 / 180 min (default 30)
  If the stored fetch interval is not offered by the profile in use (e.g. after switching
  profile) the profile's default is used. The page shows a TEST badge in the TEST profile.
  The fetch interval is stored and offered now; the fetch itself is the next step.
- **Place names are ASCII only** (the e-paper fonts have nothing else): the page
  transliterates (Zurich, Kobenhavn, Lodz), the device drops anything else, and a long name
  is shortened with "..." on the screen.
- **Weather fetch: DECIDED.** One request per update (Open-Meteo, HTTPS, `timeformat=unixtime`,
  `timezone=auto`, `forecast_days=3`): the "current" snapshot plus 72 hourly values of
  temperature, weather code, wind and is_day. The whole thing (about 400 bytes) is kept in flash.
    - "Now" = the snapshot while it is less than 90 minutes old, otherwise the hourly value for the
      current hour, so a 12 or 24 hour fetch interval still shows the right hour.
    - Morning 06-12, Afternoon 12-18, Night 18-24 in the WEATHER LOCATION's local time; the
      temperature is the mean of the slot, the icon the most telling weather of the slot (one wet
      hour alone does not count, a thunderstorm hour always does). From 21:00 local the slots show
      TOMORROW (a "TOMORROW" tag appears above the forecast row).
    - Updates: at boot, right after leaving the Connect page when settings were saved, then every
      fetch interval. After a FAILED update the next try comes after min(interval, 15 min).
    - Wi-Fi is on only for the update (about 3-5 s when it works, up to 35 s when it does not) and
      the screen buttons do not react meanwhile. Reasons shown: network not found / password rejected
      / connection timed out / can't reach Open-Meteo / Open-Meteo error <code> / unexpected answer.
- **Profiles (updated).** Both profiles fetch the REAL weather. TEST: short intervals, and made-up demo
  data while there is no real data (the API line says "demo data"; the failure reason still shows as a
  red note). PRODUCTION: fetch options 1 h / 3 h / 12 h / 24 h (default 3 h), redraw every 10 min,
  real data only.
- **TLS is not verified** (`setInsecure()`): only public weather data travels here and it works with a
  wrong clock, but a man-in-the-middle could feed wrong weather. Later option: a certificate bundle.
- **microSD on the shared SPI bus: DECIDED, checked in the sources.** The card and the e-paper panel share
  SPI2 (MOSI 13, MISO 14, SCLK 15; panel CS 44, card CS 47). M5GFX starts that bus through Arduino's
  global `SPI` object and takes the bus for each of its own transactions; `SD.begin()` calls `SPI.begin()`
  without arguments (a no-op while the bus runs) and `SD.end()` never ends the bus. So the standard
  Arduino `SD` library works on the same `SPI` object, provided the sketch never calls `SPI.begin(<pins>)`
  or `SPI.end()` and never touches the card during a panel refresh (refreshes block the loop, so card
  access only happens between refreshes). Card speed is 4 MHz (as slow as the panel) until the gallery
  needs more.
    - Presence: PM1 GPIO1 (LOW = inserted; GPIO4 driven HIGH enables the switch), read before every
      redraw and on every web status request. Nothing polls the card between redraws, so a swap shows on
      the screen at the next redraw (10 min in PRODUCTION, 60 s in TEST).
    - An inserted card that can't be mounted is "unreadable" and retried only after 10 min, or when
      it is taken out and put back, or when the SD test is run. The chip select comes from M5Unified.
    - Used / free space is deliberately NOT read automatically: `SD.totalBytes()` / `usedBytes()` may scan
      the card's allocation table (seconds on a big card). Only the raw card size is shown.
    - Web page: Device tab shows the card status; Tests tab has "Test SD card" (writes, reads back,
      compares and deletes a 32 KB file, reports speed). Run it, leave the Connect page: if the Weather
      page then redraws normally, the sharing works on the real hardware.
- **microSD card formats (note for the README).** The card is read through FatFs from ESP-IDF:
    - FAT32: works, recommended (volumes up to 2 TB are readable).
    - FAT16 / FAT12: work too (mostly old cards of 2 GB or less).
    - exFAT and NTFS: NOT supported. ESP-IDF's FatFs has `FF_FS_EXFAT` set to 0 (exFAT compiled out), and
      the factory firmware's `sdkconfig` doesn't enable it either, so the factory firmware has the same limit.
    - THE PARTITION SCHEME MATTERS TOO: FatFs reads only a classic MBR partition table (or a card with no
      partition table at all). A card that is "FAT32" but partitioned with GPT is NOT recognised. This
      happens on a Mac when the whole card is erased with the "GUID Partition Map" scheme: in Disk Utility
      choose View > Show All Devices, select the CARD ITSELF (not just its volume), Erase, format "MS-DOS
      (FAT)", scheme "Master Boot Record". (The first version of this note forgot to mention the scheme.)
    - SDXC cards (over 32 GB) come pre-formatted as exFAT, so they must be reformatted. Windows won't offer
      FAT32 above 32 GB (use a tool such as guiformat).
    - Advice: use a card of 32 GB or less, or format a bigger one as FAT32 with 32 KB clusters (a large
      FAT32 volume with small clusters has a huge allocation table, which makes the free-space readout
      the gallery will need slow).
    - Verified in the ESP-IDF master sources, not in the exact IDF version bundled with the installed
      Arduino ESP32 core; the SD test on a real card is the definitive check.
- **microSD diagnosis (when a mount fails).** `sdcard::diagnose()` talks to the card directly in SPI mode
  (CMD0, CMD8, ACMD41, CMD58, CMD17 at 400 kHz, never longer than ~1.5 s in total, only using
  `SPI.beginTransaction / transfer / endTransaction` and the card's own chip select), reads sector 0 and
  puts ONE sentence in the card's error (serial log, web Device tab, SD test):
    - "No answer from the card (0xFF)": nothing replies on the bus (card not seated, contacts, bus problem);
    - "Card answers but doesn't finish starting up": damaged or incompatible card;
    - "Card starts up, but its first sector can't be read";
    - "GPT partition table (macOS 'GUID' scheme): reformat with 'Master Boot Record'";
    - "Card is exFAT / NTFS, which isn't supported: format it as FAT32", "Partition type 07 (exFAT or NTFS)...";
    - "Card is blank" / "no partition table" / "Partition table is empty": format it as FAT32;
    - "FAT32 (FAT16, FAT12) partition / volume found, but mounting failed": the card looks fine, so the SD
      library is retried at 1 MHz and then 400 kHz (if that works the card stays mounted at the slower speed
      and the serial log says so).
  The probe runs only when a mount attempt fails (so it follows the same 10-minute retry rule), never on a
  card that mounts normally. If the library's own messages are wanted as well, set the Arduino IDE's
  Tools > Core Debug Level to Warn / Info (it logs "GO_IDLE_STATE failed", "f_mount failed: ..." etc.).
- **Picture library on the card: BUILT (step 7, gallery API), not yet tested on the device.** (`gallery.h`, endpoints in
  `web.h`, the "Images" tab in `index_html.h`.)
    - Layout on the card: `/images/NAME.png` (the dithered pictures, 600x400 or 400x600), `/masters/NAME.jpg` (the
      ORIGINAL of each picture, same base name, `.jpg` or `.jpeg`), `/thumbs/NAME.png` (optional previews; the page
      uses the picture itself when there is none), `/playlist.json` (written by the device: on/off and order).
      Missing folders are created by the device. Long file names work (the Arduino core builds FatFs with LFN).
    - A picture's id is its file name without ".png": 1..40 characters of A-Z a-z 0-9 _ - ONLY. Other names in
      /images (spaces, dots, accents, macOS "._" files, Thumbs.db, other extensions) are ignored and counted as
      "ignored". The factory page's names ("imageN001.png", "imaged001.png") are fine. Max 120 pictures.
    - THE FILES ARE THE TRUTH: every scan lists /images and reconciles the playlist with it (vanished pictures are
      dropped, new files are appended and switched on, saved order and on/off kept). A hand-edited or corrupt
      playlist is tolerated and repaired. The list is cached and read again after a card swap or "Rescan".
    - Each picture is checked: PNG signature + IHDR give width and height ("fits" = 600x400 or 400x600).
    - Endpoints (all count as user activity): GET /api/images, GET /api/images/file?id=&kind=image|thumb|master,
      POST /api/images/enable?id=&on=0|1, POST /api/images/order?ids=a,b,c, POST /api/images/delete?id=,
      POST /api/images/rescan. Every change answers with the new list. No card -> 409 "no card" (the list itself
      says "absent" / "error" + the reason). Ids are validated (no path tricks can reach outside the folders).
      Delete removes the picture, its thumbnail and its original (the page asks first).
    - Playlist writes: temp file + rename, falling back to writing in place if the rename fails (never loses it).
    - NOT in this step: uploading (step 8: the page prepares the dithered PNG + thumbnail + the master JPEG; the
      device side needs a streamed multipart endpoint writing to temporary names, renamed when complete),
      drawing a picture on the screen, the slideshow itself (step 9, uses the playlist: enabled pictures in order).
    - Drawing hint from the factory firmware: pictures the page already reduced to the 6 colours are drawn with
      `epd_fastest` (no second dithering by the panel driver); un-dithered ones with `epd_quality`.
- **How the FACTORY firmware prepares pictures (read from its sources, applies to our step 8b).** Its web page does NOT
  dither. It composes the picture on a 600x400 (or 400x600) canvas (white background, pan / zoom / rotate of one or more
  layers) and uploads an UN-dithered RGB PNG with a field `algorithm` = "dither" (default) or "nearest". The device names
  the file `imageN###.png` for "nearest" and `imaged###.png` for "dither", and draws `imageN*` files with `epd_fastest`
  (the panel driver converts each pixel to the NEAREST of the 6 colours, no dithering) and all other files with
  `epd_quality` (the panel driver DITHERS while drawing: pairs of pixels with a spatial noise bias, tuned for this panel).
  The page's JavaScript `quantizeToEpdPalette()` is a re-implementation of that driver algorithm, used only for the
  on-page preview. Two palettes appear in it: the pure one (black, white, 255/255/0, 255/0/0, 0/0/255, 0/255/0) and a
  measured "original" one (white, black, 255/243/56 yellow, 191/0/0 red, 0/0/255 blue, 67/138/28 green) used for nearest.
  => Our slideshow must support BOTH kinds of file: pictures already reduced to the 6 colours (like the user's own
  "_dithered" files: draw with `epd_fastest`) and plain RGB pictures (draw with `epd_quality`, the driver dithers).
  Proposed rule, no extra metadata needed: PNG colour type 3 (palette) = already reduced -> `epd_fastest`; otherwise
  `epd_quality`. (To be confirmed on the device when the slideshow is built.)
- **Uploading pictures, slice A: BUILT, not yet tested on the device.** Device side in `gallery.h` + `web.h`, page side:
  the "Add a picture" panel in the Images tab.
    - POST /api/images/upload?id=<name>[&replace=1]: multipart, part `png` (required, 600x400 or 400x600, 3 MB at most)
      and `thumb` (optional, a PNG, 256 KB at most; a bad one is simply left out). Answers with the list plus
      "added":"<final name>". POST /api/images/master?id=<name>: part `master` (a JPEG, 32 MB at most) for a picture
      that is already on the card. The large original is a SEPARATE request because the ESP32 web server reads uploads
      byte by byte (slow): the picture appears in the list quickly and the original follows, with progress.
    - The `png` (or `master`) part must be the FIRST part of its request. Only multipart/form-data works (the server
      streams only that); chunks are 1436 bytes and are collected into 8 KB blocks before they are written.
    - Safety: every part goes to /upload/*.tmp; only when the request is complete and valid are the files renamed into
      place (thumbnail, then the picture, which is what makes it appear). Failures (bad PNG, wrong size, too big, write
      error, no card, phone gone) leave nothing behind. Leftovers of an interrupted upload (power cut) are deleted at
      the next scan; a new picture (or original) arriving while an old request is still pending makes the device
      forget the old one (so a dead request's thumbnail can never be attached to the next picture).
    - Names: letters, digits, _ and - (40 at most). A name that is taken becomes "name-2", "name-3", ...; capitals do
      not count (FAT would overwrite the old file otherwise). `replace=1` replaces the picture in place (position and
      on/off kept, its own capitals kept, old thumbnail dropped, original kept). 120 pictures at most.
    - Page: the PNG is checked in the browser before anything is sent (signature + size from the header, size limit);
      a name is suggested from the file name (".png", "_dithered", "_resized", "_dither", "_nearest" removed, only
      safe characters kept); the thumbnail (150x100 or 100x150) is drawn in the browser (skipped if the browser can't
      within 4 s); the picture counts for 30 % of the progress bar when an original follows.
- **Uploading pictures, slice B: the editor in the page: BUILT, not yet tested on the device.** (The "Add a picture" panel with its
  Picture / Original / Name fields of slice A is gone: every photo now goes through the editor.)
    - Flow: a "+" at the left of the PICTURES header opens a panel IN PLACE (not a window) with one button, "Choose a photo"
      (JPEG, PNG or TIFF; raw files such as ARW are refused by name, because an ARW is a TIFF inside). Even a PNG that
      already has the right size goes through the editor. Saving uploads the processed picture + a thumbnail, then the
      ORIGINAL (the chosen file, unchanged, named .jpg / .png / .tif by its real kind), which stays on the card so the
      picture can be edited again. An original over 32 MB is not kept (the page says so); the picture is still saved.
    - WHY the processing is in the browser: the ESP32 can't safely decode a multi-megabyte photo, resize it and dither it.
      It only stores what it is given. (The panel driver's own dithering of an RGB picture happens when drawing.)
    - Editor: preview frame 600x400 (portrait 400x600, chosen from the photo's shape, can be switched), drag to move,
      pinch / wheel / slider to zoom (100-800 %), Rotate (quarter turns), Fill (crop) or Fit (white or black border),
      Brightness / Contrast / Saturation / Warmth / Tint (-100..100), Colours: "Panel" (default), "Floyd-Steinberg", "Nearest colour".
      Warmth moves red up and blue down (cooler when negative); Tint moves green down and red + blue up (pinker when positive, greener
      when negative); +-100 is about +-40 steps of 255 on the strongest channel. They are applied with brightness and contrast
      in three 256-entry tables (fast), BEFORE saturation, and are part of the note stored in the picture (older pictures
      without them read as 0). Use them to tune photos (for instance RAW files converted to washed-out JPEGs) to the screen.
      The preview is the plain photo while you interact and the way the panel will show it (in the real ink colours)
      220 ms after you stop. Big photos get a 2400 px working copy for dragging (first step never above 4096 px: iOS
      limits canvases to about 16 million pixels); the final picture is drawn from the full photo when it is shown at half
      size or more, else from a smoothly halved copy. EXIF rotation is applied (createImageBitmap "from-image").
    - Colours: "Panel" saves the plain RGB picture (tone settings applied) and lets the panel driver dither it when drawn
      (the factory behaviour; the preview is an exact port of the factory page's quantiser, tested byte for byte against
      the factory source). "Floyd-Steinberg" and "Nearest" reduce the picture in the page and save the six PURE palette
      colours. Both judge "nearest" in CIE Lab against the real ink colours (plain RGB distance sends every mid-grey to the
      green ink and speckles grey walls and skies with green). All are RGB PNGs from canvas.toBlob.
    - HOW A PICTURE WAS EDITED travels inside it: a PNG "tEXt" chunk `pcedit` (JSON: orientation, rotation, fill/fit, zoom,
      pan, tone, colour mode, border) right after IHDR. The device ignores it. Edit reads it back (never trusted: every value
      is clamped and whitelisted). It is applied only together with the original: a picture that has no original is edited
      as it is, with default settings.
    - Edit button on every picture that is readable or has an original: fetches the picture, then (if there is one) the
      original, reopens the editor with the stored settings, name fixed, Save = replace (position and on/off kept) and the
      original is NOT sent again.
    - FOR THE SLIDESHOW (step 9), how to draw a picture: read the `pcedit` note (a few bytes before IDAT): colour mode
      "panel" -> `epd_quality` (the driver dithers); "fs" or "nearest" -> `epd_fastest` (already six colours). A PNG
      without our note (made elsewhere): palette PNG (colour type 3) -> `epd_fastest`, otherwise `epd_quality`.
    - Originals on the card: /masters/NAME.jpg|.jpeg|.png|.tif|.tiff (the device recognises the kind by the first bytes,
      not the name; only one original per picture; the list says which in "mext"). TIFF originals can only be edited in
      a browser that can open TIFF (Safari); the page says so otherwise.
    - Measured on a desktop (60 ms panel / 150 ms Floyd-Steinberg / 130 ms nearest for a full 600x400 frame; tone settings 2-10 ms);
      a phone is probably 2-4 times slower. NOT yet measured on the real iPhone: dragging smoothness, the pause before the
      dithered preview, memory with 12 and 48 megapixel photos.
- **Wi-Fi error "wifi_init_default: netstack cb reg failed with 12308" (seen on the Serial Monitor after setting the Wi-Fi credentials).**
  12308 = 0x3014 = ESP_ERR_WIFI_STOP_STATE ("WiFi is stopping"). The message comes from ESP-IDF's handler that sets the network
  interface up when the radio STARTS; it fails if the driver is still busy STOPPING. Cause here: leaving the Connect page stops the
  access point and, because settings were just changed, a weather fetch starts within milliseconds and switches the radio on
  again. Fix (written, tested on the PC only: the PC test checks the timing, not the ESP-IDF message): `radio.h`, one function,
  `radio::off()`: switch off, wait until `WiFi.getMode()` says off, then wait 250 ms more. Used when the access point stops, when
  a weather fetch ends (also when it failed) and when the network scan before the access point ends. Costs about 0.3 s per switch.
  If the message still appears but the weather updates and the Connect page work, it is harmless noise. The next thing to try
  would be a longer settle time (`radio::SETTLE_MS`).
- **Temperature / humidity sensor (SHT40) and the Connect page.** It was read ONCE each time the Connect page was drawn (the page
  shares `buildReading()` with the Weather page), never continuously and never by the web handlers. The sensor has no sleep command
  or enable pin (it idles by itself between single-shot measurements), so "switching it off" means not reading it: the Connect
  page now calls `buildReading(false)`. The Weather page still reads it each time it is drawn. (The web page's status poll reads
  the clock and the battery, not the sensor.)
- **Slideshow page: BUILT, not yet tested on the device.** (`slideshow.h` = the logic, the drawing and the page in `PaperColorFrame.ino`,
  the message screen in `ui.h`, the settings in `settings.h` / `web.h`, the controls in the Images tab of the page.)
    - Pages: Weather -> Slideshow -> Connect (G10 forward, G9 back). G1 on the Slideshow page = the next picture at once (and restarts
      the timer); on the Weather page it is still the manual weather update.
    - Which pictures: switched on AND readable PNGs, in playlist order, or SHUFFLED (every picture once per round, never the same twice in a
      row, the round is made again when the playlist changes). If the current picture is deleted or switched off the one that took its place
      comes next. A single picture is drawn once and then left alone (no panel refresh at every interval).
    - Interval and shuffle: saved at once from the Images tab (`slideMin`, `shuffle` in POST /api/settings; GET /api/settings has
      `"slide":{"min":..,"options":[..],"shuffle":0|1}`). TEST: 1, 2, 5, 10 min (default 2). PRODUCTION: 15 min, 1 h, 3 h, 24 h (default 1 h).
      A stored interval that is not one of the profile's options becomes the default (`settings::fixSlideMin`). Changing only the slideshow
      does NOT make the device fetch the weather (the "settings changed" hook is not called). The numbers are parsed strictly now ("5x", "1e1"
      are refused, also for the weather interval).
    - Drawing: the picture FILE is read into PSRAM (at most 4 MB, in a loop: a read may hand over less than asked for; an incomplete file is never
      drawn) and drawn with `canvas.drawPng(data, length, x, y, 0, 0, 0, 0, scale, scale)` into the true-colour (RGB565) canvas, then ONE refresh.
      NOT `drawPngFile(SD, ...)`: that template did not compile with the M5GFX version installed on the user's computer ("DataWrapperT<fs::SDFS>
      is abstract"); `drawPng(const uint8_t*, uint32_t, ...)` does not depend on the file-system support of the library. The header and the note
      are taken from the same buffer (no second read). The panel mode
      comes from the picture's note (see `slide::drawMode`): "panel" -> `epd_quality` (the panel driver dithers); "fs" / "nearest" -> `epd_fastest`
      (already six colours); a picture without our note: palette PNG -> fastest, anything else -> quality. The note must be inside the first
      1024 bytes of the file (the editor puts it right after the header). After each picture `epd_text` is set again (the other pages use it).
    - Placement: every picture fills the screen as far as its shape allows, centred, nothing cut off. PORTRAIT pictures (like the editor's 400x600)
      are drawn TURNED a quarter turn so that they fill the landscape panel (a 400x600 one fills it exactly): the person turns the frame to look
      at them. Setting "Portrait pictures" (Images tab, saved at once, `portrait` in /api/settings): 0 = "Fill the screen: I turn the frame
      clockwise" (the picture is drawn turned counter-clockwise), 1 = "... counter-clockwise" (drawn turned clockwise), 2 = "Keep upright,
      smaller (no turning)" (about 267x400 with white borders at the sides; this is also the fall-back if the turned drawing can't be done).
      Default 0. I could not know which way you turn the frame: if a portrait picture is upside down, pick the other direction.
      How: the picture is drawn into a buffer of its own (RGB565, the size of the picture, about 480 KB in PSRAM) and then COPIED turned into the
      canvas by `slide::rotateQuarter` (a plain pixel copy: exact, no smoothing, so the six pure colours stay pure). M5GFX's own
      `pushRotateZoom` was tested and rejected: it was off by one pixel (an empty row or column and a cut-off last line) unless the centre was
      shifted by a pixel in a direction-dependent way. If there is no memory for the buffer the picture is shown upright; a picture that can't
      be decoded is skipped (not tried again upright). Landscape pictures are never turned (also square ones); other sizes are scaled to fit.
    - When there is nothing to show a message screen says why: "No SD card", "SD card error", "No pictures" (these three look again every 5 s
      and show a picture by themselves as soon as there is one), "Can't show them" (pictures exist but none can be drawn: tried again only at
      the next interval or with G1: NOT every few seconds, which would refresh the panel all the time). The same message is never drawn twice in a
      row. A picture that can't be drawn is skipped (up to 5 tries), with a log line.
    - Weather: not fetched while the Slideshow page is open; a due fetch happens when the Weather page is entered again. (Power management,
      step 10, should decide how the weather and the slideshow share the time.)
    - Run on the device by the user (Oct 2026): works (the slideshow, the portrait turning, the in-memory picture loading, the upload and the editor, the two image tabs).
- **Weather cache:** internal flash (Preferences / NVS), not the SD card.
- **AP status via LED (idea):** since the screen is static in AP mode, the RGB LED
  could show AP on / client connected / about to time out.
- **Forecast slots:** proposed Morning 06-12, Afternoon 12-18, Night 18-24
  (one representative temperature each; precipitation chance could be added).
- **Stale weather data: DECIDED.** A failed update never blanks the screen: the last good
  forecast stays on screen with a red "Update failed: <reason>" note above the "API last
  updated" line, for as long as the forecast still covers the current hour. After that the
  outdoor field shows "Weather unavailable - Forecast expired". A forecast that belongs to a
  different place than the one saved is thrown away.
- **Charging detection:** inferred (USB present and battery < ~4.15 V); tune
  from the serial log on real hardware.

## Small ideas that came up (not planned yet)
- Certificate bundle instead of `setInsecure()`.
- LED feedback during the weather update (it blocks for a few seconds): see "Status LEDs".

## Known cosmetic item (deferred on request)
- In the header the text and icons are not perfectly centered vertically. The
  cause: M5GFX centers text on the font box, not the visible glyphs. A fix is to
  center on the glyph ink bounds (computed from the font's glyph table).

## Build order (option C)
1. Done: setup, sensors + RTC + battery, weather screen layout, error screen, TEST/PRODUCTION profile
2. DONE: G9 / G10 switch Weather <-> Connect-to-Device page; Connect page starts the AP, shows QR + instructions, inactivity timeout
3. DONE: AP page #1: RGB LED control + phone -> device time sync + diagnostics card (tested on the iPhone)
4. DONE: AP page #2: tabbed page; settings (home Wi-Fi with scan list, location by city search or coordinates, fetch interval), saved in flash; TEST / PRODUCTION profiles
5. DONE: Device-side weather: join home Wi-Fi, fetch Open-Meteo, cache in internal flash, real error reasons, G1 button for an update on demand (tested on the device)
6. DONE: microSD: presence check, mount on the shared bus, header icon, degraded mode, web SD test, diagnosis of unreadable cards (works on the device with a good card)
7. Gallery API (list, delete, enable, order) + Images tab  <- DONE: run on the device by the user (Oct 2026), works
8a. Upload endpoints on the device (picture + thumbnail, then the original; the first "Add a picture" panel was replaced by the editor of 8b)  <- DONE: run on the device by the user (Oct 2026), works
8b. Upload pipeline editor (JPEG/PNG/TIFF -> crop / rotate / zoom / tone -> colours; no raw files)  <- DONE: run on the device by the user (Oct 2026), works
9. Slideshow from the playlist  <- DONE: run on the device by the user (Oct 2026), works
10. Power management (RTC alarm + PM1 power-off between updates), status LEDs, button actions
