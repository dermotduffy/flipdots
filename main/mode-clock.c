#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mode-clock.h"
#include "displaybuffer.h"
#include "liberation_sans_20.h"

#define HOUR_BUFFER_SIZE   3 // 2 digits + null byte.
#define GAP_BETWEEN_DIGITS 1

#define MINS_PER_HOUR      60

// Use 59 steps rather than 60, in order to ensure the last minute of the hour
// shows the board full rather than almost full. Error with these calculations
// is spread over the whole hour.
#define PIXELS_PER_MIN     (DISPLAY_PIXELS / (MINS_PER_HOUR-1))
#define PIXELS_PER_MIN_ERR (DISPLAY_PIXELS % (MINS_PER_HOUR-1))

SemaphoreHandle_t mode_clock_semaphore = NULL;

static xTaskHandle task_mode_clock_handle;
static char hour_buffer[HOUR_BUFFER_SIZE];  // 2 digits + null byte.
static const font_info_t* hour_font = &liberationSans_20pt_font_info;

static void draw_hours(int hours, displaybuffer_t* displaybuffer) {
  configASSERT(snprintf(hour_buffer, HOUR_BUFFER_SIZE, "%02i", 0) ==
      HOUR_BUFFER_SIZE-1);

  int text_width_in_pixels = font_get_string_pixel_length(
    hour_buffer, GAP_BETWEEN_DIGITS, hour_font);

  // This should never happen or the font size has been misjudged.
  assert(text_width_in_pixels < DISPLAY_WIDTH);

  // Center the text.
  int start_x = (DISPLAY_WIDTH - text_width_in_pixels) / 2;
  int start_y = (DISPLAY_HEIGHT - hour_font->height) / 2;

  buffer_tdf_draw_string(
      start_x, start_y, hour_buffer, GAP_BETWEEN_DIGITS,
      hour_font, displaybuffer);
}

static void draw_minutes(int mins, displaybuffer_t* displaybuffer) {
  int dots_to_fill = (mins * PIXELS_PER_MIN) + ceil(
      (mins / ((double) (MINS_PER_HOUR-1))) * PIXELS_PER_MIN_ERR);

  for (int y = 0; y < DISPLAY_HEIGHT && dots_to_fill > 0; ++y) {
    for (int x = 0;
         x < DISPLAY_WIDTH && dots_to_fill > 0;
         ++x, --dots_to_fill) {
      bool current_value = displaybuffer->data[x][y];
      displaybuffer->data[x][y] = !current_value;
    }
  }
}

static void task_mode_clock(void* pvParameters) {
  assert(pvParameters != NULL);
  displaybuffer_t* displaybuffer = ((displaybuffer_t*)pvParameters);

  while (true) {
    if (xSemaphoreTake(mode_clock_semaphore, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    configASSERT(snprintf(hour_buffer, HOUR_BUFFER_SIZE, "%02i", 0) ==
        HOUR_BUFFER_SIZE-1);

    buffer_wipe(displaybuffer);

    // TODO: Get actual hour/minutes.
    draw_hours(0, displaybuffer);
    draw_minutes(0, displaybuffer);

    while (xSemaphoreGive(mode_clock_semaphore) != pdTRUE);
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

void mode_clock_init(displaybuffer_t* displaybuffer) {
  mode_clock_semaphore = xSemaphoreCreateBinary();

  configASSERT(xTaskCreatePinnedToCore(
      task_mode_clock,
      TASK_MODE_CLOCK_NAME,
      TASK_MODE_CLOCK_STACK_WORDS,
      displaybuffer,
      TASK_MODE_CLOCK_PRIORITY,
      &task_mode_clock_handle,
      tskNO_AFFINITY) == pdTRUE);
}
