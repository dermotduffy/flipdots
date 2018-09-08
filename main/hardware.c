#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "flipdots-base.h"
#include "hardware.h"

#define SPI_SPEED_HZ  10000000

#if SPI_MODE == SPI_NATIVE
static spi_device_handle_t spi_top_cols;
static spi_device_handle_t spi_top_rows;
static spi_device_handle_t spi_bot_cols;
static spi_device_handle_t spi_bot_rows;
#endif

void hardware_setup() {
  // By default on esp32 certain pins (e.g. 13, 14) are not in GPIO mode on
  // boot. Set all pins into GPIO_MODE_OUTPUT.
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask =
      // Shift register pins.
      (1ULL << PIN_TOP_SERIAL_OUT) |
      (1ULL << PIN_TOP_SERIAL_CLK) |
      (1ULL << PIN_TOP_SR1_LATCH) |
      (1ULL << PIN_TOP_SR2_LATCH) |
      (1ULL << PIN_BOT_SERIAL_OUT) |
      (1ULL << PIN_BOT_SERIAL_CLK) |
      (1ULL << PIN_BOT_SR1_LATCH) |
      (1ULL << PIN_BOT_SR2_LATCH) |

      // Flipdot FP2800 enable (latch) pins.
      (1ULL << PIN_TOP_ENABLE) |
      (1ULL << PIN_BOT_ENABLE);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

#if SPI_MODE == SPI_NATIVE
  spi_host_device_t host_top = HSPI_HOST;
  spi_host_device_t host_bot = VSPI_HOST;

  spi_bus_config_t default_buscfg = {
    .miso_io_num=-1,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1
  };

  spi_bus_config_t spi_top_buscfg = default_buscfg;
  spi_top_buscfg.mosi_io_num=PIN_TOP_SERIAL_OUT;
  spi_top_buscfg.sclk_io_num=PIN_TOP_SERIAL_CLK;
  ESP_ERROR_CHECK(spi_bus_initialize(host_top, &spi_top_buscfg, 1));

  spi_bus_config_t spi_bot_buscfg = default_buscfg;
  spi_bot_buscfg.mosi_io_num=PIN_BOT_SERIAL_OUT;
  spi_bot_buscfg.sclk_io_num=PIN_BOT_SERIAL_CLK;
  ESP_ERROR_CHECK(spi_bus_initialize(host_bot, &spi_bot_buscfg, 2));

  spi_device_interface_config_t default_cfg = {
    .clock_speed_hz=SPI_SPEED_HZ,
    .mode=0,
    .queue_size=1,
    .pre_cb=NULL,
    .post_cb=NULL,
  };

  spi_device_interface_config_t spi_top_cols_cfg = default_cfg;
  spi_top_cols_cfg.spics_io_num=PIN_TOP_SR1_LATCH;
  ESP_ERROR_CHECK(
      spi_bus_add_device(host_top, &spi_top_cols_cfg, &spi_top_cols));

  spi_device_interface_config_t spi_top_rows_cfg = default_cfg;
  spi_top_rows_cfg.spics_io_num=PIN_TOP_SR2_LATCH;
  ESP_ERROR_CHECK(
      spi_bus_add_device(host_top, &spi_top_rows_cfg, &spi_top_rows));

  spi_device_interface_config_t spi_bot_cols_cfg = default_cfg;
  spi_bot_cols_cfg.spics_io_num=PIN_BOT_SR1_LATCH;
  ESP_ERROR_CHECK(
      spi_bus_add_device(host_bot, &spi_bot_cols_cfg, &spi_bot_cols));

  spi_device_interface_config_t spi_bot_rows_cfg = default_cfg;
  spi_bot_rows_cfg.spics_io_num=PIN_BOT_SR2_LATCH;
  ESP_ERROR_CHECK(
      spi_bus_add_device(host_bot, &spi_bot_rows_cfg, &spi_bot_rows));
#endif
}

