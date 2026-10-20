// ui.h - all drawing code. Draws into the global `canvas` (an M5Canvas) that the
// sketch declares before including this file. Does not touch hardware.
#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "types.h"

constexpr int HEADER_H = 40;    // height of the top bar
constexpr int WX_W     = 400;   // width of the outdoor/weather panel; indoor gets the rest

// Battery icon in the header (body only, without the little nub). The percentage text and the
// plug symbol have to fit inside with a small margin.
#ifndef BATTERY_W
#define BATTERY_W 60
#endif
#ifndef BATTERY_H
#define BATTERY_H 22
#endif

// Shown under "OUTDOOR": the place the user picked on the settings page (empty = not set yet).
static char locationName[40] = "";

// Panel palette colors (from M5GFX Panel_ED2208)
static uint16_t C_BLACK, C_WHITE, C_YELLOW, C_RED, C_BLUE, C_GREEN;

static void initColors() {
  C_BLACK  = canvas.color565(0, 0, 0);
  C_WHITE  = canvas.color565(255, 255, 255);
  C_YELLOW = canvas.color565(255, 243, 56);
  C_RED    = canvas.color565(191, 0, 0);
  C_BLUE   = canvas.color565(100, 64, 255);
  C_GREEN  = canvas.color565(67, 138, 28);
}

// ============================================================================
//  Font metric helpers
//  M5GFX puts the text baseline `fontAscent()` pixels below the top of the text
//  box (the tallest glyph in the font). Knowing that lets us place the degree
//  ring and unit letter exactly instead of guessing.
// ============================================================================
static int fontAscent(const lgfx::GFXfont* f) {
  int ab = 0;
  for (int c = f->first; c <= f->last; c++) {
    int a = -(int)f->glyph[c - f->first].yOffset;
    if (a > ab) ab = a;
  }
  return ab;
}
static int glyphAscent(const lgfx::GFXfont* f, char ch) {
  return -(int)f->glyph[ch - f->first].yOffset;
}

// ============================================================================
//  Temperature text: "23.4 °C"  (number + hand-drawn ring + optional "C")
//  The GFX free fonts have no degree glyph, so the ring is drawn by hand.
// ============================================================================
struct TempStyle {
  const lgfx::GFXfont* numFont;
  float                numSize;
  int                  ringR;
  const lgfx::GFXfont* unitFont;   // nullptr = no unit letter
};

static int tempWidth(const TempStyle& st, const char* num) {
  canvas.setFont(st.numFont);
  canvas.setTextSize(st.numSize);
  int w = canvas.textWidth(num) + 4 + 2 * st.ringR + 2;
  if (st.unitFont) {
    canvas.setFont(st.unitFont);
    canvas.setTextSize(1.0f);
    w += 3 + canvas.textWidth("C");
  }
  return w;
}

// (x, y) = top-left of the text box. Returns the right edge.
static int drawTemp(int x, int y, const TempStyle& st, const char* num, uint16_t col) {
  canvas.setTextColor(col);
  canvas.setTextDatum(top_left);

  canvas.setFont(st.numFont);
  canvas.setTextSize(st.numSize);
  canvas.drawString(num, x, y);
  const int tw = canvas.textWidth(num);

  const int baseline  = y + (int)(fontAscent(st.numFont) * st.numSize + 0.5f);
  const int digitTop  = baseline - (int)(glyphAscent(st.numFont, '0') * st.numSize + 0.5f);

  const int rx = x + tw + 4 + st.ringR;
  const int ry = digitTop + st.ringR + 1;
  canvas.drawCircle(rx, ry, st.ringR, col);
  canvas.drawCircle(rx, ry, st.ringR - 1, col);
  int right = x + tw + 4 + 2 * st.ringR + 2;

  if (st.unitFont) {
    canvas.setFont(st.unitFont);
    canvas.setTextSize(1.0f);
    const int uy = baseline - fontAscent(st.unitFont);    // align baselines
    canvas.drawString("C", right + 3, uy);
    right += 3 + canvas.textWidth("C");
  }
  return right;
}

static const TempStyle STYLE_BIG   = { &fonts::FreeSansBold24pt7b, 1.6f, 6, &fonts::FreeSansBold18pt7b };
static const TempStyle STYLE_MID   = { &fonts::FreeSansBold24pt7b, 1.0f, 4, &fonts::FreeSansBold12pt7b };
static const TempStyle STYLE_SMALL = { &fonts::FreeSansBold12pt7b, 1.0f, 3, nullptr };

// ============================================================================
//  Weather icons - drawn with primitives so there are no image assets.
//  Each icon fits inside an s x s box whose top-left is (x, y).
// ============================================================================

