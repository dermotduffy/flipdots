/* 
Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>

#include "displaybuffer.h"

// **************
// These functions adapted from the Adafruit GFX Library.
// https://github.com/adafruit/Adafruit-GFX-Library
// **************

#ifndef _swap_uint8_t
#define _swap_uint8_t(a, b) { uint8_t t = a; a = b; b = t; }
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#endif

void buffer_draw_line(
  uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, PixelValue value,
  displaybuffer_t* buffer) {

  bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_uint8_t(x0, y0);
    _swap_uint8_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_uint8_t(x0, x1);
    _swap_uint8_t(y0, y1);
  }

  int8_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int8_t err = dx / 2;
  int8_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      buffer_draw_pixel(y0, x0, value, buffer);
    } else {
      buffer_draw_pixel(x0, y0, value, buffer);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void buffer_draw_bitmap(
  uint8_t x, uint8_t y,
  const uint8_t *bitmap, 
  uint8_t w, uint8_t h, PixelValue value,
  displaybuffer_t* buffer) {

  uint8_t xpos, ypos, byte_width = (w + 7) / 8;
  uint8_t byte = 0;

  for(ypos = 0; ypos < h; ypos++) {
    for(xpos = 0; xpos < w; xpos++) {
      if(xpos & 7) {
        byte >>= 1;
      } else {
        byte = bitmap[ypos * byte_width + xpos / 8];
      } 
      if(byte & 0x01) {
        buffer_draw_pixel(x+xpos, y+ypos, value, buffer);
      }
    }
  }
}

void buffer_draw_triangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2, PixelValue value,
    displaybuffer_t* buffer) {
  buffer_draw_line(x0, y0, x1, y1, value, buffer);
  buffer_draw_line(x1, y1, x2, y2, value, buffer);
  buffer_draw_line(x2, y2, x0, y0, value, buffer);
}

void buffer_fill_triangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    PixelValue value,
    displaybuffer_t* buffer) {
  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    _swap_int16_t(y0, y1); _swap_int16_t(x0, x1);
  }
  if (y1 > y2) {
    _swap_int16_t(y2, y1); _swap_int16_t(x2, x1);
  }
  if (y0 > y1) {
    _swap_int16_t(y0, y1); _swap_int16_t(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    buffer_draw_line(a, y0, a+(b-a), y0, value, buffer);
    return;
  }

  int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
  int32_t
    sa   = 0,
    sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) _swap_int16_t(a,b);
    buffer_draw_line(a, y, a+(b-a), y, value, buffer);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) _swap_int16_t(a,b);
    buffer_draw_line(a, y, a+(b-a), y, value, buffer);
  }
}

// Note: Coordinate based rather than w/h based as in original Adafruit library.
void buffer_fill_rectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
 PixelValue value, displaybuffer_t* buffer) {
  int16_t x_low = x0 <= x1 ? x0 : x1;
  int16_t x_high = x0 <= x1 ? x1 : x0;  

  for (int16_t i = x_low; i <= x_high; i++) {
    buffer_draw_line(i, y0, i, y1, value, buffer);
  }
}

void buffer_fill_circle(
    int16_t x0, int16_t y0, int16_t r,
    PixelValue value, displaybuffer_t* buffer) {
  buffer_draw_line(x0, y0-r, x0, y0+r, value, buffer);
  buffer_fill_circle_helper(x0, y0, r, 3, 0, value, buffer);
}

void buffer_fill_circle_helper(
    int16_t x0, int16_t y0, int16_t r,
    uint8_t cornername, int16_t delta,
    PixelValue value, displaybuffer_t* buffer) {
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      buffer_draw_line(x0+x, y0-y, x0+x, y0+y+delta, value, buffer);
      buffer_draw_line(x0+y, y0-x, x0+y, y0+x+delta, value, buffer);
    }
    if (cornername & 0x2) {
      buffer_draw_line(x0-x, y0-y, x0-x, y0+y+delta, value, buffer);
      buffer_draw_line(x0-y, y0-x, x0-y, y0+x+delta, value, buffer);
    }
  }
}
