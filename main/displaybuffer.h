#ifndef DISPLAYBUFFER_H
#define DISPLAYBUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include "flipdots-base.h"
#include "MatrixFontCommon.h"
#include "fonts-tdf.h"

typedef struct {
  bool data[DISPLAY_WIDTH][DISPLAY_HEIGHT];
  bool modified;
  uint8_t height, width;
} displaybuffer_t;

// ************************
// Display buffer functions
// ************************

void buffer_init(uint8_t width, uint8_t height, displaybuffer_t* buffer);
void buffer_fill(bool data, displaybuffer_t* buffer);
void buffer_wipe(displaybuffer_t* buffer);
void buffer_draw_pixel(
    uint8_t x, uint8_t y, bool data, displaybuffer_t* buffer);
void buffer_inverse(displaybuffer_t* buffer);

// BDF2C based fonts.
void buffer_bdf2c_draw_char(
    uint8_t x, uint8_t y, char c, const bitmap_font* font,
    displaybuffer_t* buffer);
void buffer_bdf2c_draw_string(
    uint8_t x, uint8_t y, const char* s, const bitmap_font* font,
    displaybuffer_t* buffer);

// The dot factory based fonts.
void buffer_tdf_draw_char(
    uint8_t x, uint8_t y, char c, const font_info_t* font,
    displaybuffer_t* buffer);
void buffer_tdf_draw_char_info(
    uint8_t x, uint8_t y, const font_char_info_t* c, const font_info_t* font,
    displaybuffer_t* buffer);
void buffer_tdf_draw_string(
    uint8_t x, uint8_t y, const char* s, int gap_between_chars,
    const font_info_t* font,
    displaybuffer_t* buffer);
void buffer_tdf_draw_string_centre(
    const char* s, int gap_between_chars,
    const font_info_t* font,
    displaybuffer_t* buffer);

void buffer_draw_line(
    uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool data,
    displaybuffer_t* buffer);
void buffer_draw_bitmap(
    uint8_t x, uint8_t y,
    const uint8_t *bitmap, 
    uint8_t w, uint8_t h, bool data,
    displaybuffer_t* buffer);
bool buffer_save_if_needed(
    displaybuffer_t* src, displaybuffer_t* dst);

#endif
