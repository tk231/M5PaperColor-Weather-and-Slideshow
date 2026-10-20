// slideshow.h - what the Slideshow page needs that has nothing to do with the hardware:
//   - how a picture has to be drawn (by the panel driver's own dithering, or as it is),
//   - where it goes on the screen,
//   - which picture comes next (in the order of the playlist, or shuffled).
// The drawing itself (the canvas, the card, the panel refresh) is in the sketch.
#pragma once
#include <stdint.h>
#include <string.h>
#include "gallery.h"

namespace slide {

constexpr size_t HEAD_BYTES = 1024;       // how much of the start of a picture file is read to learn how to draw it
constexpr size_t MAX_FILE_BYTES = 4UL * 1024 * 1024;   // a picture file bigger than this is not shown (the page only uploads up to 3 MB)

// How drawing a portrait picture turned went (this type is here, not in the sketch: the Arduino IDE writes a prototype for every
// function of a sketch near the top of the file, and a type used by a function must be known by then).
enum TurnResult : uint8_t { TURN_OK, TURN_NO_MEMORY, TURN_FAILED };

enum DrawMode : uint8_t {
  DRAW_QUALITY,                           // epd_quality: the panel driver dithers the picture while drawing it
  DRAW_FASTEST                            // epd_fastest: the picture is already six colours, each pixel just takes the nearest one
};

// How to draw a picture, from the first bytes of its PNG file.
//   - A picture made by the page's editor carries a note ("pcedit", a PNG tEXt chunk right after the header) that says how its colours
//     were prepared: "fs" and "nearest" mean "already six colours" -> DRAW_FASTEST; "panel" means "plain RGB, let the panel dither it"
//     -> DRAW_QUALITY.
//   - A picture without our note (made elsewhere): a palette PNG (colour type 3) is taken to be six colours already -> DRAW_FASTEST,
//     anything else -> DRAW_QUALITY (dithering a picture that is already six colours does no harm; skipping the dithering of a
//     photograph would ruin it).
//   - Anything that is not a readable PNG header -> DRAW_QUALITY.
inline DrawMode drawMode(const uint8_t* b, size_t n) {
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
  if (!b || n < 33 || memcmp(b, sig, 8) != 0 || memcmp(b + 12, "IHDR", 4) != 0) return DRAW_QUALITY;
  const uint8_t colourType = b[25];
  size_t o = 33;                                          // the first chunk after IHDR
  while (o + 12 <= n) {
    const uint32_t len = gallery::be32(b + o);
    const uint8_t* type = b + o + 4;
    if (memcmp(type, "IDAT", 4) == 0 || memcmp(type, "IEND", 4) == 0 || len > 0x7FFFFFFFu) break;
    if (memcmp(type, "tEXt", 4) == 0 && (size_t)len + o + 8 <= n) {
      const uint8_t* d = b + o + 8;
      size_t k = 0;
      while (k < len && d[k] != 0) k++;
      if (k == 6 && memcmp(d, "pcedit", 6) == 0 && k < len) {
        char text[320];
        size_t tl = len - k - 1;
        if (tl >= sizeof(text)) tl = sizeof(text) - 1;
        memcpy(text, d + k + 1, tl);
        text[tl] = 0;
        const char* v = gallery::jsonValue(text, text + tl, "dither");
        if (v && (strncmp(v, "\"fs\"", 4) == 0 || strncmp(v, "\"nearest\"", 9) == 0)) return DRAW_FASTEST;
        return DRAW_QUALITY;                              // "panel", or a note we can't read
      }
    }
    o += 12 + (size_t)len;
  }
  return colourType == 3 ? DRAW_FASTEST : DRAW_QUALITY;
}

// A picture shown as large as possible without cutting anything off, in the middle of the screen. (A picture of the same shape as
// the screen, like 600x400 on a 600x400 screen, fills it exactly. A portrait picture on the landscape screen is shown upright with a
// white border left and right.)
struct Placement { float scale; int x, y, w, h; };
inline Placement fit(int imgW, int imgH, int scrW, int scrH) {
  Placement p = { 1.0f, 0, 0, 0, 0 };
  if (imgW <= 0 || imgH <= 0 || scrW <= 0 || scrH <= 0) return p;
  const float sx = (float)scrW / imgW, sy = (float)scrH / imgH;
  p.scale = sx < sy ? sx : sy;
  p.w = (int)(imgW * p.scale);
  p.h = (int)(imgH * p.scale);
  p.x = (scrW - p.w) / 2;
  p.y = (scrH - p.h) / 2;
  return p;
}

// ---- portrait pictures on the landscape screen ------------------------------------------------------------
// A portrait picture (like the 400x600 ones the editor makes) can fill the screen if it is drawn turned a quarter turn: it then
// looks sideways, and the person turns the frame to look at it. Which way is a setting (the two ways differ by half a turn).
enum PortraitMode : uint8_t {
  PORTRAIT_TURN_CW  = 0,      // "turn the frame clockwise": the picture is drawn turned counter-clockwise, so that it is upright once the frame is turned
  PORTRAIT_TURN_CCW = 1,      // "turn the frame counter-clockwise": the picture is drawn turned clockwise
  PORTRAIT_UPRIGHT  = 2,      // keep it upright, smaller, with a border at the sides (the frame stays as it is)
  PORTRAIT_MODES    = 3
};

struct Layout {
  bool  rotate;               // the picture is drawn into a buffer of its own and then turned a quarter turn into the canvas
  bool  clockwise;            // ... and this is the way the PICTURE is turned
  float scale;                // the scale the picture file is drawn at
  int   dw, dh;               // the size of the picture as drawn at that scale (before it is turned)
  int   x, y, w, h;           // where the finished picture is on the screen
};

inline Layout layout(int imgW, int imgH, int scrW, int scrH, uint8_t portraitMode) {
  Layout l = { false, false, 1.0f, 0, 0, 0, 0, 0, 0 };
  if (imgW <= 0 || imgH <= 0 || scrW <= 0 || scrH <= 0) return l;
  if (imgH > imgW && scrW >= scrH && portraitMode < PORTRAIT_UPRIGHT) {        // a portrait picture on a landscape screen
    const float sx = (float)scrW / imgH, sy = (float)scrH / imgW;              // turned, it is imgH wide and imgW high
    l.rotate = true;
    l.clockwise = (portraitMode == PORTRAIT_TURN_CCW);
    l.scale = sx < sy ? sx : sy;
    l.dw = (int)(imgW * l.scale);
    l.dh = (int)(imgH * l.scale);
    l.w = l.dh;
    l.h = l.dw;
    l.x = (scrW - l.w) / 2;
    l.y = (scrH - l.h) / 2;
    return l;
  }
  const Placement p = fit(imgW, imgH, scrW, scrH);
  l.scale = p.scale; l.dw = p.w; l.dh = p.h; l.x = p.x; l.y = p.y; l.w = p.w; l.h = p.h;
  return l;
}

// Copies a picture (sw x sh pixels of 16 bits, rows srcStride pixels apart) into dst (rows dstStride pixels apart), turned a quarter turn,
// with the top-left corner of the turned picture at (x0, y0). Clockwise: the picture's top edge ends up on the right; counter-clockwise: on
// the left. A plain copy, so every pixel is exact (the picture's colours are never touched).
inline void rotateQuarter(const uint16_t* src, int srcStride, int sw, int sh, bool clockwise, uint16_t* dst, int dstStride, int x0, int y0) {
  for (int y = 0; y < sh; y++) {
    const uint16_t* row = src + (size_t)y * srcStride;
    for (int x = 0; x < sw; x++) {
      const int X = clockwise ? x0 + (sh - 1 - y) : x0 + y;
      const int Y = clockwise ? y0 + x            : y0 + (sw - 1 - x);
      dst[(size_t)Y * dstStride + X] = row[x];
    }
  }
}

// Is there anything to show at all? (A picture takes part if it is switched on and is a readable PNG.)
inline bool anyToShow(const gallery::Library& l) {
  for (int i = 0; i < l.n; i++) if (l.items[i].on && l.items[i].ok) return true;
  return false;
}

// ---- which picture is next ------------------------------------------------------------------------------------
// Only pictures that are switched on and are readable PNGs take part. In playlist order: the one after the previous one (if the
// previous one is gone or switched off: the one that has taken its place). Shuffled: every picture once, in random order, then a
// new random order; the same picture never twice in a row (unless it is the only one). The bag is made again whenever the set of
// pictures, or their order, changes.
class Picker {
 public:
  Picker() { reset(); }
  void reset() { last_[0] = 0; lastPos_ = 0; bagN_ = 0; bagPos_ = 0; fp_ = 0; }
  const char* last() const { return last_; }