// Cloud silhouette from three circles + a flat base. `inset` shrinks it so a
// white fill drawn on top of a black one leaves a clean outline.
static void cloudShape(int x, int y, int s, int inset, uint16_t col) {
  const int r1 = s * 17 / 100 - inset;
  const int r2 = s * 24 / 100 - inset;
  const int r3 = s * 16 / 100 - inset;
  const int x1 = x + s * 28 / 100;
  const int x2 = x + s * 72 / 100;
  canvas.fillCircle(x1, y + s * 62 / 100, r1, col);
  canvas.fillCircle(x + s * 50 / 100, y + s * 46 / 100, r2, col);
  canvas.fillCircle(x2, y + s * 60 / 100, r3, col);
  canvas.fillRect(x1, y + s * 60 / 100, x2 - x1, s * 19 / 100 - inset, col);
}

// A cloud with an outline colour and a fill colour: by default a white cloud with a blue outline.
static void drawCloudColored(int x, int y, int s, uint16_t outline, uint16_t fill) {
  cloudShape(x, y, s, 0, outline);
  cloudShape(x, y, s, 2, fill);
}
static void drawCloud(int x, int y, int s) { drawCloudColored(x, y, s, C_BLUE, C_WHITE); }

static void drawSun(int cx, int cy, int r) {
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    float ca = cosf(a), sa = sinf(a);
    int r0 = r + 4;
    int r1 = r + 4 + r * 6 / 10;
    canvas.drawWideLine((int)(cx + ca * r0), (int)(cy + sa * r0),
                        (int)(cx + ca * r1), (int)(cy + sa * r1), 1.4f, C_RED);      // red rays
  }
  canvas.fillCircle(cx, cy, r, C_YELLOW);
  canvas.drawCircle(cx, cy, r, C_RED);
}

static void drawMoon(int cx, int cy, int r) {
  canvas.fillCircle(cx, cy, r, C_BLUE);
  canvas.fillCircle(cx + r * 45 / 100, cy - r * 30 / 100, r * 85 / 100, C_WHITE);   // bite out of it
}

static void drawFlake(int cx, int cy, int r, uint16_t col) {
  int d = r * 7 / 10;
  canvas.drawLine(cx - r, cy, cx + r, cy, col);
  canvas.drawLine(cx, cy - r, cx, cy + r, col);
  canvas.drawLine(cx - d, cy - d, cx + d, cy + d, col);
  canvas.drawLine(cx - d, cy + d, cx + d, cy - d, col);
}

static void drawBoltIcon(int x, int y, int s) {
  auto p = [&](int v) { return s * v / 100; };
  canvas.fillTriangle(x + p(60), y + p(54), x + p(38), y + p(80), x + p(53), y + p(80), C_YELLOW);
  canvas.fillTriangle(x + p(46), y + p(74), x + p(64), y + p(74), x + p(40), y + p(100), C_YELLOW);
  canvas.drawTriangle(x + p(60), y + p(54), x + p(38), y + p(80), x + p(53), y + p(80), C_BLACK);
  canvas.drawTriangle(x + p(46), y + p(74), x + p(64), y + p(74), x + p(40), y + p(100), C_BLACK);
}

// One wind streak: a line that ends in a curl (the right half of a ring), curling up or down.
static void drawWindStreak(int x0, int x1, int y, int r, bool curlUp, int thick, uint16_t col) {
  canvas.drawWideLine(x0, y, x1, y, thick / 2.0f, col);
  const int cy = curlUp ? y - r : y + r;
  canvas.fillArc(x1, cy, r - thick / 2, r + (thick + 1) / 2, 270, 90, col);
}

