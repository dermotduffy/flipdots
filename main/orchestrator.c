#include <assert.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "mode-clock.h"
#include "mutex-util.h"
#include "network.h"

#define TASK_ORCHESTRATOR_NAME        "orchestrator"
#define TASK_ORCHESTRATOR_STACK_WORDS 2<<11
#define TASK_ORCHESTRATOR_PRORITY     6

#define TIME_DELAY_ORCHESTRATOR_MS       1*1000

EventGroupHandle_t orchestrator_event_group;
static xTaskHandle task_orchestrator_handle;

const static char *LOG_TAG = "orchestrator";

// Handle network input. Returns the semaphore associated with the
// newly runnable mode, or NULL on network error.
//
// All mode locks must be held prior to calling this function, such
// that mode specific parameters may be safely changed (as no mode is executing
// at this moment).
//
// Expected format: <uint8_t: mode to activate><uint8_t: mode-specific-data>
static SemaphoreHandle_t orchestrator_network_input() {
  SemaphoreHandle_t new_runnable_mode_mutex = NULL;

  mutex_lock(network_data_mutex);
  if (network_data_buf_used_size == 2) {
    uint8_t mode = network_data_buf[0];
    uint8_t mode_specific_data = network_data_buf[1];
    mutex_unlock(network_data_mutex);

    switch (mode) {
      case 'C':
        mode_clock_network_input(
            &mode_specific_data, sizeof(mode_specific_data));
        new_runnable_mode_mutex = mode_clock_mutex;
        break;
      default:
        ESP_LOGW(LOG_TAG, "Received unrecognised network data. Ignoring.");
    }
  } else {
    mutex_unlock(network_data_mutex);

    ESP_LOGW(LOG_TAG,
        "Received unrecognised network data size (%i). Ignoring.",
        network_data_buf_used_size);
  }
  return new_runnable_mode_mutex;
}

void task_orchestrator() {
  assert(network_event_group != NULL);

  // All mode mutexes come pre-locked.
  
  // Let the clock run the default.
  SemaphoreHandle_t runnable_mode_mutex = mode_clock_mutex;

  while (true) {
    bool network_event_received = false;

    mutex_unlock(runnable_mode_mutex);  // Unblock task.
    if ((xEventGroupWaitBits(                                
        network_event_group,
        NETWORK_EVENT_DATA_READY_BIT,
        pdTRUE,   // Clear on exit.
        pdTRUE,   // Wait for all bits.
        TIME_DELAY_ORCHESTRATOR_MS) & NETWORK_EVENT_DATA_READY_BIT) ==
            NETWORK_EVENT_DATA_READY_BIT) {
      network_event_received = true;
    }
    mutex_lock(runnable_mode_mutex);  // Block task.

    if (network_event_received) {
      SemaphoreHandle_t* new_runnable_mode_mutex = orchestrator_network_input();
      if (new_runnable_mode_mutex) {
        runnable_mode_mutex = new_runnable_mode_mutex;
      }
    }
  }
}

void orchestrator_setup() {
  orchestrator_event_group = xEventGroupCreate();   
  configASSERT(orchestrator_event_group != NULL);

  // Setup mode tasks.
  mode_clock_setup();
}

void orchestrator_start() {
  // Lock mode locks before initing tasks or starting the orchestrator.
  mutex_lock(mode_clock_mutex);

  configASSERT(xTaskCreatePinnedToCore(
      task_orchestrator,
      TASK_ORCHESTRATOR_NAME,
      TASK_ORCHESTRATOR_STACK_WORDS,
      NULL,
      TASK_ORCHESTRATOR_PRORITY,
      &task_orchestrator_handle,
      tskNO_AFFINITY) == pdTRUE);

  // Start orchestrator modes.
  mode_clock_start();
}
