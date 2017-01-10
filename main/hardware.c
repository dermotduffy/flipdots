#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "hardware.h"

#include "flipdots-base.h"

void init_hardware() {
  gpio_set_direction(PIN_TOP_ENABLE, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_BOT_ENABLE, GPIO_MODE_OUTPUT);
}

void init_spi() {
  gpio_set_direction(PIN_TOP_SERIAL_OUT, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_TOP_SERIAL_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_TOP_SR1_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_TOP_SR2_LATCH, GPIO_MODE_OUTPUT);

  gpio_set_direction(PIN_BOT_SERIAL_OUT, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_BOT_SERIAL_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_BOT_SR1_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_BOT_SR2_LATCH, GPIO_MODE_OUTPUT);
}

void shift_byte(const uint8_t data_pin, const uint8_t clock_pin,
    const uint8_t latch_pin, uint8_t data) {
  uint8_t len = sizeof(data)*8;

  for (int i = len-1; i >= 0; --i) {
      gpio_set_level(data_pin, data & (1<<i));
      gpio_set_level(clock_pin, false);
      gpio_set_level(clock_pin, true);
  }
  gpio_set_level(latch_pin, false);
  gpio_set_level(latch_pin, true);
}

bool translate_to_board_space(
    uint8_t x_in, uint8_t y_in, bool data_in,
    uint8_t* x_out, uint8_t* y_out, bool* data_out,
    FlipBoard* board) {
  assert (x_out != NULL); 
  assert (y_out != NULL); 
  assert (board != NULL); 

  if (x_in >= DISPLAY_WIDTH || y_in >= DISPLAY_HEIGHT) {
    return false;
  }

  if (y_in >= FLIP_BOARD_HEIGHT) {
    *board = FLIP_BOARD_BOTTOM;
  } else {
    *board = FLIP_BOARD_TOP;     
  }

  if (*board == FLIP_BOARD_TOP) {
    *y_out = y_in + 1;  // Boards rows are indexed from 1.
    *x_out = FLIP_BOARD_WIDTH - x_in; // Boards cols are index right to left.
    *data_out = data_in;
  } else {
    // Bottom display is upside down.
    *y_out = FLIP_BOARD_HEIGHT - (y_in - FLIP_BOARD_HEIGHT);
    *x_out = x_in + 1; 

    // TODO: Physically flip all the dots on the board, remove this hack.
    // Temporary hack until all flipdots are manually/physically
    // reversed such that the two boards can be joined and visually
    // look consistent.
    if (y_in <= 21 || x_in >= 21) {
      *data_out = !data_in;
    } else {
      *data_out = data_in;
    }
  }

  return true;
}

bool raw_write_bit(uint8_t col_in, uint8_t row_in, bool data_in) {
  FlipBoard board;
  uint8_t board_row, board_col;
  bool board_data;
  if (!translate_to_board_space(
      col_in, row_in, data_in, &board_col, &board_row, &board_data, &board)) {
    return false;
  }

  uint8_t row_sr_output =
      (board_data << FLIP_COLS_DATA_BIT) |
      ((!board_data) << FLIP_ROWS_DATA_BIT) |
      translate_board_row_to_address(board_row);
  uint8_t col_sr_output = translate_board_col_to_address(board_col);

  uint8_t pin_data, pin_clock, pin_latch_col, pin_latch_row, pin_enable;

  if (board == FLIP_BOARD_TOP) {
    pin_data = PIN_TOP_SERIAL_OUT;
    pin_clock = PIN_TOP_SERIAL_CLK;
    pin_latch_col = PIN_TOP_SR1_LATCH;
    pin_latch_row = PIN_TOP_SR2_LATCH;
    pin_enable = PIN_TOP_ENABLE;
  } else {
    pin_data = PIN_BOT_SERIAL_OUT;
    pin_clock = PIN_BOT_SERIAL_CLK;
    pin_latch_col = PIN_BOT_SR1_LATCH;
    pin_latch_row = PIN_BOT_SR2_LATCH;
    pin_enable = PIN_BOT_ENABLE;
  }

  shift_byte(pin_data, pin_clock, pin_latch_col, col_sr_output);
  shift_byte(pin_data, pin_clock, pin_latch_row, row_sr_output);

  gpio_set_level(pin_enable, true);
  vTaskDelay(FR2800_ENABLE_MS / portTICK_PERIOD_MS);
  gpio_set_level(pin_enable, false);

  return true;
}
