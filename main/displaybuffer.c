#include <assert.h>
#include <string.h>
#include "displaybuffer.h"
#include "MatrixFontCommon.h"

// ************************
// Display buffer functions
// ************************

void buffer_init(uint8_t width, uint8_t height, displaybuffer_t* buffer) {
  assert(buffer != NULL);

  memset(buffer->data, 0, sizeof(*(buffer->data)));
  buffer->modified = false;
  buffer->height = height;
  buffer->width = width;
}

void buffer_fill(bool data, displaybuffer_t* buffer) {
  assert(buffer != NULL);

  for (uint8_t x = 0; x < buffer->width; ++x) {
    for (uint8_t y = 0; y < buffer->height; ++y) {
      buffer->data[x][y] = data;
    }
  }
  buffer->modified = true;
}

void buffer_wipe(displaybuffer_t* buffer) {
  buffer_fill(false, buffer);
}

void buffer_draw_pixel(uint8_t x, uint8_t y, bool data, displaybuffer_t* buffer) {
  assert(buffer != NULL);
  assert(x < buffer->width);
  assert(y < buffer->height);

  buffer->data[x][y] = data;
  buffer->modified = true;
}

void buffer_inverse(displaybuffer_t* buffer) {
  assert(buffer != NULL);

  for (uint8_t x = 0; x < buffer->width; ++x) {    
    for (uint8_t y = 0; y < buffer->height; ++y) {
      buffer->data[x][y] = !buffer->data[x][y];
    }
  }
  buffer->modified = true;
}

void buffer_draw_character(
    uint8_t x, uint8_t y, char c, const bitmap_font* font,
    displaybuffer_t* buffer) {
  assert(font != NULL);
  assert(buffer != NULL);

  for (int row = 0;
       row < font->Height && (y+row) < buffer->height;
       ++row) {
    uint16_t pixel_row = getBitmapFontRowAtXY(c, row, font); 

    for (int col = 0;
         col < font->Width && (x+col) < buffer->width;
         ++col) {
      buffer->data[x+col][y+row] = pixel_row & (0x80>>col);
    }
  }
  buffer->modified = true;
}

void buffer_draw_string(
    uint8_t x, uint8_t y, const char* s, const bitmap_font* font,
    displaybuffer_t* buffer) {
  assert(x < buffer->width);  
  assert(y < buffer->height);
  assert(font != NULL);
  assert(buffer != NULL);

  while (*s != '\0') {
    buffer_draw_character(x, y, *(s++), font, buffer);
    x += (font->Width+1);
  }
}

bool buffer_save_if_needed(displaybuffer_t* src, displaybuffer_t* dst) {
  assert(src != NULL);
  assert(dst != NULL);

  if (src->modified) {
    memcpy(dst, src, sizeof(*dst));
    src->modified = false;
    dst->modified = true;
    return true;
  }
  return false;
}
