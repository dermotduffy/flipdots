#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "displaybuffer.h"
#include "displaydriver.h"
#include "mode-clock.h"
#include "mutex-util.h"
#include "time-util.h"

#include "liberation_sans_20pt.h"
#include "gohufont_11.h"

#define TIME_BUFFER_SIZE               6 // Largest: 'XX:XX\0' => 6
#define GAP_BETWEEN_DIGITS             1

#define MINS_PER_HOUR                  60

#define TIME_DELAY_BETWEEN_RUNS_MS     1*1000
#define TIME_DELAY_IF_ERROR_MS         10*1000

// Use 59 steps rather than 60, in order to ensure the last minute of the hour
// shows the board full rather than almost full. Error with these calculations
// is spread over the whole hour.
#define HOUR_ONLY_PIXELS_PER_MIN     (DISPLAY_PIXELS / (MINS_PER_HOUR-1))
#define HOUR_ONLY_PIXELS_PER_MIN_ERR (DISPLAY_PIXELS % (MINS_PER_HOUR-1))

SemaphoreHandle_t mode_clock_mutex = NULL;
ModeClockParameters mode_clock_params;

const static char *LOG_TAG = "mode-clock";

static xTaskHandle task_mode_clock_handle;
static char time_buffer[TIME_BUFFER_SIZE];
static const font_info_t* font_large = &liberation_sans_20pt_font_info;
static const font_info_t* font_medium = &gohufont_11_font_info;

static void draw_large_hours(int hours, displaybuffer_t* displaybuffer) {
  configASSERT(snprintf(time_buffer, TIME_BUFFER_SIZE, "%02i", hours) > 0);

  buffer_tdf_draw_string_centre(
      time_buffer, GAP_BETWEEN_DIGITS, font_large, displaybuffer);
}

static void draw_dot_minutes(int mins, displaybuffer_t* displaybuffer) {
  int dots_to_fill = (mins * HOUR_ONLY_PIXELS_PER_MIN) + ceil(
      (mins / ((double) (MINS_PER_HOUR-1))) * HOUR_ONLY_PIXELS_PER_MIN_ERR);

  for (int y = 0; y < DISPLAY_HEIGHT && dots_to_fill > 0; ++y) {
    for (int x = 0;
         x < DISPLAY_WIDTH && dots_to_fill > 0;
         ++x, --dots_to_fill) {
      bool current_value = displaybuffer->data[x][y];
      displaybuffer->data[x][y] = !current_value;
    }
  }
}

static void draw_time(
    const struct tm* time_info, displaybuffer_t* displaybuffer) {
  configASSERT(strftime(
      time_buffer, TIME_BUFFER_SIZE, "%H:%M", time_info) > 0);

  buffer_tdf_draw_string_centre(
    time_buffer, GAP_BETWEEN_DIGITS, font_medium, displaybuffer);    
}

static void task_mode_clock(void* pvParameters) {
  struct tm time_info;
  bool ever_had_valid_time = false;

  while (true) {
    mutex_lock(mode_clock_mutex);

    // Display an error message if there is no valid time available.
    if (!ever_had_valid_time) {
      if (have_valid_time()) {
        ever_had_valid_time = true;
      } else {
        buffer_wipe(&buffer_draw);
        buffer_tdf_draw_string_centre(
            "?", GAP_BETWEEN_DIGITS, font_large, &buffer_draw);
        buffer_commit_drawing();

        mutex_unlock(mode_clock_mutex);
        vTaskDelay(TIME_DELAY_IF_ERROR_MS / portTICK_PERIOD_MS);
        continue;
      }
    }

    // Get the current time.
    get_time_info(&time_info);

    // Update display buffer.
    buffer_wipe(&buffer_draw);

    if (mode_clock_params.clock_style == CLOCK_STYLE_HOUR_ONLY) {
      draw_large_hours(time_info.tm_hour, &buffer_draw);
      draw_dot_minutes(time_info.tm_min, &buffer_draw);
    } else { // Default: CLOCK_STYLE_DIGITAL
      draw_time(&time_info, &buffer_draw);
    }
    
    buffer_commit_drawing();
    // Release the lock, and wait until the next run is due.
    mutex_unlock(mode_clock_mutex);
    vTaskDelay(TIME_DELAY_BETWEEN_RUNS_MS / portTICK_PERIOD_MS);
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

void mode_clock_setup() {
  mode_clock_mutex = xSemaphoreCreateMutex();

  mutex_lock(mode_clock_mutex);
  mode_clock_params.clock_style = CLOCK_STYLE_DEFAULT;
  mutex_unlock(mode_clock_mutex);
}

void mode_clock_start() {
  // mode_clock_mutex will already be held by orchestrator.
  configASSERT(xTaskCreatePinnedToCore(
      task_mode_clock,
      TASK_MODE_CLOCK_NAME,
      TASK_MODE_CLOCK_STACK_WORDS,
      NULL,
      TASK_MODE_CLOCK_PRIORITY,
      &task_mode_clock_handle,
      tskNO_AFFINITY) == pdTRUE);
}

void mode_clock_network_input(const uint8_t* data, int bytes) {
  // mode_clock_mutex will already be held by orchestrator.
  assert(data != NULL);
  assert(bytes == 1);  

  if (*data <= CLOCK_STYLE_MAX) {
    mode_clock_params.clock_style = *data;
  }
}
