#include <assert.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "displaydriver.h"
#include "mode-notification.h"
#include "mutex-util.h"

#include "graphics_bell.xbm"

SemaphoreHandle_t mode_notification_mutex = NULL;
ModeNotificationParameters mode_notification_params;

const static char *LOG_TAG = "mode-notification";

static xTaskHandle task_mode_notification_handle;

static void draw_notification(displaybuffer_t* buffer) {
  const uint8_t* bitmap = NULL;
  const uint8_t width = DISPLAY_WIDTH;
  const uint8_t height = DISPLAY_HEIGHT;

  switch (mode_notification_params.notification_icon) {
    case NOTIFICATION_ICON_DOORBELL:
      bitmap = graphics_bell_bits;
  }
  
  if (bitmap != NULL) {
    buffer_draw_bitmap(0, 0, bitmap, width, height, PIXEL_YELLOW, buffer);
  }
}

static void task_mode_notification(void* pvParameters) {
  bool inverse_the_icon = false;

  while (true) {
    mutex_lock(mode_notification_mutex);

    buffer_wipe(&buffer_draw);
    draw_notification(&buffer_draw);

    if (inverse_the_icon) {
      buffer_inverse(&buffer_draw);
    }

    inverse_the_icon = !inverse_the_icon;
    buffer_commit_drawing();

    // Release the lock, and wait until the next run is due.
    mutex_unlock(mode_notification_mutex);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

void mode_notification_setup() {
  mode_notification_mutex = xSemaphoreCreateMutex();

  mutex_lock(mode_notification_mutex);
  mode_notification_params.notification_icon = NOTIFICATION_ICON_DEFAULT;
  mutex_unlock(mode_notification_mutex);
}

void mode_notification_start() {
  // mode_notification_mutex will already be held by orchestrator.
  configASSERT(xTaskCreatePinnedToCore(
      task_mode_notification,
      TASK_MODE_NOTIFICATION_NAME,
      TASK_MODE_NOTIFICATION_STACK_WORDS,
      NULL,
      TASK_MODE_NOTIFICATION_PRIORITY,
      &task_mode_notification_handle,
      tskNO_AFFINITY) == pdTRUE);
}

void mode_notification_network_input(const uint8_t* data, int bytes) {
  // mode_notification_mutex will already be held by orchestrator.
  assert(data != NULL);

  if (bytes == 1 && *data <= NOTIFICATION_ICON_MAX) {
    ESP_LOGW(LOG_TAG, "Setting notification icon: %i", *data);
    mode_notification_params.notification_icon = *data;
  }
}
