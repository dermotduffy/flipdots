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

void buffer_bdf2c_draw_char(
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

void buffer_bdf2c_draw_string(
    uint8_t x, uint8_t y, const char* s, const bitmap_font* font,
    displaybuffer_t* buffer) {
  assert(x < buffer->width);  
  assert(y < buffer->height);
  assert(font != NULL);
  assert(buffer != NULL);

  while (*s != '\0') {
    buffer_bdf2c_draw_char(x, y, *(s++), font, buffer);
    x += (font->Width+1);
  }
}

void buffer_tdf_draw_char(
    uint8_t x, uint8_t y, char c, const font_info_t* font,
    displaybuffer_t* buffer) {
  buffer_tdf_draw_char_info(x, y, font_get_char_info(c, font), font, buffer);
}

void buffer_tdf_draw_char_info(
    uint8_t x, uint8_t y,
    const font_char_info_t* char_info, const font_info_t* font,
    displaybuffer_t* buffer) {
  assert(font != NULL);
  assert(buffer != NULL);

  // Don't assert on missing character to avoid bad input causing issues, just
  // do nothing instead.
  if (char_info == NULL) {
    return;
  }

  for (int row = 0;
       row < font->height && (y+row) < buffer->height;
       ++row) {
    for (int col = 0;
         col < char_info->width && (x+col) < buffer->width;
         ++col) {
      buffer->data[x+col][y+row] =
          font_get_pixel(char_info, x+col, y+row, font);
    }
  }
  buffer->modified = true;
}

void buffer_tdf_draw_string(
    uint8_t x, uint8_t y, const char* s, int gap_between_chars,
    const font_info_t* font, displaybuffer_t* buffer) {
  assert(x < buffer->width);  
  assert(y < buffer->height);
  assert(font != NULL);
  assert(buffer != NULL);

  while (*s != '\0' && x < buffer->width) {
    const font_char_info_t* char_info = font_get_char_info(*(s++), font);
    buffer_tdf_draw_char_info(x, y, char_info, font, buffer);
    x += (char_info->width+gap_between_chars);
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
