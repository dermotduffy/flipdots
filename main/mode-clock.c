#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mode-clock.h"
#include "mutex-util.h"
#include "displaybuffer.h"
#include "liberation_sans_20.h"
#include "time-util.h"

#define HOUR_BUFFER_SIZE               3 // 2 digits + null byte.
#define GAP_BETWEEN_DIGITS             1

#define MINS_PER_HOUR                  60

#define TIME_DELAY_BETWEEN_RUNS_MS     1*1000
#define TIME_DELAY_IF_ERROR_MS         10*1000

// Use 59 steps rather than 60, in order to ensure the last minute of the hour
// shows the board full rather than almost full. Error with these calculations
// is spread over the whole hour.
#define PIXELS_PER_MIN     (DISPLAY_PIXELS / (MINS_PER_HOUR-1))
#define PIXELS_PER_MIN_ERR (DISPLAY_PIXELS % (MINS_PER_HOUR-1))

static xTaskHandle task_mode_clock_handle;
static char hour_buffer[HOUR_BUFFER_SIZE];
static const font_info_t* hour_font = &liberationSans_20pt_font_info;

static void draw_hours(int hours, displaybuffer_t* displaybuffer) {
  configASSERT(snprintf(hour_buffer, HOUR_BUFFER_SIZE, "%02i", hours) > 0);

  buffer_tdf_draw_string_centre(
      hour_buffer, GAP_BETWEEN_DIGITS, hour_font, displaybuffer);
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

  struct tm time_info;
  bool ever_had_valid_time = false;

  while (true) {
    mutex_lock(mode_clock_mutex);

    // Display an error message if there is no valid time available.
    if (!ever_had_valid_time) {
      if (have_valid_time()) {
        ever_had_valid_time = true;
      } else {
        buffer_wipe(displaybuffer);
        buffer_tdf_draw_string_centre(
            "?", GAP_BETWEEN_DIGITS, hour_font, displaybuffer);

        mutex_unlock(mode_clock_mutex);
        vTaskDelay(TIME_DELAY_IF_ERROR_MS / portTICK_PERIOD_MS);
        continue;
      }
    }

    // Get the current time.
    get_time_info(&time_info);

    // Update display buffer.
    buffer_wipe(displaybuffer);
    draw_hours(time_info.tm_hour, displaybuffer);
    draw_minutes(time_info.tm_min, displaybuffer);

    // Release the lock, and wait until the next run is due.
    mutex_unlock(mode_clock_mutex);
    vTaskDelay(TIME_DELAY_BETWEEN_RUNS_MS / portTICK_PERIOD_MS);
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

void mode_clock_init() {
  configASSERT(xTaskCreatePinnedToCore(
      task_mode_clock,
      TASK_MODE_CLOCK_NAME,
      TASK_MODE_CLOCK_STACK_WORDS,
      NULL,
      TASK_MODE_CLOCK_PRIORITY,
      &task_mode_clock_handle,
      tskNO_AFFINITY) == pdTRUE);
}
