#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"

#include "mgos.h"

#include "displaydriver.h"
#include "orchestrator.h"

const static char *LOG_TAG = "flipdots";

// ****
// Main
// ****
enum mgos_app_init_result mgos_app_init(void) {
  ESP_LOGI(LOG_TAG, "Application start ...");

  displaydriver_setup();
  displaydriver_start();

  // === Display is functional ===
  // On start, do an all-pixel test. This also serves to wipe whatever was
  // previously on the display.
  // (Must be prior to orchestrator taking over the display).
  displaydriver_testpattern();

  // Setup & start the orchestrator.
  orchestrator_setup();
  orchestrator_start();

  return MGOS_APP_INIT_SUCCESS;
}
