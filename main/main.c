#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "soc/soc.h"

#include "displaybuffer.h"
#include "flipdots-base.h"
#include "hardware.h"
#include "graphics_bell.xbm"

#define TASK_DISPLAY_UPDATE_NAME         "display-update"
#define TASK_DISPLAY_UPDATE_STACK_WORDS  2<<11
#define TASK_DISPLAY_UPDATE_PRORITY      4

displaybuffer_t buffer_live;    // On the display currently.
displaybuffer_t buffer_prelive; // Being written to the display.
displaybuffer_t buffer_staging; // Complete frames held before writing.
displaybuffer_t buffer_draw;    // Drawing happens here.

static SemaphoreHandle_t buffer_staging_mutex; 

static xTaskHandle task_display_update_handle;

static EventGroupHandle_t display_event_group;
const static int DISPLAY_EVENT_COMMIT_BIT = BIT0;

const static char *LOG_TAG = "Flipdots";

// ****************
// Helper functions
// ****************
static inline void mutex_lock(SemaphoreHandle_t* mutex) {
  configASSERT(xSemaphoreTake(*mutex, portMAX_DELAY) == pdTRUE);
}

static inline void mutex_unlock(SemaphoreHandle_t* mutex) {
  configASSERT(xSemaphoreGive(*mutex) == pdTRUE);
}

// **************
// Display buffer helper routines
// **************
static bool buffer_commit_drawing() {
  mutex_lock(&buffer_staging_mutex);
  bool needed = buffer_save_if_needed(&buffer_draw, &buffer_staging);
  mutex_unlock(&buffer_staging_mutex);

  if (needed) {
    xEventGroupSetBits(display_event_group, DISPLAY_EVENT_COMMIT_BIT);
  }
  return needed;
}

static bool buffer_save_staging_to_prelive() {
  mutex_lock(&buffer_staging_mutex);
  bool needed = buffer_save_if_needed(&buffer_staging, &buffer_prelive);
  mutex_unlock(&buffer_staging_mutex);
  return needed;
}

// **************
// FreeRTOS Tasks
// **************
void task_display_update() {
  assert(buffer_prelive.height == buffer_live.height);
  assert(buffer_prelive.width == buffer_live.width);

  // Force a total display re-write on first-run, as the state it is
  // currently in is unknown.
  bool first_run = true;

  while (true) {
    if (!first_run) {
      if ((xEventGroupWaitBits(
           display_event_group,
           DISPLAY_EVENT_COMMIT_BIT,
           pdTRUE,   // Clear on exit.
           pdTRUE,   // Wait for all bits.
           portMAX_DELAY) & DISPLAY_EVENT_COMMIT_BIT) == 0) {
        continue;
      }
    }

    ESP_LOGI(LOG_TAG, "[Display update: found work]"); 

    // Copy the staging buffer into the pre-live buffer.
    if (first_run || buffer_save_staging_to_prelive()) {
      // Only write out the bits that are different
      // (Write to a flip-dot display is extremely time expensive
      //  per pixel, e.g. >1ms per pixel).
      for (uint8_t x = 0; x < buffer_prelive.width; ++x) {
        for (uint8_t y = 0; y < buffer_prelive.height; ++y) {
          if (first_run || buffer_prelive.data[x][y] != buffer_live.data[x][y]) {
            bool data = buffer_prelive.data[x][y];
            raw_write_bit(x, y, data);
            buffer_live.data[x][y] = data;
          }
        }
      }

      // Just to be tidy.
      buffer_live.modified = false;
    } else {
      ESP_LOGI(LOG_TAG, "[Display update task: found no work]"); 
    }

    first_run = false;
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

// ****
// Main
// ****
void app_main(void)
{
  nvs_flash_init();
  init_hardware();
  init_spi();

  buffer_staging_mutex = xSemaphoreCreateMutex();
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_live);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_prelive);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_staging);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_draw);

  display_event_group = xEventGroupCreate();
  configASSERT(display_event_group != NULL);

  configASSERT(xTaskCreatePinnedToCore(
      task_display_update,
      TASK_DISPLAY_UPDATE_NAME,
      TASK_DISPLAY_UPDATE_STACK_WORDS,
      NULL,
      TASK_DISPLAY_UPDATE_PRORITY,
      &task_display_update_handle,
      tskNO_AFFINITY) == pdTRUE);

  buffer_draw_bitmap(
      0, 0, graphics_bell_bits, graphics_bell_width, graphics_bell_height,
      true, &buffer_draw);
  buffer_commit_drawing();

  while (true) {
    buffer_inverse(&buffer_draw);
    buffer_commit_drawing();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
