#include <assert.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mgos_bmp_loader.h"

#include "displaydriver.h"
#include "mode-notification.h"
#include "mutex-util.h"

ModeNotificationParameters mode_notification_params;

const static char *LOG_TAG = "mode-notification";

int mode_notification_draw() {
  if (mode_notification_params.draws >= NOTIFICATION_DRAWS) {
    return 0;   // Notification finished.
  }

  buffer_wipe(&buffer_draw);

  char* filename = NULL;
  const uint8_t* bitmap = NULL;

  switch (mode_notification_params.notification_icon) {
    case NOTIFICATION_ICON_DOORBELL:
      filename = "doorbell.bmp";
      break;
    case NOTIFICATION_ICON_DISHWASHER:
      filename = "dishwasher.bmp";
      break;
    case NOTIFICATION_ICON_TUMBLE_DRYER:
      filename = "tumble-dryer.bmp";
      break;
    case NOTIFICATION_ICON_WASHING_MACHINE:
      filename = "washing-machine.bmp";
      break;
    default:
      ESP_LOGE(LOG_TAG, "Unknown notification icon: %i", mode_notification_params.notification_icon);
      return 0;
  }

  bmpread_t* bmp = mgos_bmp_loader_create();
  if (!mgos_bmp_loader_load(
          bmp, filename,
          BMPREAD_TOP_DOWN | BMPREAD_BYTE_ALIGN | BMPREAD_ANY_SIZE)) {
    ESP_LOGE(LOG_TAG, "Couldn't open bitmap file: %s", filename);
    return 0;
  }

  bitmap = mgos_bmp_loader_get_data(bmp);

  if (bitmap != NULL) {
    buffer_draw_bitmap_rgb(0, 0, bitmap, DISPLAY_WIDTH, DISPLAY_HEIGHT, PIXEL_YELLOW, &buffer_draw);
  }

  mgos_bmp_loader_free(bmp);
  bmp = NULL;

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

bool mode_notification_get_icon_by_str(char* str, enum NotificationIcon* icon) {
  if (strcmp(str, NOTIFICATION_TAG_DOORBELL) == 0) {
    *icon = NOTIFICATION_ICON_DOORBELL;
    return true;
  } else if (strcmp(str, NOTIFICATION_TAG_DISHWASHER) == 0) {
    *icon = NOTIFICATION_ICON_DISHWASHER;
    return true;
  } else if (strcmp(str, NOTIFICATION_TAG_TUMBLE_DRYER) == 0) {
    *icon = NOTIFICATION_ICON_TUMBLE_DRYER;
    return true;
  } else if (strcmp(str, NOTIFICATION_TAG_WASHING_MACHINE) == 0) {
    *icon = NOTIFICATION_ICON_WASHING_MACHINE;
    return true;
  }
  return false;
}