static void drawWeatherIcon(WeatherIcon ic, int x, int y, int s) {
  auto p = [&](int v) { return s * v / 100; };

  switch (ic) {
    case ICON_CLEAR_DAY:
      drawSun(x + p(50), y + p(50), p(22));
      break;

    case ICON_CLEAR_NIGHT:
      drawMoon(x + p(46), y + p(52), p(30));
      canvas.fillCircle(x + p(80), y + p(20), 2, C_BLUE);
      canvas.fillCircle(x + p(88), y + p(44), 1, C_BLUE);
      canvas.fillCircle(x + p(66), y + p(10), 1, C_BLUE);
      break;

    case ICON_PARTLY_DAY:
      drawSun(x + p(34), y + p(34), p(17));
      drawCloud(x + p(14), y + p(20), p(86));
      break;

    case ICON_PARTLY_NIGHT:
      drawMoon(x + p(34), y + p(36), p(20));
      drawCloud(x + p(14), y + p(20), p(86));
      break;

    case ICON_CLOUDY:
      drawCloudColored(x + p(18), y - p(6), p(80), C_BLUE, C_BLUE);    // back: solid blue
      drawCloud(x - p(4),  y + p(6), p(92));                          // front: white, blue outline
      break;

    case ICON_FOG:
      for (int i = 0; i < 4; i++) {
        static const int x0[4] = {15, 25, 10, 20};
        static const int x1[4] = {85, 95, 75, 90};
        int yy = y + p(28 + i * 15);
        canvas.drawWideLine(x + p(x0[i]), yy, x + p(x1[i]), yy, 2.0f, C_BLUE);
      }
      break;

    case ICON_DRIZZLE:
    case ICON_RAIN: {
      drawCloud(x, y - p(9), s);
      int len = (ic == ICON_RAIN) ? p(20) : p(11);
      for (int i = 0; i < 3; i++) {
        int dx = x + p(30 + i * 20);
        int dy = y + p(74) + (i == 1 ? p(4) : 0);
        canvas.drawWideLine(dx + p(3), dy, dx - p(3), dy + len, 1.6f, C_BLUE);
      }
      break;
    }

    case ICON_SNOW: {
      drawCloud(x, y - p(9), s);
      int r = s * 9 / 100; if (r < 4) r = 4;
      drawFlake(x + p(28), y + p(80), r, C_BLUE);
      drawFlake(x + p(50), y + p(92), r, C_BLUE);
      drawFlake(x + p(72), y + p(80), r, C_BLUE);
      break;
    }

    case ICON_THUNDER:
      drawCloudColored(x, y - p(9), s, C_BLUE, C_BLUE);                // a solid blue storm cloud with a yellow bolt
      drawBoltIcon(x, y, s);
      break;

    case ICON_WIND: {
      const int th = p(4) < 3 ? 3 : p(4);
      drawWindStreak(x + p(8),  x + p(58), y + p(36), p(10), true,  th, C_BLUE);
      drawWindStreak(x + p(8),  x + p(76), y + p(54), p(13), true,  th, C_BLUE);
      drawWindStreak(x + p(22), x + p(60), y + p(74), p(10), false, th, C_BLUE);
      break;
    }

    case ICON_ERROR: {
      // Sad little cloud: dot eyes, a frown and a tear.
      drawCloudColored(x, y + p(6), s, C_BLACK, C_WHITE);
      const int er = (s >= 90) ? p(3) + 1 : p(4);
      canvas.fillCircle(x + p(40), y + p(52), er, C_BLACK);                // eyes
      canvas.fillCircle(x + p(60), y + p(52), er, C_BLACK);
      canvas.fillArc(x + p(50), y + p(76), p(8), p(11), 215, 325, C_BLACK);   // frown
      const int tr = (s >= 90) ? 3 : 2;                                    // tear
      canvas.fillTriangle(x + p(40), y + p(58), x + p(40) - tr, y + p(64), x + p(40) + tr, y + p(64), C_BLUE);
      canvas.fillCircle(x + p(40), y + p(64) + 1, tr, C_BLUE);
      break;
    }

    case ICON_NONE:
    default:
      canvas.setFont(&fonts::FreeSansBold18pt7b);
      canvas.setTextSize(s >= 90 ? 2.5f : 1.0f);     // bigger "?" in the big current-conditions slot
      canvas.setTextColor(C_BLACK);
      canvas.setTextDatum(middle_center);
      canvas.drawString("?", x + s / 2, y + s / 2);
      break;
  }
}

// ============================================================================
//  Battery icon (header)
// ============================================================================

// Electric plug symbol, 10 x 16 px, top-left at (x, y).
static void drawPlug(int x, int y, uint16_t col) {
  canvas.fillRect(x + 2, y,      2, 5, col);    // left prong
  canvas.fillRect(x + 6, y,      2, 5, col);    // right prong
  canvas.fillRect(x,     y + 5, 10, 6, col);    // body
  canvas.fillRect(x + 4, y + 11,  2, 5, col);   // cable
}

