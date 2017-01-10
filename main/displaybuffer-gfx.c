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
// This function adapted from the Adafruit GFX Library.
// https://github.com/adafruit/Adafruit-GFX-Library
// **************

#ifndef _swap_uint8_t
#define _swap_uint8_t(a, b) { uint8_t t = a; a = b; b = t; }
#endif

void buffer_draw_line(
  uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool data,
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
      buffer_draw_pixel(y0, x0, data, buffer);
    } else {
      buffer_draw_pixel(x0, y0, data, buffer);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

// **************
// This function adapted from the Adafruit GFX Library
// https://github.com/adafruit/Adafruit-GFX-Library
// **************

void buffer_draw_bitmap(
  uint8_t x, uint8_t y,
  const uint8_t *bitmap, 
  uint8_t w, uint8_t h, bool data,
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
        buffer_draw_pixel(x+xpos, y+ypos, true, buffer);
      }
    }
  }
}
