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

#define TASK_DISPLAYBUFFER_UPDATE_NAME        "display-update"
#define TASK_DISPLAYBUFFER_UPDATE_STACK_WORDS 2<<11
#define TASK_DISPLAYBUFFER_UPDATE_PRORITY     4

#define TASK_DISPLAYBUFFER_UPDATE_WORKER_TOP_NAME    "display-update-worker-top"
#define TASK_DISPLAYBUFFER_UPDATE_WORKER_BOT_NAME    "display-update-worker-bot"
#define TASK_DISPLAYBUFFER_UPDATE_WORKER_STACK_WORDS 2<<11
#define TASK_DISPLAYBUFFER_UPDATE_WORKER_PRORITY     5

displaybuffer_t buffer_live;    // On the display currently.
displaybuffer_t buffer_prelive; // Being written to the display.
displaybuffer_t buffer_staging; // Complete frames held before writing.
displaybuffer_t buffer_draw;    // Drawing happens here.

static EventGroupHandle_t display_event_group;
#define DISPLAY_EVENT_COMMIT_BIT                     BIT0
#define DISPLAY_EVENT_WORKER_TOP_ACTIVATE_BIT        BIT1
#define DISPLAY_EVENT_WORKER_BOT_ACTIVATE_BIT        BIT2
#define DISPLAY_EVENT_WORKER_TOP_ACTIVATE_FULL_BIT   BIT3
#define DISPLAY_EVENT_WORKER_BOT_ACTIVATE_FULL_BIT   BIT4
#define DISPLAY_EVENT_WORKER_TOP_COMPLETE_BIT        BIT5
#define DISPLAY_EVENT_WORKER_BOT_COMPLETE_BIT        BIT6

// Configuration parameters for a display update worker.
typedef struct {
  // Coordinates defining the extent of the displaybuffer to operate on.
  uint8_t start_x;
  uint8_t start_y;
  uint8_t end_x;
  uint8_t end_y;

  // Event group parameters for synchronisation.
  EventGroupHandle_t* event_group;
  const int activate_bit;
  const int activate_full_bit;
  const int complete_bit;

  // Data from source buffer is copied to destination buffer.
  displaybuffer_t* src;
  displaybuffer_t* dst;
} displaybuffer_worker_config_t;

displaybuffer_worker_config_t displaybuffer_worker_top_config = {
  .start_x = 0,
  .end_x = DISPLAY_WIDTH-1,
  .start_y = 0,
  .end_y = (DISPLAY_HEIGHT / 2) - 1,
  .event_group = &display_event_group,
  .activate_bit = DISPLAY_EVENT_WORKER_TOP_ACTIVATE_BIT,
  .activate_full_bit = DISPLAY_EVENT_WORKER_TOP_ACTIVATE_FULL_BIT,
  .complete_bit = DISPLAY_EVENT_WORKER_TOP_COMPLETE_BIT,
  .src = &buffer_prelive,
  .dst = &buffer_live,
};
displaybuffer_worker_config_t displaybuffer_worker_bot_config = {
  .start_x = 0,
  .end_x = DISPLAY_WIDTH-1,
  .start_y = (DISPLAY_HEIGHT / 2),
  .end_y = DISPLAY_HEIGHT - 1,
  .event_group = &display_event_group,
  .activate_bit = DISPLAY_EVENT_WORKER_BOT_ACTIVATE_BIT,
  .activate_full_bit = DISPLAY_EVENT_WORKER_BOT_ACTIVATE_FULL_BIT,
  .complete_bit = DISPLAY_EVENT_WORKER_BOT_COMPLETE_BIT,
  .src = &buffer_prelive,
  .dst = &buffer_live,
};

static SemaphoreHandle_t buffer_staging_mutex; 

static xTaskHandle task_displaybuffer_update_handle;
static xTaskHandle task_displaybuffer_update_worker_top_handle;
static xTaskHandle task_displaybuffer_update_worker_bot_handle;

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