// White outline on the black header, interior filled to the charge level.
// On battery the percentage is printed inside; while charging a plug symbol is
// shown instead (no percentage). The contents are drawn twice with clip
// rectangles so they stay readable on both the filled and the empty part.
// `rightX` is the right edge of the whole icon (nub included); returns its left edge.
static int drawBatteryIcon(int rightX, int cy, bool ok, int pct, bool charging) {
  const int bodyW = BATTERY_W, bodyH = BATTERY_H, nubW = 3, nubH = 8;
  const int bodyX = rightX - nubW - bodyW;
  const int bodyY = cy - bodyH / 2;
  const int innerX = bodyX + 3, innerY = bodyY + 3;
  const int innerW = bodyW - 6, innerH = bodyH - 6;

  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  // outline, hollow interior, nub
  canvas.fillRoundRect(bodyX, bodyY, bodyW, bodyH, 4, C_WHITE);
  canvas.fillRoundRect(bodyX + 2, bodyY + 2, bodyW - 4, bodyH - 4, 2, C_BLACK);
  canvas.fillRect(bodyX + bodyW, cy - nubH / 2, nubW, nubH, C_WHITE);

  // fill level
  const int fillW = ok ? (innerW * pct) / 100 : 0;
  const uint16_t fillColor  = (pct >= 50) ? C_GREEN : (pct >= 20) ? C_YELLOW : C_RED;
  const uint16_t textOnFill = (fillColor == C_YELLOW) ? C_BLACK : C_WHITE;
  if (fillW > 0) canvas.fillRect(innerX, innerY, fillW, innerH, fillColor);

  // contents: plug (charging) or percentage (on battery), centered
  char t[8];
  if (ok) snprintf(t, sizeof(t), "%d%%", pct);
  else    snprintf(t, sizeof(t), "--");
  const int cx = bodyX + bodyW / 2;

  auto paint = [&](uint16_t col) {
    if (charging) {
      drawPlug(cx - 5, cy - 8, col);
    } else {
      canvas.setFont(&fonts::FreeSansBold9pt7b);
      canvas.setTextSize(1.0f);
      canvas.setTextDatum(middle_center);
      canvas.setTextColor(col);
      canvas.drawString(t, cx, cy);
    }
  };

  if (fillW > 0) {
    canvas.setClipRect(innerX, innerY, fillW, innerH);
    paint(textOnFill);
  }
  canvas.setClipRect(innerX + fillW, innerY, innerW - fillW, innerH);
  paint(C_WHITE);
  canvas.clearClipRect();

  return bodyX;
}

// ============================================================================
//  "Battery empty" screen: a big battery, crossed out in red, and nothing else. It is the last thing the
//  panel shows before the device switches itself off, so it has to say "dead battery", not "crashed".
// ============================================================================
static void drawLowBatteryScreen() {
  const int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(C_WHITE);

  const int bodyW = 280, bodyH = 150, nubW = 22, nubH = 60, wall = 12;
  const int x = (W - bodyW - nubW) / 2, y = (H - bodyH) / 2;

  canvas.fillRoundRect(x + bodyW - 6, y + (bodyH - nubH) / 2, nubW + 6, nubH, 6, C_BLACK);    // the nub
  canvas.fillRoundRect(x, y, bodyW, bodyH, 18, C_BLACK);                                      // the body: a thick outline ...
  canvas.fillRoundRect(x + wall, y + wall, bodyW - 2 * wall, bodyH - 2 * wall, 8, C_WHITE);   // ... and nothing inside: empty

  // crossed out: a thick red slash, with a white edge so that it also stands out where it crosses the black outline
  const float x0 = x - 30, y0 = y + bodyH + 30, x1 = x + bodyW + nubW + 30, y1 = y - 30;
  canvas.drawWideLine(x0, y0, x1, y1, 15.0f, C_WHITE);
  canvas.drawWideLine(x0, y0, x1, y1, 9.0f, C_RED);
}

// ============================================================================
//  microSD card symbol (header), 14 x 18 px, to the left of the date
//    mounted  : white card with contacts
//    unreadable: yellow card with an "!"   (a card is inserted but can't be used)
//    none     : outlined card struck through in red
// ============================================================================
// `rightX` = right edge of the symbol, vertically centered on `cy`. Returns its left edge.
static int drawSdIcon(int rightX, int cy, SdState st) {
  const int w = 14, h = 18, cut = 5;
  const int x = rightX - w, y = cy - h / 2;

  if (st == SD_ABSENT) {
    canvas.drawLine(x, y, x + w - cut - 1, y, C_WHITE);                         // top
    canvas.drawLine(x + w - cut - 1, y, x + w - 1, y + cut, C_WHITE);           // cut corner
    canvas.drawLine(x + w - 1, y + cut, x + w - 1, y + h - 1, C_WHITE);         // right
    canvas.drawLine(x + w - 1, y + h - 1, x, y + h - 1, C_WHITE);               // bottom
    canvas.drawLine(x, y + h - 1, x, y, C_WHITE);                               // left
    canvas.drawWideLine(x - 2, y + h + 1, x + w + 1, y - 1, 1.2f, C_RED);       // strike-through
  } else {
    canvas.fillRect(x, y, w, h, st == SD_OK ? C_WHITE : C_YELLOW);
    canvas.fillTriangle(x + w - cut, y, x + w, y, x + w, y + cut, C_BLACK);     // the cut corner (header colour)
    if (st == SD_OK) {
      for (int k = 0; k < 3; k++) canvas.fillRect(x + 2 + k * 3, y + 2, 2, 5, C_BLACK);   // contacts
    } else {
      canvas.fillRect(x + 6, y + 5, 2, 7, C_BLACK);                             // "!"
      canvas.fillRect(x + 6, y + 14, 2, 2, C_BLACK);
    }
  }
  return x;
}

