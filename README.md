# PaperColorFrame

A picture frame and weather station for the **M5Stack PaperColor** (ESP32-S3, 600 x 400 six-colour e-paper). It shows the weather,
or your own pictures from a microSD card, and it is set up from a phone through a web page the device serves itself. It is built to
run for weeks on its battery: the e-paper keeps its picture without power, so the device sleeps between updates.

* **Weather page**: the outdoor weather now and for the morning, afternoon and night (from [Open-Meteo](https://open-meteo.com), no account needed),
  with colour icons (including wind), and the indoor temperature and humidity from the sensor in the device.
* **Slideshow page**: the pictures on the microSD card, in order or shuffled, each for 1, 3, 12 or 24 hours. Portrait pictures are drawn
  turned so that they fill the screen (you turn the frame).
* **Web page** (the device's own Wi-Fi, opened from your phone): choose and edit pictures in a built-in picture editor
  (crop, zoom, tone, six-colour dithering), set the place and the home Wi-Fi, set the clock, control the LED, read the instructions.
* **Status light**: the RGB LED shows what the device is doing (button received, screen refreshing, weather updating, upload, error,
  charging ...).
* **Battery care**: a crossed-out battery screen and a clean switch-off before the battery is empty (an e-paper keeps its last picture, so
  a dead battery otherwise looks like a crashed device).

> **Status.** Developed with a large automatic test suite on a PC (`test/`) and run on a real device by the author. At the time of
> writing the newest parts - light sleep, the status light, the clock refresh over the internet (NTP), the wind icon, the handling of failed
> weather updates and the hour-based production timings - had been tested on the PC only. See [Status](#status-what-has-run-on-a-device).

## Hardware

M5Stack **PaperColor**: ESP32-S3 (16 MB flash, OPI PSRAM), 6-colour e-paper panel (native 400 x 600, used as 600 x 400), three buttons
(G10, G9, G1), microSD slot (shares its SPI bus with the panel), SHT40 temperature / humidity sensor, RX8130 real-time clock, the M5PM1 power
management chip (battery, charging, the Power button, the LED), 1250 mAh battery.

## Install

1. Arduino IDE with the **ESP32** board package (Espressif; Boards Manager).
2. Libraries (Library Manager): **M5Unified**, **M5GFX**, **M5PM1**.
   The code was written against M5GFX `347a232`, M5Unified `e125e73` and M5PM1 `be9a545` (October 2026). Other versions may work, but
   differences between versions do bite: for instance `drawPngFile(SD, ...)` did not compile with the M5GFX that was installed on the author's
   computer, so the sketch reads the file itself and calls `drawPng(data, length, ...)`.
3. Open `PaperColorFrame/PaperColorFrame.ino` (the folder must keep the name `PaperColorFrame`).
4. Tools menu: **Board: ESP32S3 Dev Module, Flash Size: 16MB, PSRAM: OPI PSRAM, USB CDC On Boot: Enabled**; serial monitor 115200.
5. Choose the build profile near the top of the sketch (`BUILD_PROFILE`): `PROFILE_TEST` (intervals of minutes, for developing) or
   `PROFILE_PRODUCTION` (intervals of hours, for normal use).
6. Upload.

## First use

1. Format a microSD card as **FAT32** (on a Mac choose the *Master Boot Record* scheme; 32 GB or less works best; exFAT and NTFS are not supported)
   and put it in. Without a card the Slideshow page says so; the weather works without one.
2. Press **G10** twice (Weather -> Slideshow -> Connect to Device). The screen shows the device's Wi-Fi name, its password (random, made on first
   use) and a QR code. Join that Wi-Fi with your phone and open the address shown. After 10 minutes without activity the device closes its Wi-Fi.
3. In the web page: **Device** tab (the clock is synced from the phone), **Weather** tab (search your place), **Wi-Fi** tab (your home network:
   the device joins it only briefly, to fetch the weather and the time), **Upload Image** / **Image Gallery** tabs for pictures. The
   **Instructions** tab explains everything again for the person who uses the frame.

## Using it

| Control | Does |
|---|---|
| **G10** / **G9** | next page / previous page (Weather -> Slideshow -> Connect to Device) |
| **G1** | Weather page: update the weather now. Slideshow page: next picture. |
| **Power** (belongs to the PM1 chip) | single click = restart, double click = switch off, long press = programming mode, any press while off = switch on |

**Status light** (the first that applies wins): red / yellow flashing for 10 s = a weather update failed or the SD card can not be read; one long
white pulse = a button press; pink blinking = the screen refreshes; blue and green pulses in turns = the weather is updated; fast blue blinking =
a picture is uploaded; one short red flash every 30 s = battery 15 % or less; slow white pulse = the Wi-Fi of the device waits for a phone (steady
white = a phone is connected); slow yellow pulse = charging, steady green = charged (both as long as the charger is connected). A colour set by
hand in the LED tab replaces the status light until it is switched off. The light can be switched off or dimmed in the LED tab.

**An e-paper keeps its last picture**, so a screen that does not change is not always a frozen device: it may be switched off or its battery empty.

## Timings and battery

| | PRODUCTION | TEST |
|---|---|---|
| Weather page changes | every 3 h | every 60 s |
| Weather update | 3 h (default), 6 h, 12 h, 24 h | 1, 2 (default), 5, 10 min |
| Picture changes | 1 h (default), 3 h, 12 h, 24 h | 1, 2 (default), 5, 10 min |

* **Light sleep**: on the battery, while idle, the device sleeps (at most 5 s at a time, never past the next due event). A button press wakes it at
  once. It never sleeps on a charger, on the Connect page, while a button is held, or while the LED shows something. `IDLE_SLEEP` in the sketch
  switches it off.
* **Failed weather update**: if the device could not get online it does not retry by itself (that only drains the battery): it waits until you save
  the Wi-Fi settings or press G1. A server error is retried after an hour (`RETRY_SERVER_MIN`, `RETRY_NETWORK_MIN` in the sketch).
* **Estimate**: roughly three weeks on the 1250 mAh battery, assuming about 2 mA in light sleep. **That is an assumption, not a
  measurement**; the sleep current decides. If it is too high, the next step is to power the ESP32 completely off between updates (what the factory
  firmware does); the notes in `docs/FUTURE_IDEAS.md` describe how.
* **Low battery**: below 15 % the LED flashes; at 3350 mV (running) or below 3500 mV (at start-up, on battery) the device shows the crossed-out
  battery and switches itself off (`battery.h`).

## Project layout

```
PaperColorFrame/   the Arduino sketch (open PaperColorFrame.ino)
  PaperColorFrame.ino   setup / loop / pages          weather.h, weather_fetch.h, ntp.h   the weather and the clock
  ui.h                  all drawing                   slideshow.h, gallery.h              the pictures
  web.h, index_html.h   web server and web page       statusled.h, idlesleep.h, battery.h LED, sleep, battery
  sensors.h, sdcard.h, settings.h, radio.h, wifi_*.h, types.h, timeutil.h               hardware helpers, settings, types
docs/FUTURE_IDEAS.md   decisions, design notes, ideas that are not built yet
test/                  the automatic tests (see test/README.md)
```

## Tests

`test/` holds the automatic tests (39 suites, run with one command) and the "planted bug" mutation tests. They run on a normal computer with
stand-ins for the hardware: logic, the web server handlers, the whole sketch end to end (both build profiles), the web page in a simulated browser,
and checks that the written instructions match the code. See [test/README.md](test/README.md).

## Status: what has run on a device

* Run on a real device by the author: the weather page and its colours, the slideshow (including turned portrait pictures), picture upload and the
  picture editor, the two picture tabs of the web page, the crossed-out battery screen and the start-up check, the Instructions tab.
* **Tested on a PC only so far**: light sleep (waking on the buttons, the PSRAM during sleep), the status light, the NTP clock refresh, the wind icon,
  the retry rules for failed updates, the production timings in hours, Floyd-Steinberg as the default colour method. A PC test can not show the
  real current draw, the real colours of the LED, or the real e-paper.
* The long press of the Power button (programming mode, as the datasheet says) has not been tried.

## Known limitations

* The web page is plain HTTP on the device's own Wi-Fi (WPA2 with a random password); anyone who has that password can change the settings.
* The weather request is HTTPS **without certificate verification** (`setInsecure()`): only public weather data travels there. A certificate
  bundle would fix that.
* One place at a time; Open-Meteo is the only weather source; camera raw files are not accepted (JPEG, PNG and TIFF are; TIFF only opens in Safari).
* Pictures must be on a FAT32 card; the card is read when the slideshow draws and is shared with the panel's bus.

## Credits and license

* Weather data by [Open-Meteo.com](https://open-meteo.com) (CC BY 4.0).
* Built on [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX) (which includes the QR code and PNG
  decoders) and [M5PM1](https://github.com/m5stack/M5PM1). The hardware set-up follows M5Stack's factory firmware
  ([M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo)), which is also the reference for the "Panel" colour mode of the picture editor.
* **License: not chosen yet.** Add a `LICENSE` file before publishing; without one, nobody else may legally reuse the code.