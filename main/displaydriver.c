#include <assert.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "displaybuffer.h"
#include "displaydriver.h"
#include "flipdots-base.h"
#include "hardware.h"
#include "mutex-util.h"

#define TASK_DISPLAYDRIVER_UPDATE_NAME               "displaydriver"
#define TASK_DISPLAYDRIVER_UPDATE_STACK_WORDS        2<<11
#define TASK_DISPLAYDRIVER_UPDATE_PRIORITY           4

#define TASK_DISPLAYDRIVER_UPDATE_WORKER_TOP_NAME    "displaydriver-worker-top"
#define TASK_DISPLAYDRIVER_UPDATE_WORKER_BOT_NAME    "displaydriver-worker-bot"
#define TASK_DISPLAYDRIVER_UPDATE_WORKER_STACK_WORDS 2<<11
#define TASK_DISPLAYDRIVER_UPDATE_WORKER_PRIORITY    5

#define TIME_DELAY_DISPLAYDRIVER_TEST_MS             1*1000

const static char *LOG_TAG = "displaydriver";

extern EventGroupHandle_t display_event_group;

static xTaskHandle task_displaydriver_update_handle;
static xTaskHandle task_displaydriver_update_worker_top_handle;
static xTaskHandle task_displaydriver_update_worker_bot_handle;

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
} displaydriver_worker_config_t;

// Workers need to be aligned with the physical hardware, so rotation needs to
// be taken into account.
displaydriver_worker_config_t displaydriver_worker_top_config = {
  .start_x = 0,
  .start_y = 0,
#if DISPLAY_ROTATION == DISPLAY_ROTATION_90 || DISPLAY_ROTATION == DISPLAY_ROTATION_270
  .end_x = (DISPLAY_WIDTH / 2) - 1,
  .end_y = (DISPLAY_HEIGHT - 1),
#else
  .end_x = (DISPLAY_WIDTH - 1),
  .end_y = (DISPLAY_HEIGHT / 2) - 1,
#endif
  .event_group = &display_event_group,
  .activate_bit = DISPLAY_EVENT_WORKER_TOP_ACTIVATE_BIT,
  .activate_full_bit = DISPLAY_EVENT_WORKER_TOP_ACTIVATE_FULL_BIT,
  .complete_bit = DISPLAY_EVENT_WORKER_TOP_COMPLETE_BIT,
  .src = &buffer_prelive,
  .dst = &buffer_live,
};
displaydriver_worker_config_t displaydriver_worker_bot_config = {
#if DISPLAY_ROTATION == DISPLAY_ROTATION_90 || DISPLAY_ROTATION == DISPLAY_ROTATION_270
  .start_x = (DISPLAY_WIDTH / 2),
  .start_y = 0,
#else
  .start_x = 0,
  .start_y = (DISPLAY_HEIGHT / 2),
#endif
  .end_x = DISPLAY_WIDTH-1,
  .end_y = DISPLAY_HEIGHT - 1,
  .event_group = &display_event_group,
  .activate_bit = DISPLAY_EVENT_WORKER_BOT_ACTIVATE_BIT,
  .activate_full_bit = DISPLAY_EVENT_WORKER_BOT_ACTIVATE_FULL_BIT,
  .complete_bit = DISPLAY_EVENT_WORKER_BOT_COMPLETE_BIT,
  .src = &buffer_prelive,
  .dst = &buffer_live,
};

SemaphoreHandle_t buffer_staging_mutex; 

displaybuffer_t buffer_live;
displaybuffer_t buffer_prelive;
displaybuffer_t buffer_staging;
displaybuffer_t buffer_draw;

bool buffer_commit_drawing() {
  mutex_lock(buffer_staging_mutex);
  bool needed = buffer_save_if_needed(&buffer_draw, &buffer_staging);
  mutex_unlock(buffer_staging_mutex);

  if (needed) {
    xEventGroupSetBits(display_event_group, DISPLAY_EVENT_COMMIT_BIT);
  }
  return needed;
}

bool buffer_save_staging_to_prelive() {
  mutex_lock(buffer_staging_mutex);
  bool needed = buffer_save_if_needed(&buffer_staging, &buffer_prelive);
  mutex_unlock(buffer_staging_mutex);
  return needed;
}