// ============================================================================
//  Header bar, shared by every page: title on the left, date + battery on the right
// ============================================================================
static void drawHeader(const char* title, const Reading& r) {
  const int W  = canvas.width();
  const int cy = HEADER_H / 2;

  canvas.fillRect(0, 0, W, HEADER_H, C_BLACK);

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextSize(1.0f);
  canvas.setTextColor(C_WHITE);
  canvas.setTextDatum(middle_left);
  canvas.drawString(title, 16, cy);

  int left = drawBatteryIcon(W - 12, cy, r.power.batOk,
                             r.power.batOk ? batteryPercent(r.power.batMv) : 0,
                             r.power.charging);
  int textLeft = left - 12;                            // left edge of what sits to the right of the SD symbol
  if (r.rtcOk) {
    canvas.setFont(&fonts::FreeSans12pt7b);
    canvas.setTextColor(C_WHITE);
    canvas.setTextDatum(middle_right);
    canvas.drawString(r.date, left - 12, cy);
    textLeft = left - 12 - canvas.textWidth(r.date);
  }
  drawSdIcon(textLeft - 10, cy, r.sd);
}

// ============================================================================
//  One forecast column (deliberately small): label, icon, temperature
// ============================================================================
// Temperature digits: blue when it freezes, red when it is hot, black otherwise (a colour only where it says something).
static uint16_t tempColor(float c) { return c <= 0.0f ? C_BLUE : c >= 30.0f ? C_RED : C_BLACK; }

static void drawForecastSlot(int cx, int top, const char* label, const ForecastSlot& f, uint16_t chip, uint16_t chipText) {
  const int iconS = 48;

  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextSize(1.0f);
  const int chipW = canvas.textWidth(label) + 18, chipH = 20;                 // the label sits on a coloured chip
  canvas.fillRoundRect(cx - chipW / 2, top - 2, chipW, chipH, 6, chip);
  canvas.setTextColor(chipText);
  canvas.setTextDatum(middle_center);
  canvas.drawString(label, cx, top - 2 + chipH / 2);

  drawWeatherIcon(f.valid ? f.icon : ICON_NONE, cx - iconS / 2, top + 22, iconS);

  char num[12];
  if (f.valid) snprintf(num, sizeof(num), "%.0f", f.tempC);
  else         snprintf(num, sizeof(num), "--");
  int w = tempWidth(STYLE_SMALL, num);
  drawTemp(cx - w / 2, top + 22 + iconS + 6, STYLE_SMALL, num, f.valid ? tempColor(f.tempC) : C_BLACK);
}

// ============================================================================
//  "No weather data" state: replaces the whole outdoor body (current + forecast)
// ============================================================================
static void drawWeatherError(const Reading& r, int top, int bottom) {
  const int cx    = WX_W / 2;
  const int iconS = 140;

  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.setTextSize(1.0f);
  const int titleH = canvas.fontHeight();
  canvas.setFont(&fonts::FreeSans12pt7b);
  const int reasonH = canvas.fontHeight();

  const int blockH = iconS + 4 + titleH + 2 + reasonH;
  const int y0     = top + ((bottom - top) - blockH) / 2;

  drawWeatherIcon(ICON_ERROR, cx - iconS / 2, y0, iconS);

  canvas.setTextColor(C_BLACK);
  canvas.setTextDatum(top_center);

  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.drawString("Weather unavailable", cx, y0 + iconS + 4);

  canvas.setFont(&fonts::FreeSans12pt7b);
  canvas.drawString(r.wxError[0] ? r.wxError : "No data received", cx, y0 + iconS + 4 + titleH + 2);
}

// Shortens `text` in place and appends "..." until it fits into maxW pixels (current font).
static void ellipsize(char* text, size_t cap, int maxW) {
  if (canvas.textWidth(text) <= maxW) return;
  char t[72];
  for (int n = (int)strlen(text) - 1; n >= 1; n--) {
    if (n > (int)sizeof(t) - 4) continue;
    memcpy(t, text, n);
    memcpy(t + n, "...", 4);                        // includes the terminating 0
    if (canvas.textWidth(t) <= maxW) { snprintf(text, cap, "%s", t); return; }
  }
}

