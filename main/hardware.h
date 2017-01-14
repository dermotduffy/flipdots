#ifndef HARDWARE_H
#define HARDWARE_H

//#include "driver/spi_master.h"

// ***********************************************
// All hardware specific logic belows in this file
// ***********************************************

//                                      +------------------------------------+
// +-------+     2 shift registers ==== | 14x28 Flipdot board -- dual FP2800 |
// | esp32 |____/                       |             Top board              |
// +-------+    \                       +------------------------------------+
//               2 shift registers ==== | 14x28 Flipdot board -- dial FP2800 |
//                                      |            Bottom board            |                      
//                                      |   Upside down: flipdots reversed   |
//                                      +------------------------------------+

#define PIN_TOP_SERIAL_OUT               23 // Serial data out (future: MOSI).
#define PIN_TOP_SERIAL_CLK               23 // Serial data clock (future: SCLK).
#define PIN_TOP_SR1_LATCH                18 // Serial latch for SR1 (cols)
#define PIN_TOP_SR2_LATCH                19 // Serial latch for SR2 (rows+data)
#define PIN_TOP_ENABLE                   27

#define PIN_BOT_SERIAL_OUT               5  // Serial data out (future: MOSI).
#define PIN_BOT_SERIAL_CLK               4  // Serial data clock (future: SCLK).
#define PIN_BOT_SR1_LATCH                0  // Serial latch for SR1 (cols)
#define PIN_BOT_SR2_LATCH                2  // Serial latch for SR2 (rows+data)
#define PIN_BOT_ENABLE                   26

#define FR2800_ENABLE_MS                  1 // Ms duration of enable pulse.

// FP2800A addresses are shifted out as the esp32 does not have a sufficient
// number of independent GPIO pins to drive 4 fully charged FP2800 chips.
//
// Below represent bit positions within the shift register datastream. To
// achieve a reduced number of shifts, rows and data are shifted together,
// columns are shifted separately. This makes it faster to scan a col at a time
// versus a row at a time.
//
// Columns (shift register #1)
#define FLIP_COLS_A0_BIT                 0
#define FLIP_COLS_A1_BIT                 1
#define FLIP_COLS_A2_BIT                 2
#define FLIP_COLS_B0_BIT                 3
#define FLIP_COLS_B1_BIT                 4

// Rows and data (shift regiser #2)
#define FLIP_ROWS_A0_BIT                 0
#define FLIP_ROWS_A1_BIT                 1
#define FLIP_ROWS_A2_BIT                 2
#define FLIP_ROWS_B1_BIT                 3 
#define FLIP_ROWS_NUM_ADDRESS_BITS       (FLIP_ROWS_B1_BIT+1)
#define FLIP_ROWS_DATA_BIT               4  // Hard-wired to Rows B0 bit.
#define FLIP_COLS_DATA_BIT               5

#define SPI_BITBANG                      1
#define SPI_NATIVE                       2
#define SPI_MODE                         SPI_BITBANG

typedef enum {
  FLIP_BOARD_TOP,
  FLIP_BOARD_BOTTOM,
} FlipBoard;

void init_hardware();
void init_spi();

#if SPI_MODE == SPI_BITBANG
void shift_byte(const uint8_t data_pin, const uint8_t clock_pin,
    const uint8_t latch_pin, uint8_t data);
#else
void shift_byte(uint8_t data, spi_device_handle_t* spi_device);
#endif

// **************************************************
// Raw writing / physical board translation functions
// **************************************************

// Convert between program-space and flipdot board space.
// Program space is a 27x27 grid (0-27).
//
// Board space is:

// +---------------+
// |Flip display #1|
// |(right way up) |
// |   1-28 cols   |
// |   1-14 rows   |
// +---------------+
// |Flip display #2|
// |(upside down)  |
// |   1-28 cols   |
// |   1-14 rows   |
// +---------------+

// Dual FP2800 display boards
// Rows are 1 - 14 (top to botton, chips are at the top)
// Cols are 1 - 28 (left to right)
bool translate_to_board_space(
    uint8_t x_in, uint8_t y_in, bool data_in,
    uint8_t* x_out, uint8_t* y_out, bool* data_out,
    FlipBoard* board);

uint8_t inline translate_board_col_to_address(uint8_t x) {
  uint8_t remainder = x % 7;
  uint8_t highbits = ((x-1) / 7); 
  
  if (remainder == 0) {
    remainder = 7;
  }   
  return (highbits << 3) | remainder;
}

uint8_t inline translate_board_row_to_address(uint8_t y) {
  bool offset = false; 
  bool high_bit = false; 
  if (y <= 7) {
    offset = true; 
  } else {
    high_bit = true;
  }
  return (high_bit<<3) | (((~y+offset)) & 0b111);
}

bool raw_write_bit(uint8_t col_in, uint8_t row_in, bool data_in);

#endif
