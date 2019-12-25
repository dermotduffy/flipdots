#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "driver/touch_pad.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#include "touchpad.h"
#include "orchestrator.h"

static const char* TAG = "Touch pad";

#define TOUCH_THRESH_NO_USE           0
#define TOUCHPAD_FILTER_TOUCH_PERIOD  10

static xTaskHandle task_touchpad_handle;
static EventGroupHandle_t touchpad_event_group;
#define TOUCHPAD_EVENT_WAKEUP_BIT         BIT0

#define TOUCHPAD_DEBOUNCE_MS          200
#define TOUCHPAD_FILTER_START_MS      200

#define TASK_TOUCHPAD_NAME            "touchpad"
#define TASK_TOUCHPAD_STACK_WORDS     2<<11
#define TASK_TOUCHPAD_PRIORITY        7

#define NUM_TOUCHPADS                 2
static int touchpads[] =              {0, 2};

static bool touchpads_activated[TOUCH_PAD_MAX];

static void task_touchpad(void *pvParameter) {
  bool pad0 = false, pad1 = false;

  while (true) {
    if ((xEventGroupWaitBits(
          touchpad_event_group,
          TOUCHPAD_EVENT_WAKEUP_BIT,
          pdTRUE,   // Clear on exit.
          pdTRUE,   // Wait for all bits.
          2000 / portTICK_PERIOD_MS) & TOUCHPAD_EVENT_WAKEUP_BIT) == 0) {
      continue;
    }

    pad0 = pad1 = false;

    if (touchpads_activated[touchpads[0]] == true) {
      pad0 = true;
    }

    if (touchpads_activated[touchpads[1]] == true) {
      pad1 = true;
    }

    if (pad0 || pad1) {
      orchestrator_touchpad_input(pad0, pad1);

      // Wait a while for the pad being released
      vTaskDelay(TOUCHPAD_DEBOUNCE_MS / portTICK_PERIOD_MS);
    }

    for (int i = 0; i < NUM_TOUCHPADS; ++i) {
      if (touchpads_activated[touchpads[i]] == true) {
        touchpads_activated[touchpads[i]] = false;
      }
    }
  }
}

static void touchpad_intr(void *arg) {
  uint32_t pad_intr = touch_pad_get_status();

  for (int i = 0; i < NUM_TOUCHPADS; ++i) {
    if ((pad_intr >> touchpads[i]) & 0x01) {
      touchpads_activated[touchpads[i]] = true;
    }
  }

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xEventGroupSetBitsFromISR(
      touchpad_event_group,
      TOUCHPAD_EVENT_WAKEUP_BIT,
      &xHigherPriorityTaskWoken) != pdFAIL) {
        portYIELD_FROM_ISR();
  }

  touch_pad_clear_status();
}

void touchpad_setup() {
  touchpad_event_group = xEventGroupCreate();
  configASSERT(touchpad_event_group != NULL);

  ESP_LOGI(TAG, "Initializing touch pad");
  touch_pad_init();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

  for (int i = 0; i < NUM_TOUCHPADS; ++i) {
    touch_pad_config(touchpads[i], TOUCH_THRESH_NO_USE);
  }

  vTaskDelay(TOUCHPAD_FILTER_START_MS / portTICK_PERIOD_MS);

  uint16_t touch_value;
  for (int i = 0; i < NUM_TOUCHPADS; ++i) {
    touch_pad_read(touchpads[i], &touch_value);
    ESP_LOGD(TAG, "touchpads: touch pad [%d] init val is %d", touchpads[i], touch_value);
    ESP_ERROR_CHECK(touch_pad_set_thresh(touchpads[i], touch_value * 2 / 3));
  }

  touch_pad_isr_register(touchpad_intr, NULL);
}

void touchpad_start() {
  configASSERT(xTaskCreatePinnedToCore(
      task_touchpad,
      TASK_TOUCHPAD_NAME,
      TASK_TOUCHPAD_STACK_WORDS,
      NULL,
      TASK_TOUCHPAD_PRIORITY,
      &task_touchpad_handle,
      tskNO_AFFINITY) == pdTRUE);

  touch_pad_intr_enable();
}
