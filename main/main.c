#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"

#include "mgos.h"

#include "blynk.h"
#include "displaydriver.h"
#include "touchpad.h"
#include "mqtt.h"
#include "orchestrator.h"
#include "rpc.h"
#include "status-led.h"

const static char *LOG_TAG = "flipdots";

// ****
// Main
// ****
enum mgos_app_init_result mgos_app_init(void) {
  ESP_LOGI(LOG_TAG, "Application start ...");

  status_led_setup();
  status_led_set(true);

  displaydriver_setup();
  displaydriver_start();

  // === Display is functional ===
  // On start, do an all-pixel test. This also serves to wipe whatever was
  // previously on the display.
  // (Must be prior to orchestrator taking over the display).
  displaydriver_testpattern();

  touchpad_setup();
  touchpad_start();

  // Setup & start the orchestrator.
  orchestrator_setup();
  orchestrator_start();

  mqtt_setup();
  rpc_setup();
  blynk_setup();

  return MGOS_APP_INIT_SUCCESS;
}
