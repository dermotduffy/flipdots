#include <assert.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "displaydriver.h"
#include "mode-notification.h"
#include "mutex-util.h"

#include "graphics_bell.xbm"

ModeNotificationParameters mode_notification_params;

const static char *LOG_TAG = "mode-notification";

int mode_notification_draw() {
  if (mode_notification_params.draws >= NOTIFICATION_DRAWS) {
    return 0;   // Notification finished.
  }

  buffer_wipe(&buffer_draw);

  const uint8_t* bitmap = NULL;
  switch (mode_notification_params.notification_icon) {
    case NOTIFICATION_ICON_DOORBELL:
      bitmap = graphics_bell_bits;
  }

  if (bitmap != NULL) {
    buffer_draw_bitmap(0, 0, bitmap, DISPLAY_WIDTH, DISPLAY_HEIGHT, PIXEL_YELLOW, &buffer_draw);
  }

  if (mode_notification_params.invert) {
    buffer_inverse(&buffer_draw);
  }
  buffer_commit_drawing();

  mode_notification_params.invert = !mode_notification_params.invert;
  mode_notification_params.draws++;

  return NOTIFICATION_TIME_DELAY_BETWEEN_DRAWS_MS;
}

void mode_notification_setup() {
  mode_notification_params.notification_icon = NOTIFICATION_ICON_DEFAULT;
  mode_notification_params.invert = false;
  mode_notification_params.draws = 0;
}

bool mode_notification_set_icon(int icon) {
  if (icon < NOTIFICATION_ICON_MIN || icon > NOTIFICATION_ICON_MAX) {
    ESP_LOGW(LOG_TAG, "Notification icon must be >= %d and <= %d",
        NOTIFICATION_ICON_MIN, NOTIFICATION_ICON_MAX);
    return false;
  }
  ESP_LOGI(LOG_TAG, "Setting notification icon: %i", icon);
  mode_notification_params.notification_icon = icon;
  mode_notification_params.invert = false;
  mode_notification_params.draws = 0;
  return true;
}