// Master displaybuffer update task: locks staging buffer, copies
// to pre-live then activates workers to update the live display and
// awaits completion.
void task_displaydriver_update() {
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
    }

    first_run = false;
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

// Actually updates a portion of the live display and flips physical dots.
void task_displaydriver_update_worker(void* pvParameters) {
  assert(pvParameters != NULL);

  displaydriver_worker_config_t* config =
      ((displaydriver_worker_config_t *) pvParameters);

  assert(config->start_x < DISPLAY_WIDTH);
  assert(config->end_x < DISPLAY_WIDTH);
  assert(config->start_y < DISPLAY_HEIGHT);
  assert(config->end_y < DISPLAY_HEIGHT);
  assert(config->event_group != NULL);

  while (true) {
    EventBits_t bits = xEventGroupWaitBits(
        *(config->event_group),
        config->activate_bit | config->activate_full_bit,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    bool full_write = false;

    if ((bits & config->activate_full_bit)) {
      full_write = true;
    } else if ((bits & config->activate_bit) == 0) {
      continue;
    }

// Small visual tweak: Keep the 'sweep' of the display refresh coming in from
// the 'sides' of the individual flipboards, regardless of the rotation.
#if DISPLAY_ROTATION == DISPLAY_ROTATION_90 || DISPLAY_ROTATION == DISPLAY_ROTATION_270
    for (uint8_t y = config->start_y; y <= config->end_y; ++y) {
      for (uint8_t x = config->start_x; x <= config->end_x; ++x) {
#else
    for (uint8_t x = config->start_x; x <= config->end_x; ++x) {
      for (uint8_t y = config->start_y; y <= config->end_y; ++y) {
#endif
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
  
    xEventGroupSetBits(*(config->event_group), config->complete_bit);
  }
}

void displaydriver_setup() {
  hardware_setup();

  display_event_group = xEventGroupCreate();
  configASSERT(display_event_group != NULL);

  buffer_staging_mutex = xSemaphoreCreateMutex();
  configASSERT(buffer_staging_mutex != NULL);

  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_live);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_prelive);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_staging);
  buffer_init(DISPLAY_WIDTH, DISPLAY_HEIGHT, &buffer_draw);
}

void displaydriver_start() {
  // Create two update worker tasks for each half of the flipdot display (top &
  // bottom).
  configASSERT(xTaskCreatePinnedToCore(
      task_displaydriver_update_worker,
      TASK_DISPLAYDRIVER_UPDATE_WORKER_TOP_NAME,
      TASK_DISPLAYDRIVER_UPDATE_WORKER_STACK_WORDS,
      &displaydriver_worker_top_config,
      TASK_DISPLAYDRIVER_UPDATE_WORKER_PRIORITY,
      &task_displaydriver_update_worker_top_handle,
      tskNO_AFFINITY) == pdTRUE);

  configASSERT(xTaskCreatePinnedToCore(
      task_displaydriver_update_worker,
      TASK_DISPLAYDRIVER_UPDATE_WORKER_BOT_NAME,
      TASK_DISPLAYDRIVER_UPDATE_WORKER_STACK_WORDS,
      &displaydriver_worker_bot_config,
      TASK_DISPLAYDRIVER_UPDATE_WORKER_PRIORITY,
      &task_displaydriver_update_worker_bot_handle,
      tskNO_AFFINITY) == pdTRUE);

  // Start the master update task (which activates the workers).
  configASSERT(xTaskCreatePinnedToCore(
      task_displaydriver_update,
      TASK_DISPLAYDRIVER_UPDATE_NAME,
      TASK_DISPLAYDRIVER_UPDATE_STACK_WORDS,
      NULL,
      TASK_DISPLAYDRIVER_UPDATE_PRIORITY,
      &task_displaydriver_update_handle,
      tskNO_AFFINITY) == pdTRUE);
}

void displaydriver_testpattern() {
  buffer_fill(false, &buffer_draw);
  buffer_commit_drawing();
  vTaskDelay(TIME_DELAY_DISPLAYDRIVER_TEST_MS / portTICK_PERIOD_MS);

  buffer_fill(true, &buffer_draw);
  buffer_commit_drawing();
  vTaskDelay(TIME_DELAY_DISPLAYDRIVER_TEST_MS / portTICK_PERIOD_MS);

  buffer_fill(false, &buffer_draw);
  buffer_commit_drawing();
}