  // Index into lib.items of the picture to show next, or -1 if there is none.
  int next(const gallery::Library& l, bool shuffle, uint32_t (*rnd)()) {
    int en[gallery::MAX_ITEMS];
    int n = 0;
    for (int i = 0; i < l.n; i++) if (l.items[i].on && l.items[i].ok) en[n++] = i;
    if (n == 0) { last_[0] = 0; lastPos_ = 0; bagN_ = 0; bagPos_ = 0; return -1; }

    int lastK = -1;
    for (int k = 0; k < n; k++) if (strcmp(l.items[en[k]].id, last_) == 0) { lastK = k; break; }

    int pos;
    if (!shuffle) {
      pos = lastK >= 0 ? (lastK + 1) % n : lastPos_ % n;
    } else {
      const uint32_t f = fingerprint(l, en, n);
      if (f != fp_ || bagN_ != n || bagPos_ >= bagN_) {
        for (int k = 0; k < n; k++) bag_[k] = (uint8_t)k;
        for (int k = n - 1; k > 0; k--) { int j = (int)(rnd() % (uint32_t)(k + 1)); uint8_t t = bag_[k]; bag_[k] = bag_[j]; bag_[j] = t; }
        if (n > 1 && lastK >= 0 && bag_[0] == lastK) { uint8_t t = bag_[0]; bag_[0] = bag_[1]; bag_[1] = t; }
        bagN_ = n; bagPos_ = 0; fp_ = f;
      }
      pos = bag_[bagPos_++];
    }
    snprintf(last_, sizeof(last_), "%s", l.items[en[pos]].id);
    lastPos_ = pos;
    return en[pos];
  }

 private:
  static uint32_t fingerprint(const gallery::Library& l, const int* en, int n) {          // FNV-1a over the ids, in order
    uint32_t h = 2166136261u;
    for (int k = 0; k < n; k++) {
      for (const char* c = l.items[en[k]].id; *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
      h ^= 0xFF; h *= 16777619u;
    }
    return h;
  }
  char     last_[gallery::ID_MAX + 1];
  int      lastPos_;
  uint8_t  bag_[gallery::MAX_ITEMS];
  int      bagN_, bagPos_;
  uint32_t fp_;
};

}  // namespace slide
