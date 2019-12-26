#include <assert.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mgos_bmp_loader.h"

#include "flipdots-base.h"
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

  if (!mode_notification_params.bmp) {
    ESP_LOGW(LOG_TAG, "Couldn't draw bitmap as no icon set");
    return 0;
  }

  buffer_draw_bitmap_rgb(
      0, 0, mgos_bmp_loader_get_data(mode_notification_params.bmp),
      DISPLAY_WIDTH, DISPLAY_HEIGHT,
      PIXEL_YELLOW, &buffer_draw);

  if (mode_notification_params.invert) {
    buffer_inverse(&buffer_draw);
  }

  buffer_commit_drawing();

  mode_notification_params.invert = !mode_notification_params.invert;
  mode_notification_params.draws++;

  return NOTIFICATION_TIME_DELAY_BETWEEN_DRAWS_MS;
}

void mode_notification_setup() {
  mode_notification_params.bmp = NULL;
  mode_notification_params.invert = false;
  mode_notification_params.draws = 0;
}

bool mode_notification_set_icon(const char* icon) {
  if (icon == NULL) {
    ESP_LOGE(LOG_TAG, "Notification icon must not be NULL!");
    return false;
  }

  char filename[NOTIFICATION_FILENAME_LEN_MAX];
  if (snprintf(filename, NOTIFICATION_FILENAME_LEN_MAX, "%s.bmp", icon) >=
      NOTIFICATION_FILENAME_LEN_MAX) {
    ESP_LOGW(LOG_TAG, "Had to truncate icon filename for: %s", icon);
    return false;
  }

  bmpread_t* bmp = mgos_bmp_loader_create();
  if (!mgos_bmp_loader_load(
          bmp, filename,
          BMPREAD_TOP_DOWN | BMPREAD_BYTE_ALIGN | BMPREAD_ANY_SIZE)) {
    ESP_LOGW(LOG_TAG, "Couldn't open bitmap file: %s", filename);
    return false;
  }

  if (mgos_bmp_loader_get_width(bmp) > DISPLAY_WIDTH ||
      mgos_bmp_loader_get_height(bmp) > DISPLAY_HEIGHT) {
    ESP_LOGW(LOG_TAG,
        "BMP dimensions too large (%ix%i needs to be less than %ix%i)",
        mgos_bmp_loader_get_width(bmp), mgos_bmp_loader_get_height(bmp),
        DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return false;
  }

  ESP_LOGI(LOG_TAG, "Setting notification icon: %s (=%s)", icon, filename);

  // Free an old notification icon if one was previously set.
  if (mode_notification_params.bmp != NULL) {
    mgos_bmp_loader_free(mode_notification_params.bmp);
  }
  mode_notification_params.bmp = bmp;
  mode_notification_params.invert = false;
  mode_notification_params.draws = 0;

  return true;
}
