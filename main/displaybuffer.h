#ifndef DISPLAYBUFFER_H
#define DISPLAYBUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include "flipdots-base.h"
#include "MatrixFontCommon.h"

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
void buffer_draw_character(
    uint8_t x, uint8_t y, char c, const bitmap_font* font,
    displaybuffer_t* buffer);
void buffer_draw_string(
    uint8_t x, uint8_t y, const char* s, const bitmap_font* font,
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
