#ifndef DISPLAYBUFFER_H
#define DISPLAYBUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "flipdots-base.h"
#include "fonts-tdf.h"
#include "MatrixFontCommon.h"

typedef struct {
  bool data[DISPLAY_WIDTH][DISPLAY_HEIGHT];
  bool modified;
  uint8_t height, width;
} displaybuffer_t;

typedef enum {
  PIXEL_BLACK,
  PIXEL_YELLOW,
  PIXEL_INVERSE,
} PixelValue;

// ************************
// Display buffer functions
// ************************

void buffer_init(uint8_t width, uint8_t height, displaybuffer_t* buffer);

void buffer_fill(PixelValue value, displaybuffer_t* buffer);
void buffer_wipe(displaybuffer_t* buffer);
void buffer_inverse(displaybuffer_t* buffer);

// Take a source displaybuffer, and fill on the dst displaybuffer
// whenever a source pixel is true.
void buffer_fill_from_template(
    PixelValue value,
    const displaybuffer_t* src, displaybuffer_t* dst);

void buffer_AND(
    PixelValue value,
    const displaybuffer_t* a,
    const displaybuffer_t* b,
    displaybuffer_t* out);

PixelValue buffer_get_pixel(uint8_t x, uint8_t y, displaybuffer_t* buffer);

// Drawing methods return true if the draw was free of collisions
// otherwise false if some/all of the drawing collided with the edges of the
// display.

bool buffer_draw_pixel(
    uint8_t x, uint8_t y, PixelValue value, displaybuffer_t* buffer);

// The dot factory based fonts.
bool buffer_tdf_draw_char(
    uint8_t x, uint8_t y, PixelValue value, char c, const font_info_t* font,
    displaybuffer_t* buffer);
bool buffer_tdf_draw_char_info(
    uint8_t x, uint8_t y, PixelValue value,
    const font_char_info_t* c, const font_info_t* font,
    displaybuffer_t* buffer);
bool buffer_tdf_draw_string(
    uint8_t x, uint8_t y, PixelValue value, const char* s,
    int gap_between_chars, const font_info_t* font,
    displaybuffer_t* buffer);
bool buffer_tdf_draw_string_centre(
    int y,
    PixelValue value, const char* s, int gap_between_chars,
    const font_info_t* font,
    displaybuffer_t* buffer);

// Adafruit GFX adaptations.
bool buffer_draw_line(
    uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, PixelValue value,
    displaybuffer_t* buffer);
bool buffer_draw_bitmap(
    uint8_t x, uint8_t y,
    const uint8_t *bitmap,
    uint8_t w, uint8_t h, PixelValue value,
    displaybuffer_t* buffer);
bool buffer_draw_triangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    PixelValue value,
    displaybuffer_t* buffer);
bool buffer_fill_triangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    PixelValue value,
    displaybuffer_t* buffer);
bool buffer_fill_circle(
    int16_t x0, int16_t y0, int16_t r,
    PixelValue value, displaybuffer_t* buffer);
bool buffer_fill_circle_helper(
    int16_t x0, int16_t y0, int16_t r,
    uint8_t cornername, int16_t delta,
    PixelValue value, displaybuffer_t* buffer);
bool buffer_fill_circle_centre(
    int16_t r, PixelValue value, displaybuffer_t* buffer);
bool buffer_fill_rectangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    PixelValue value, displaybuffer_t* buffer);

bool buffer_save_if_needed(
    displaybuffer_t* src, displaybuffer_t* dst);

#endif