// Master displaybuffer update task: locks staging buffer, copies
// to pre-live then activates workers to update the live display and
// awaits completion.
void task_displaybuffer_update() {
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
      // Acivate display update workers.
      // An available optimisation would be to only activate the worker for the
      // portion of the display that has been updated.

      // On the first run force write all bits to ensure physical display
      // resembles the internal live displaybuffer.
      EventBits_t bits;      
      if (first_run) {
        bits = DISPLAY_EVENT_WORKER_TOP_ACTIVATE_FULL_BIT |
               DISPLAY_EVENT_WORKER_BOT_ACTIVATE_FULL_BIT;
      } else {
        bits = DISPLAY_EVENT_WORKER_TOP_ACTIVATE_BIT |
               DISPLAY_EVENT_WORKER_BOT_ACTIVATE_BIT;
      }
      xEventGroupSetBits(display_event_group, bits);

      // Wait for workers to complete.
      xEventGroupWaitBits(
          display_event_group,
          DISPLAY_EVENT_WORKER_TOP_COMPLETE_BIT |
              DISPLAY_EVENT_WORKER_BOT_COMPLETE_BIT,
          pdTRUE,
          pdTRUE,
          portMAX_DELAY);

      // All changes written out.
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

// Actually updates a portion of the live display and flips physical dots.
void task_displaybuffer_update_worker(void* pvParameters) {
  assert(pvParameters != NULL);

  displaybuffer_worker_config_t* config =
      ((displaybuffer_worker_config_t *) pvParameters);

  assert(config->start_x < DISPLAY_WIDTH);
  assert(config->end_x < DISPLAY_WIDTH);
  assert(config->start_y < DISPLAY_HEIGHT);
  assert(config->end_y < DISPLAY_HEIGHT);
  assert(config->event_group != NULL);

  while (true) {
    EventBits_t bits = xEventGroupWaitBits(
        config->event_group,
        config->activate_bit | config->activate_full_bit,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY);

    bool full_write = false;

    if ((bits & config->activate_full_bit)) {
      full_write = true;
    } else if ((bits & config->activate_bit) == 0) {
      continue;
    }

    for (uint8_t x = config->start_x; x <= config->end_x; ++x) {
      for (uint8_t y = config->start_y; y <= config->end_y; ++y) {
        // Only write out the bits that are different:
        // Write to a flip-dot display is extremely time expensive per pixel,
        // e.g. >1ms per pixel.
        if (full_write || config->src->data[x][y] != config->dst->data[x][y]) {
          bool data = config->src->data[x][y];
          raw_write_bit(x, y, data);
          config->dst->data[x][y] = data;
        }
      }
    }
  
    xEventGroupSetBits(config->event_group, config->complete_bit);
  }
}

// ****
// Main
// ****
void app_main(void)
{
  nvs_flash_init();
  init_spi();
  init_hardware();

  buffer_staging_mutex = xSemaphoreCreateMutex();
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_live);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_prelive);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_staging);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_draw);

  display_event_group = xEventGroupCreate();
  configASSERT(display_event_group != NULL);

  // Create two update worker tasks for each half of the flipdot display (top &
  // bottom).
  configASSERT(xTaskCreatePinnedToCore(
      task_displaybuffer_update_worker,
      TASK_DISPLAYBUFFER_UPDATE_WORKER_TOP_NAME,
      TASK_DISPLAYBUFFER_UPDATE_WORKER_STACK_WORDS,
      &displaybuffer_worker_top_config,
      TASK_DISPLAYBUFFER_UPDATE_WORKER_PRORITY,
      &task_displaybuffer_update_worker_top_handle,
      tskNO_AFFINITY) == pdTRUE);
  configASSERT(xTaskCreatePinnedToCore(
      task_displaybuffer_update_worker,
      TASK_DISPLAYBUFFER_UPDATE_WORKER_BOT_NAME,
      TASK_DISPLAYBUFFER_UPDATE_WORKER_STACK_WORDS,
      &displaybuffer_worker_bot_config,
      TASK_DISPLAYBUFFER_UPDATE_WORKER_PRORITY,
      &task_displaybuffer_update_worker_bot_handle,
      tskNO_AFFINITY) == pdTRUE);

  // Start the master update task (which activates the workers).
  configASSERT(xTaskCreatePinnedToCore(
      task_displaybuffer_update,
      TASK_DISPLAYBUFFER_UPDATE_NAME,
      TASK_DISPLAYBUFFER_UPDATE_STACK_WORDS,
      NULL,
      TASK_DISPLAYBUFFER_UPDATE_PRORITY,
      &task_displaybuffer_update_handle,
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
