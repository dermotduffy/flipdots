#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mode-clock.h"
#include "mutex-util.h"

#define TASK_ORCHESTRATOR_NAME        "orchestrator"
#define TASK_ORCHESTRATOR_STACK_WORDS 2<<11
#define TASK_ORCHESTRATOR_PRORITY     6

#define TIME_DELAY_ORCHESTRATOR_MS       1*1000

EventGroupHandle_t orchestrator_event_group;
static xTaskHandle task_orchestrator_handle;

const static char *LOG_TAG = "orchestrator";

void task_orchestrator() {
  // All mode mutexes come pre-locked.
  
  // Let the clock run the default.
  SemaphoreHandle_t runnable_task_mutex = mode_clock_mutex;

  while (true) {
    mutex_unlock(runnable_task_mutex);
    vTaskDelay(TIME_DELAY_ORCHESTRATOR_MS / portTICK_PERIOD_MS);
    mutex_lock(runnable_task_mutex);

    // For now default behaviour is to do nothing. Future:
    // - Reach to input (physical / network)
    // - Activate appropriate modes.
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
