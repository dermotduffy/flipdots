#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "displaydriver.h"
#include "network.h"
#include "orchestrator.h"
#include "time-util.h"

const static char *LOG_TAG = "flipdots";

// ****
// Main
// ****
void app_main(void)
{
  nvs_flash_init();

  networking_setup();
  networking_start();

  time_sntp_start();

  displaydriver_setup();
  displaydriver_start();

  // === Display is functional ===
  // On start, do an all-pixel test.
  // (Must be prior to orchestrator taking over the display).
  displaydriver_testpattern();

  // Setup & start the orchestrator.
  orchestrator_setup();
  orchestrator_start();
}
