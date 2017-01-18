#ifndef FONTS_TDF_H_
#define FONTS_TDF_H_

#include <inttypes.h>
#include <stdbool.h>

// A font library based on The Dot Factory
// (http://www.eran.io/the-dot-factory-an-lcd-font-and-image-generator/), an
// application that converts text to C structs when given a font + size.
//
// The Dot Factory settings to work with this library:
// - (Defaults except):
// - Height in bits.
// - Generate space bitmap.
// - Bitmaps: const uint8_t {0}_bitmaps
// - Character info: const font_char_info_t {0}_descriptors
// - Font info: const font_info_t {0}_font_info
// - Width: const uint_8 {0}_width
// - Height: const uint_8 {0}_height
//
// Resultant .h file modified to #include
// - stdint.h
// - font-tdf.h (this file)
//
// Resultant .c file modified to include:
// - The resultant .h file.

typedef struct {
  const uint8_t width;
  const uint8_t height;

  // Offset of the character's bitmap in bytes, into the FONT_INFO's data array.
  const uint16_t offset;
  
} font_char_info_t;

typedef struct
{
  const uint8_t height;

  // The first character in the font.
  const uint8_t start_char;

  // The last character in the font.
  const uint8_t end_char;

  // Pointer to array of char information.
  const font_char_info_t* font_char_info;
     
  // Pointer to generated array of character visual representation.
  const uint8_t* data;
    
} font_info_t;

const font_char_info_t* font_get_char_info(
    uint8_t c, const font_info_t* font);
bool font_get_pixel(
    const font_char_info_t* c, uint8_t x, uint8_t y, const font_info_t* font);
uint32_t font_get_row(
    const font_char_info_t* c, uint8_t y, const font_info_t* font);
int font_get_string_pixel_length(
    const char* s, int gap_between_chars, const font_info_t* font);

#endif