// ============================================================================
//  The whole screen
// ============================================================================
static void drawScreen(const Reading& r) {
  const int W = canvas.width();
  const int H = canvas.height();
  canvas.fillScreen(C_WHITE);
  canvas.setTextSize(1.0f);

  drawHeader("Weather", r);

  // ================= body =================
  const int titleY = HEADER_H + 12;
  const int subY   = titleY + 26;
  const int ruleY  = subY + 24;
  const int bodyY  = ruleY + 12;

  canvas.setTextColor(C_BLACK);
  canvas.setTextDatum(top_left);

  // ---- titles ----
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(C_BLUE);                                  // each title in the colour of the rule under it
  canvas.drawString("OUTDOOR", 20, titleY);
  canvas.setTextColor(C_RED);
  canvas.drawString("INDOOR", WX_W + 20, titleY);
  canvas.setTextColor(C_BLACK);

  canvas.setFont(&fonts::FreeSans9pt7b);
  char sub[96];
  if (locationName[0]) {
    // a long place name is shortened so that " (Open-Meteo)" always stays visible
    const char* tail = " (Open-Meteo)";
    char place[48];
    snprintf(place, sizeof(place), "%s", locationName);
    ellipsize(place, sizeof(place), (WX_W - 40) - canvas.textWidth(tail));
    snprintf(sub, sizeof(sub), "%s%s", place, tail);
  } else {
    snprintf(sub, sizeof(sub), "Location not set");
  }
  canvas.drawString(sub, 20, subY);
  canvas.drawString("onboard SHT40", WX_W + 20, subY);

  // ---- accent rules + divider between the two fields (runs to the bottom) ----
  canvas.fillRect(0, ruleY, WX_W, 4, C_BLUE);
  canvas.fillRect(WX_W, ruleY, W - WX_W, 4, C_RED);
  canvas.drawFastVLine(WX_W, ruleY + 4, H - (ruleY + 4), C_BLACK);

  const int curH  = 130;
  const int halfW = WX_W / 2;

  if (!r.wxOk) {
    // ================= OUTDOOR: no data -> error state =================
    drawWeatherError(r, bodyY, H - 34);
  } else {
    // ================= OUTDOOR: current conditions =================
    // Two halves, no divider: icon on the left; temperature + descriptor on the right.
    {
      // left: big icon
      const int iconS = 116;
      drawWeatherIcon(r.wxOk ? r.wxIcon : ICON_NONE,
                      (halfW - iconS) / 2 + 4, bodyY + (curH - iconS) / 2, iconS);

      // right: temperature (shrunk to fit if needed) + descriptor
      const int tx   = halfW + 8;
      const int maxW = WX_W - 12 - tx;

      char num[12];
      if (r.wxOk) snprintf(num, sizeof(num), "%.1f", r.wxTempC);
      else        snprintf(num, sizeof(num), "--.-");

      TempStyle st = STYLE_BIG;
      for (st.numSize = 1.6f; st.numSize > 1.0f && tempWidth(st, num) > maxW; st.numSize -= 0.1f) {}
      canvas.setFont(st.numFont);
      canvas.setTextSize(st.numSize);
      const int tempH = canvas.fontHeight();

      const char* desc = (r.wxOk && r.wxDesc[0]) ? r.wxDesc : "";
      const lgfx::GFXfont* df = &fonts::FreeSansBold12pt7b;
      canvas.setFont(df);
      canvas.setTextSize(1.0f);
      if (canvas.textWidth(desc) > maxW) df = &fonts::FreeSansBold9pt7b;
      canvas.setFont(df);
      const int descH = canvas.fontHeight();

      const int gap    = 2;
      const int blockH = tempH + gap + descH;
      const int by     = bodyY + (curH - blockH) / 2;

      drawTemp(tx, by, st, num, r.wxOk ? tempColor(r.wxTempC) : C_BLACK);

      canvas.setFont(df);
      canvas.setTextSize(1.0f);
      canvas.setTextColor(C_BLACK);
      canvas.setTextDatum(top_left);
      canvas.drawString(desc, tx + 2, by + tempH + gap);
    }

    // ================= OUTDOOR: Morning | Afternoon | Night =================
    const int sepY = bodyY + curH + 4;
    canvas.drawFastHLine(20, sepY, WX_W - 40, C_BLACK);
  if (r.wxTomorrow) {                                   // the three slots below are TOMORROW's: white-on-black tag
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1.0f);
    const int tw = canvas.textWidth("TOMORROW");
    const int tx = WX_W - 20 - tw - 12;                 // tag rectangle, right-aligned with the line
    canvas.fillRect(tx, sepY - 20, tw + 12, 18, C_BLUE);
    canvas.setTextColor(C_WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("TOMORROW", tx + (tw + 12) / 2, sepY - 11);
  }
    {
      const int colW = (WX_W - 40) / 3;
      const int top  = sepY + 6;
      const int fcH  = 92;
      drawForecastSlot(20 + colW * 0 + colW / 2, top, "Morning",   r.morning,   C_YELLOW, C_BLACK);
      drawForecastSlot(20 + colW * 1 + colW / 2, top, "Afternoon", r.afternoon, C_RED,    C_WHITE);
      drawForecastSlot(20 + colW * 2 + colW / 2, top, "Night",     r.night,     C_BLUE,   C_WHITE);
      canvas.drawFastVLine(20 + colW * 1, top, fcH, C_BLACK);      // dividers
      canvas.drawFastVLine(20 + colW * 2, top, fcH, C_BLACK);
    }

  }

  // ================= INDOOR (smaller, tighter) =================
  {
    const int x = WX_W + 20;
    char num[12];

    if (r.shtOk) snprintf(num, sizeof(num), "%.1f", r.tempC);
    else         snprintf(num, sizeof(num), "--.-");
    drawTemp(x, bodyY, STYLE_MID, num, C_BLACK);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(C_BLACK);
    canvas.setTextDatum(top_left);
    canvas.drawString("temperature", x, bodyY + 46);

    char hum[12];
    if (r.shtOk) snprintf(hum, sizeof(hum), "%.0f %%", r.humPct);
    else         snprintf(hum, sizeof(hum), "-- %%");
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextSize(1.0f);
    canvas.drawString(hum, x, bodyY + 92);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.drawString("humidity", x, bodyY + 92 + 46);
  }

  // ================= timestamps: bottom of each field =================
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextSize(1.0f);
  canvas.setTextColor(C_BLACK);
  canvas.setTextDatum(middle_left);

  if (r.fetchNote[0]) {                                 // last update failed, older data is still shown
    char note[56];
    snprintf(note, sizeof(note), "%s", r.fetchNote);
    ellipsize(note, sizeof(note), WX_W - 40);
    canvas.setTextColor(C_RED);
    canvas.drawString(note, 20, H - 40);
    canvas.setTextColor(C_BLACK);
  }

  char line[64];
  snprintf(line, sizeof(line), "API last updated: %s", r.apiTs);
  canvas.drawString(line, 20, H - 18);

  canvas.drawString("Last sensor reading:", WX_W + 20, H - 38);
  canvas.drawString(r.rtcOk ? r.sensorTs : "RTC unavailable", WX_W + 20, H - 18);
}

