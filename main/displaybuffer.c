#include <assert.h>
#include <string.h>

#include "displaybuffer.h"
#include "MatrixFontCommon.h"

// ************************
// Display buffer functions
// ************************

static void inline raw_write_pixel(
    uint8_t x, uint8_t y, PixelValue value, displaybuffer_t* buffer) {
  if (value == PIXEL_INVERSE) {
    buffer->data[x][y] = !buffer->data[x][y];
  } else {
    buffer->data[x][y] = (bool)value;
  }
}

void buffer_init(uint8_t width, uint8_t height, displaybuffer_t* buffer) {
  assert(buffer != NULL);

  memset(buffer->data, 0, sizeof(*(buffer->data)));
  buffer->modified = false;
  buffer->height = height;
  buffer->width = width;
}

void buffer_fill(PixelValue value, displaybuffer_t* buffer) {
  assert(buffer != NULL);

  for (uint8_t x = 0; x < buffer->width; ++x) {
    for (uint8_t y = 0; y < buffer->height; ++y) {
      raw_write_pixel(x, y, value, buffer);
    }
  }
  buffer->modified = true;
}

void buffer_wipe(displaybuffer_t* buffer) {
  buffer_fill(PIXEL_BLACK, buffer);
}

void buffer_inverse(displaybuffer_t* buffer) {
  buffer_fill(PIXEL_INVERSE, buffer);
}

void buffer_draw_pixel(
    uint8_t x, uint8_t y, PixelValue value,
    displaybuffer_t* buffer) {
  assert(buffer != NULL);
  assert(x < buffer->width);
  assert(y < buffer->height);

  raw_write_pixel(x, y, value, buffer);

  buffer->modified = true;
}

void buffer_tdf_draw_char(
    uint8_t x, uint8_t y, PixelValue value,
    char c, const font_info_t* font,
    displaybuffer_t* buffer) {
  buffer_tdf_draw_char_info(x, y, value, font_get_char_info(c, font), font, buffer);
}

void buffer_tdf_draw_char_info(
    uint8_t x, uint8_t y, PixelValue value,
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
      if (font_get_pixel(char_info, col, row, font)) {
        raw_write_pixel(x+col, y+row, value, buffer);
      }
    }
  }
  buffer->modified = true;
}

void buffer_tdf_draw_string(
    uint8_t x, uint8_t y, PixelValue value,
    const char* s, int gap_between_chars,
    const font_info_t* font, displaybuffer_t* buffer) {
  assert(x < buffer->width);  
  assert(y < buffer->height);
  assert(font != NULL);
  assert(buffer != NULL);

  while (*s != '\0' && x < buffer->width) {
    const font_char_info_t* char_info = font_get_char_info(*(s++), font);
    buffer_tdf_draw_char_info(x, y, value, char_info, font, buffer);
    x += (char_info->width+gap_between_chars);
  }
}

void buffer_tdf_draw_string_centre(
    PixelValue value, 
    const char* s, int gap_between_chars,
    const font_info_t* font,
    displaybuffer_t* buffer) {
  int text_width_in_pixels = font_get_string_pixel_length(
      s, gap_between_chars, font);

  int start_x = 0;
  int start_y = (DISPLAY_HEIGHT - font->height) / 2;
  if (text_width_in_pixels < DISPLAY_WIDTH) {
    start_x = (DISPLAY_WIDTH - text_width_in_pixels) / 2;
  }

  buffer_tdf_draw_string(
      start_x, start_y, value, s, gap_between_chars, font, buffer);
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

void buffer_AND(PixelValue value,
                const displaybuffer_t* src, displaybuffer_t* dst) {
  assert(src && dst);
  assert(src->width == dst->width);
  assert(src->height == dst->height);

  for (uint8_t x = 0; x < src->width; ++x) {
    for (uint8_t y = 0; y < src->height; ++y) {
      if (src->data[x][y] & dst->data[x][y]) {
        raw_write_pixel(x, y, value, dst);
      }
    }
  }
  dst->modified = true;
}
