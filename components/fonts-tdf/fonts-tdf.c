#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "fonts-tdf.h"

const font_char_info_t* font_get_char_info(uint8_t c, const font_info_t* font) {
  if (c < font->start_char || c > font->end_char) {
    return NULL;
  } else {
    return &(font->font_char_info[c - font->start_char]);
  }
}

bool font_get_pixel(
    const font_char_info_t* char_info,
    uint8_t x,
    uint8_t y,
    const font_info_t* font) {
  assert(char_info != NULL);

  if (y >= font->height || x >= char_info->width || x >= 8*sizeof(uint32_t)) {
    return false;
  }

  uint32_t row = font_get_row(char_info, y, font);
  return (row & 1<<((8*sizeof(uint32_t))-1-x));
}

uint32_t font_get_row(
    const font_char_info_t* char_info,
    uint8_t y,
    const font_info_t* font) {
  assert(char_info != NULL);

  if (y >= font->height) {
    return false;
  }

  uint8_t bytes_per_row = ceil(char_info->width / (double)8);

  // This function only supports fonts at most 32 bits wide.
  assert(bytes_per_row < sizeof(uint32_t));

  uint32_t row = 0;

  // Data assumed to be in big-endian format, and kept in this format.
  for (int i = 0; i < bytes_per_row; ++i) {
    row <<= (8*i);
    row |= font->data[char_info->offset+(y*bytes_per_row)+i];
  }
  // Shift the data to the 'left', consumer expected to trim to 'width' bits
  // from the left.
  row <<= (8*(sizeof(uint32_t) - bytes_per_row));
  return row;
}

int font_get_string_pixel_length(
    const char* s, int gap_between_chars, const font_info_t* font) {

  int length = 0;
  while (*s != '\0') {
    if (length > 0) {
      length += gap_between_chars;
    }
    const font_char_info_t* char_info = font_get_char_info(*s, font);
    length += char_info->width;
    s++;
  }

  return length;
}