// ============================================================================
//  "Connect to Device" page: QR code (left) | numbered instructions (right)
//  The screen stays like this for as long as the access point is up.
// ============================================================================

// First font whose rendering of `text` fits in `maxW`; falls back to the last one.
static const lgfx::GFXfont* fitFont(const char* text, int maxW, const lgfx::GFXfont* const* list, int n) {
  canvas.setTextSize(1.0f);
  for (int i = 0; i < n; i++) {
    canvas.setFont(list[i]);
    if (canvas.textWidth(text) <= maxW) return list[i];
  }
  return list[n - 1];
}

// Draws `text` at (x, y) in the current font. If it is wider than maxW it wraps
// (at a space when possible, otherwise mid-word). Returns the y below the last line.
static int drawWrapped(const char* text, int x, int y, int maxW) {
  const int len = (int)strlen(text);
  const int lh  = canvas.fontHeight();
  char line[96];
  int start = 0;
  while (start < len) {
    int n = len - start;
    if (n > (int)sizeof(line) - 1) n = (int)sizeof(line) - 1;
    while (n > 1) {                                   // longest prefix that fits
      memcpy(line, text + start, n);
      line[n] = 0;
      if (canvas.textWidth(line) <= maxW) break;
      n--;
    }
    if (start + n < len) {                            // not the last line: prefer to break at a space
      for (int k = n; k > 1; k--) {
        if (text[start + k - 1] == ' ') { n = k; break; }
      }
    }
    memcpy(line, text + start, n);
    line[n] = 0;
    canvas.drawString(line, x, y);
    y += lh;
    start += n;
  }
  return y;
}

// Blue numbered circle, `cx`/`cy` = center.
static void drawStepBadge(int cx, int cy, int number) {
  char t[4];
  snprintf(t, sizeof(t), "%d", number);
  canvas.fillCircle(cx, cy, 15, C_BLUE);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextSize(1.0f);
  canvas.setTextColor(C_WHITE);
  canvas.setTextDatum(middle_center);
  canvas.drawString(t, cx, cy);
  canvas.setTextColor(C_BLACK);      // back to the normal body text color
}

// ============================================================================
//  Slideshow page when there is no picture to show: an icon and what to do about it
// ============================================================================
static void drawPictureIcon(int x, int y, int s) {                // a framed picture with a sun and two hills
  const int w = s, h = s * 3 / 4, t = s / 28 < 3 ? 3 : s / 28;
  canvas.fillRect(x, y, w, h, C_BLACK);
  canvas.fillRect(x + t, y + t, w - 2 * t, h - 2 * t, C_WHITE);
  canvas.fillCircle(x + w * 7 / 10, y + h * 3 / 10, h / 8, C_YELLOW);
  canvas.fillTriangle(x + t, y + h - t, x + w * 4 / 10, y + h * 4 / 10, x + w * 7 / 10, y + h - t, C_GREEN);
  canvas.fillTriangle(x + w * 5 / 10, y + h - t, x + w * 7 / 10, y + h * 55 / 100, x + w - t, y + h - t, C_GREEN);
}

