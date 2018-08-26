#include <assert.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "mgos.h"
#include "mgos_rpc.h"

#include "mode-clock.h"
#include "mode-notification.h"
#include "mutex-util.h"

#define TASK_ORCHESTRATOR_NAME           "orchestrator"
#define TASK_ORCHESTRATOR_STACK_WORDS    2<<11
#define TASK_ORCHESTRATOR_PRIORITY       3

#define TIME_DELAY_ORCHESTRATOR_ERROR_MS 60*1000

static xTaskHandle task_orchestrator_handle;

EventGroupHandle_t orchestrator_event_group;
#define ORCHESTRATOR_EVENT_WAKEUP_BIT         BIT0

SemaphoreHandle_t orchestrator_mutex = NULL;

#define ORCHESTRATOR_MODE_MIN       0
enum {
  ORCHESTRATOR_MODE_CLOCK = NOTIFICATION_ICON_MIN,
  ORCHESTRATOR_MODE_NOTIFICATION,
} orchestrator_mode;
#define ORCHESTRATOR_MODE_MAX       ORCHESTRATOR_MODE_NOTIFICATION
#define ORCHESTRATOR_MODE_DEFAULT   ORCHESTRATOR_MODE_CLOCK

#define RPC_MBUF_SIZE 50

const static char *LOG_TAG = "orchestrator";

void task_orchestrator() {
  int pause_ms;

  while (true) {
    mutex_lock(orchestrator_mutex);
    switch (orchestrator_mode) {  
      case ORCHESTRATOR_MODE_CLOCK:
        pause_ms = mode_clock_draw();
        break;
      case ORCHESTRATOR_MODE_NOTIFICATION:
        pause_ms = mode_notification_draw();
        break;
      default:
        ESP_LOGE(LOG_TAG, "Invalid mode: %i", orchestrator_mode);
        pause_ms = TIME_DELAY_ORCHESTRATOR_ERROR_MS;
    }

    if (pause_ms == 0) {
      orchestrator_mode = ORCHESTRATOR_MODE_DEFAULT;
      mutex_unlock(orchestrator_mutex);
      continue;
    } else {
      mutex_unlock(orchestrator_mutex);
    }

    int pause_ticks = pause_ms / portTICK_PERIOD_MS;
    int paused_ticks = 0;

    while (true) {
      TickType_t tick_start = xTaskGetTickCount();
      if (xEventGroupWaitBits(
          orchestrator_event_group,
          ORCHESTRATOR_EVENT_WAKEUP_BIT,
          pdTRUE,   // Clear on exit.
          pdTRUE,   // Wait for all bits.
          pause_ticks - paused_ticks) & ORCHESTRATOR_EVENT_WAKEUP_BIT) {
        break;
      }
      paused_ticks += (xTaskGetTickCount() - tick_start);
      if (paused_ticks >= pause_ticks) {
        break;
      }
    }
  }

  // Never reached.
  vTaskDelete(NULL);
  return; 
}

static void rpc_clock_handler(
    struct mg_rpc_request_info *ri, void *cb_arg,
    struct mg_rpc_frame_info *fi, struct mg_str args) {
  struct mbuf fb;
  struct json_out out = JSON_OUT_MBUF(&fb);
  mbuf_init(&fb, RPC_MBUF_SIZE);

  int style = 0;
  if (json_scanf(args.p, args.len, ri->args_fmt, &style) != 1) {
    json_printf(&out, "{error: %Q}", "style is required");
  } else {
    mutex_lock(orchestrator_mutex);
    bool style_success = mode_clock_set_style(style);
    orchestrator_mode = ORCHESTRATOR_MODE_CLOCK;
    mutex_unlock(orchestrator_mutex);
    
    if (!style_success) {
      json_printf(&out, "{error: %Q}", "Invalid clock style");
    } else {
      json_printf(&out, "{style: %d}", style);
      xEventGroupSetBits(orchestrator_event_group, ORCHESTRATOR_EVENT_WAKEUP_BIT);
    }
  }

  mg_rpc_send_responsef(ri, "%.*s", fb.len, fb.buf);

  ri = NULL;
  mbuf_free(&fb);
  (void) cb_arg;
  (void) fi;
}

static void rpc_notification_handler(
    struct mg_rpc_request_info *ri, void *cb_arg,
    struct mg_rpc_frame_info *fi, struct mg_str args) {
  struct mbuf fb;
  struct json_out out = JSON_OUT_MBUF(&fb);
  mbuf_init(&fb, RPC_MBUF_SIZE);

  int icon = 0;
  if (json_scanf(args.p, args.len, ri->args_fmt, &icon) != 1) {
    json_printf(&out, "{error: %Q}", "icon is required");
  } else {
    mutex_lock(orchestrator_mutex);
    bool icon_success = mode_notification_set_icon(icon);
    orchestrator_mode = ORCHESTRATOR_MODE_NOTIFICATION;
    mutex_unlock(orchestrator_mutex);
    
    if (!icon_success) {
      json_printf(&out, "{error: %Q}", "Invalid notification icon");
    } else {
      json_printf(&out, "{icon: %d}", icon);
      xEventGroupSetBits(orchestrator_event_group, ORCHESTRATOR_EVENT_WAKEUP_BIT);
    }
  }

  mg_rpc_send_responsef(ri, "%.*s", fb.len, fb.buf);

  ri = NULL;
  mbuf_free(&fb);
  (void) cb_arg;
  (void) fi;
}

void orchestrator_setup() {
  orchestrator_event_group = xEventGroupCreate();   
  configASSERT(orchestrator_event_group != NULL);

  orchestrator_mutex = xSemaphoreCreateMutex();
  configASSERT(orchestrator_mutex != NULL);

  mg_rpc_add_handler(mgos_rpc_get_global(),
      "FlipDot.Clock", "{style: %d}",
      rpc_clock_handler, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(),
      "FlipDot.Notification", "{icon: %d}",
      rpc_notification_handler, NULL);

  orchestrator_mode = ORCHESTRATOR_MODE_DEFAULT;

  // Setup mode tasks.
  mode_clock_setup();
  mode_notification_setup();
}

void orchestrator_start() {
  configASSERT(xTaskCreatePinnedToCore(
      task_orchestrator,
      TASK_ORCHESTRATOR_NAME,
      TASK_ORCHESTRATOR_STACK_WORDS,
      NULL,
      TASK_ORCHESTRATOR_PRIORITY,
      &task_orchestrator_handle,
      tskNO_AFFINITY) == pdTRUE);
}