#if SPI_MODE == SPI_BITBANG
void shift_byte(
    const uint8_t data_pin, const uint8_t clock_pin, const uint8_t latch_pin,
    uint8_t data) {
  uint8_t len = sizeof(data) * 8;

  for (int i = len-1; i >= 0; --i) {
      gpio_set_level(data_pin, data & (1<<i));
      gpio_set_level(clock_pin, false);
      gpio_set_level(clock_pin, true);
  }
  gpio_set_level(latch_pin, false);
  gpio_set_level(latch_pin, true);
}
#else
void shift_byte(uint8_t data, spi_device_handle_t* spi_device) {

  spi_transaction_t spi_trans;
  memset(&spi_trans, 0, sizeof(spi_trans));
  spi_trans.length = sizeof(data) * 8;  // Transaction length is in bits.
  spi_trans.tx_data[0] = data;
  
  spi_trans.flags = SPI_TRANS_USE_TXDATA;

  ESP_ERROR_CHECK(spi_device_transmit(*spi_device, &spi_trans));
}

#endif

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

  uint8_t x_in_r, y_in_r;

  switch (DISPLAY_ROTATION) {
    case DISPLAY_ROTATION_0:
      x_in_r = x_in;
      y_in_r = y_in;
      break;

    case DISPLAY_ROTATION_90:
      x_in_r = DISPLAY_HEIGHT - y_in - 1;
      y_in_r = x_in;
      break;

    case DISPLAY_ROTATION_180:
      y_in_r = DISPLAY_HEIGHT - y_in - 1;
      x_in_r = DISPLAY_WIDTH - x_in - 1;
      break; 

    case DISPLAY_ROTATION_270:
      y_in_r = DISPLAY_WIDTH - x_in - 1;
      x_in_r = y_in;
      break;
  }

  if (y_in_r >= FLIP_BOARD_HEIGHT) {
    *board = FLIP_BOARD_BOTTOM;
  } else {
    *board = FLIP_BOARD_TOP;     
  }

  if (*board == FLIP_BOARD_TOP) {
    *y_out = y_in_r + 1;  // Boards rows are indexed from 1.
    *x_out = FLIP_BOARD_WIDTH - x_in_r; // Boards cols are index right to left.
    *data_out = data_in;
  } else {
    // Bottom display is upside down.
    *y_out = FLIP_BOARD_HEIGHT - (y_in_r - FLIP_BOARD_HEIGHT);
    *x_out = x_in_r + 1; 

    // The bottom board is upside down, with the 'dots' physically
    // reversed so the two boards are dot-aligned. Thus the data bit
    // is different for each, flip it here to be consistent (otherwise
    // it would be inversed on one board).
    *data_out = !data_in;
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

#if SPI_MODE == SPI_BITBANG
  if (board == FLIP_BOARD_TOP) {
    shift_byte(PIN_TOP_SERIAL_OUT, PIN_TOP_SERIAL_CLK,
        PIN_TOP_SR1_LATCH, col_sr_output);
    shift_byte(PIN_TOP_SERIAL_OUT, PIN_TOP_SERIAL_CLK,
        PIN_TOP_SR2_LATCH, row_sr_output);
  } else {
    shift_byte(PIN_BOT_SERIAL_OUT, PIN_BOT_SERIAL_CLK,
        PIN_BOT_SR1_LATCH, col_sr_output);
    shift_byte(PIN_BOT_SERIAL_OUT, PIN_BOT_SERIAL_CLK,
        PIN_BOT_SR2_LATCH, row_sr_output);
  }
#else
  if (board == FLIP_BOARD_TOP) {
    shift_byte(col_sr_output, &spi_top_cols);
    shift_byte(row_sr_output, &spi_top_rows);
  } else {
    shift_byte(col_sr_output, &spi_bot_cols);
    shift_byte(row_sr_output, &spi_bot_rows);
  }
#endif

  uint8_t pin_enable;
  if (board == FLIP_BOARD_TOP) {
    pin_enable = PIN_TOP_ENABLE;
  } else {
    pin_enable = PIN_BOT_ENABLE;
  }

  // Enable, writing the data. This incurs a per-pixel delay, so
  // a value lower than 1 ms is required -- use ets_delay_us() to
  // burn CPU for the required number of microseconds.
  gpio_set_level(pin_enable, true);
  ets_delay_us(FR2800_ENABLE_USEC);
  gpio_set_level(pin_enable, false);

  return true;
}