static void drawSlideMessage(const Reading& r, const char* title, const char* line1, const char* line2) {
  const int W = canvas.width();
  const int H = canvas.height();
  const int mid = W / 2;

  canvas.fillScreen(C_WHITE);
  drawHeader("Slideshow", r);
  canvas.drawFastVLine(mid, HEADER_H, H - HEADER_H, C_BLACK);
  const int iconS = 170;
  drawPictureIcon((mid - iconS) / 2, HEADER_H + (H - HEADER_H - iconS * 3 / 4) / 2, iconS);

  const int x = mid + 24;
  canvas.setTextColor(C_BLACK);
  canvas.setTextSize(1.0f);
  canvas.setTextDatum(top_left);
  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.drawString(title, x, HEADER_H + 90);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.drawString(line1, x, HEADER_H + 150);
  canvas.drawString(line2, x, HEADER_H + 174);
  canvas.drawString("Press G9 or G10 for another page.", x, HEADER_H + 222);
}

static void drawConnectScreen(const Reading& r, const ConnectInfo& ci) {
  const int W   = canvas.width();
  const int H   = canvas.height();
  const int mid = W / 2;

  canvas.fillScreen(C_WHITE);
  drawHeader("Connect to Device", r);
  canvas.drawFastVLine(mid, HEADER_H, H - HEADER_H, C_BLACK);     // divider in the middle

  canvas.setTextColor(C_BLACK);
  canvas.setTextSize(1.0f);

  // ---------------- access point could not start ----------------
  if (!ci.ok) {
    const int iconS = 140;
    drawWeatherIcon(ICON_ERROR, (mid - iconS) / 2, HEADER_H + (H - HEADER_H - iconS) / 2, iconS);

    const int x = mid + 24;
    canvas.setTextDatum(top_left);
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.drawString("Wi-Fi", x, HEADER_H + 90);
    canvas.drawString("unavailable", x, HEADER_H + 90 + 40);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.drawString("Couldn't start the access point.", x, HEADER_H + 180);
    canvas.drawString("Press G9 or G10 to go back.", x, HEADER_H + 204);
    return;
  }

  // ---------------- left: QR code ----------------
  {
    const int qrW   = 230;
    const int capH  = 28 + 4 + 20;                       // caption + hint
    const int blockH = qrW + 8 + capH;
    const int top   = HEADER_H + (H - HEADER_H - blockH) / 2;
    const int qx    = (mid - qrW) / 2;

    canvas.qrcode(ci.wifiQr, qx, top, qrW, 1, true);

    canvas.setTextDatum(top_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawString("Scan to join Wi-Fi", mid / 2, top + qrW + 8);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.drawString("Use your phone's camera app", mid / 2, top + qrW + 8 + 32);
  }

  // ---------------- right: instructions ----------------
  {
    const int x0    = mid + 22;
    const int tx    = x0 + 42;                           // text column (right of the badge)
    const int maxW  = W - 12 - tx;
    int y           = HEADER_H + 26;

    // step 1: join the Wi-Fi
    drawStepBadge(x0 + 15, y + 15, 1);
    canvas.setTextDatum(top_left);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawString("Connect to Wi-Fi", tx, y);
    y += 30;

    static const lgfx::GFXfont* const ssidFonts[] = { &fonts::FreeSansBold18pt7b, &fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b };
    canvas.setFont(fitFont(ci.ssid, maxW, ssidFonts, 3));
    y = drawWrapped(ci.ssid, tx, y, maxW) + 2;

    // password: small label, then the value in bold so it is easy to type by hand
    if (ci.password[0]) {
      canvas.setFont(&fonts::FreeSans9pt7b);
      canvas.drawString("Password", tx, y);
      y += canvas.fontHeight();
      static const lgfx::GFXfont* const pwFonts[] = { &fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b };
      canvas.setFont(fitFont(ci.password, maxW, pwFonts, 2));
      y = drawWrapped(ci.password, tx, y, maxW) + 22;
    } else {
      canvas.setFont(&fonts::FreeSans12pt7b);
      canvas.drawString("No password", tx, y);
      y += canvas.fontHeight() + 22;
    }

    // step 2: open the page
    drawStepBadge(x0 + 15, y + 15, 2);
    canvas.setTextDatum(top_left);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawString("Open this page", tx, y);
    y += 30;

    static const lgfx::GFXfont* const urlFonts[] = { &fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b };
    canvas.setFont(fitFont(ci.url, maxW, urlFonts, 2));
    canvas.drawString(ci.url, tx, y);

    // footnote (short enough for the right half)
    char note[64];
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(middle_left);
    canvas.drawString("Press G9 or G10 to leave.", x0, H - 40);
    snprintf(note, sizeof(note), "Closes after %d min idle.", ci.timeoutMin);
    canvas.drawString(note, x0, H - 18);
  }
}
