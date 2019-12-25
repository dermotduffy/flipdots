#include <assert.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mgos.h"
#include "mgos_blynk.h"
#include "mgos_mqtt.h"
#include "mgos_rpc.h"
#include "mgos_sys_config.h"

#include "mode-bounce.h"
#include "mode-clock.h"
#include "mode-notification.h"
#include "mode-snake.h"
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
enum OrchestratorMode {
  ORCHESTRATOR_MODE_CLOCK = ORCHESTRATOR_MODE_MIN,
  ORCHESTRATOR_MODE_NOTIFICATION,
  ORCHESTRATOR_MODE_BOUNCE,
  ORCHESTRATOR_MODE_SNAKE,
} orchestrator_mode;
#define ORCHESTRATOR_MODE_MAX       ORCHESTRATOR_MODE_SNAKE
#define ORCHESTRATOR_MODE_DEFAULT   ORCHESTRATOR_MODE_CLOCK

#define RPC_MBUF_SIZE 50

#define BLYNK_PIN_MODE        0
#define BLYNK_PIN_CLOCK_STYLE 1
#define BLYNK_PIN_DIRECTION_X 2
#define BLYNK_PIN_DIRECTION_Y 3

#define BLYNK_DIRECTION_CUTOFF 0.5

#define MQTT_BUF_SIZE 50

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
      case ORCHESTRATOR_MODE_BOUNCE:
        pause_ms = mode_bounce_draw();
        break;
      case ORCHESTRATOR_MODE_SNAKE:
        pause_ms = mode_snake_draw();
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

// orchestrator mutex needs to be held.
void set_orchestrator_mode_locked(int value) {
  if (value < ORCHESTRATOR_MODE_MIN || value > ORCHESTRATOR_MODE_MAX) {
    return;
  }
  orchestrator_mode = value;
  if (orchestrator_mode == ORCHESTRATOR_MODE_SNAKE) {
    mode_snake_reset_game();
  } else if (orchestrator_mode == ORCHESTRATOR_MODE_NOTIFICATION) {
    mode_notification_set_icon(NOTIFICATION_ICON_DEFAULT);
  }
}

void blynk_event_mode(int value) {
  mutex_lock(orchestrator_mutex);

  // Blynk modes are indexed from 1, orchestrator modes from 0.
  set_orchestrator_mode_locked(value - 1);
  mutex_unlock(orchestrator_mutex);
}

#define abs(x)  ( ( (x) < 0) ? -(x) : (x) )

void blynk_event_direction(int value, bool x_axis) {
  double x;
  double y;

  if (x_axis) {
    x = (value - 512) / (double)512.0;
    if (abs(x) < BLYNK_DIRECTION_CUTOFF) {
      return;
    }
    y = 0;
  } else {
    x = 0;
    y = (value - 512) / (double)512.0;
    if (abs(y) < BLYNK_DIRECTION_CUTOFF) {
      return;
    }
  }

  ModeBounceCoords bounce_input;
  SnakeDirection snake_input;

  mutex_lock(orchestrator_mutex);
  switch (orchestrator_mode) {
    case ORCHESTRATOR_MODE_BOUNCE:
      bounce_input.x = x;
      bounce_input.y = -y;
      mode_bounce_rel_direction_input(bounce_input);
      break;
    case ORCHESTRATOR_MODE_SNAKE:
      if (abs(x) > abs(y)) {
        if (x > 0) {
          snake_input = SNAKE_EAST;
        } else {
          snake_input = SNAKE_WEST;
        }
      } else {
        if (y > 0) {
          snake_input = SNAKE_NORTH;
        } else {
          snake_input = SNAKE_SOUTH;
        }
      }
      mode_snake_direction_input(snake_input);
      break;
    default:
      break;
  }
  mutex_unlock(orchestrator_mutex);
}

void blynk_event(
    struct mg_connection *conn, const char *cmd,
    int pin, int val, int id, void *user_data) {
  // cmd: vw / vr
  // pin: The virtual pin number.
  // val: The value of the pin.
  // id: sequence id (?)

  ESP_LOGI(LOG_TAG, "Blynk event: Cmd (%s), pin %i -> %i", cmd, pin, val);

  if (strcmp("vw", cmd) != 0) {
    ESP_LOGW(LOG_TAG, "Received unsupported blynk command: %s", cmd);
    return;
  }

  switch (pin) {
    case BLYNK_PIN_MODE:
      blynk_event_mode(val);
      break;
    case BLYNK_PIN_CLOCK_STYLE:
      mode_clock_set_style(val);
      break;
    case BLYNK_PIN_DIRECTION_X:
      blynk_event_direction(val, true);
      break;
    case BLYNK_PIN_DIRECTION_Y:
      blynk_event_direction(val, false);
      break;
    default:
      ESP_LOGW(LOG_TAG, "Unknown blynk pin: %i", pin);
  }
}

// Expects a JSON list of tags, which is searched sequentially for the first
// one that has a matching notification icon.
void mqtt_scan_payload(const char *str, int len, void *user_data) {
  struct json_token t;
  int i;
  char buf[MQTT_BUF_SIZE+1];

  enum NotificationIcon icon;
  for (i = 0; json_scanf_array_elem(str, len, "", i, &t) > 0; i++) {
    int len = t.len > MQTT_BUF_SIZE ? MQTT_BUF_SIZE : t.len;
    strncpy(buf, t.ptr, len);
    buf[len] = '\0';

    if (mode_notification_get_icon_by_str(buf, &icon)) {
      mutex_lock(orchestrator_mutex);
      mode_notification_set_icon(icon);
      orchestrator_mode = ORCHESTRATOR_MODE_NOTIFICATION;
      mutex_unlock(orchestrator_mutex);
      return;
    }
  }
}

void orchestrator_mqtt_handler(
    struct mg_connection *c, int ev, void *p, void *user_data) {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *) p;

  if (ev == MG_EV_MQTT_CONNACK) {
    if (mgos_sys_config_get_mqtt_sub() == NULL) {
      LOG(LL_ERROR, ("MQTT: Topic not set. Run 'mos config-set mqtt.sub=...'"));
    } else {
      struct mg_mqtt_topic_expression te = {
          .topic = mgos_sys_config_get_mqtt_sub(),
          .qos = 1,
      };
      uint16_t sub_id = mgos_mqtt_get_packet_id();
      LOG(LL_INFO, (
          "MQTT: Subscribing to '%s' (QoS %u, id %u)", te.topic, te.qos, sub_id));
      mg_mqtt_subscribe(c, &te, 1, sub_id);
    }
  } else if (ev == MG_EV_MQTT_SUBACK) {
    LOG(LL_INFO, ("MQTT: Subscription %u acknowledged", msg->message_id));
  } else if (ev == MG_EV_MQTT_PUBLISH) {
    struct mg_str *s = &msg->payload;
    LOG(LL_INFO, ("MQTT: Got data: %.*s", (int) s->len, s->p));

    // Our MQTT sub is @ QoS 1: Need to ACK the message.
    mg_mqtt_puback(c, msg->message_id);
    if (json_scanf(s->p, s->len, "[%M]", mqtt_scan_payload, NULL) != 1) {
      LOG(LL_ERROR, ("MQTT: Could not JSON parse data: %.*s", (int) s->len, s->p));
    }
  }
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
  mode_bounce_setup();
  mode_snake_setup();

  // Blynk init.
  mgos_blynk_init();
  blynk_set_handler(blynk_event, NULL);

  // MQTT init.
  mgos_mqtt_add_global_handler(orchestrator_mqtt_handler, NULL);
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

void orchestrator_touchpad_input(bool pad0, bool pad1) {
  if (!pad0 && !pad1) {
    return;
  }
  ESP_LOGI(LOG_TAG, "Orchestrator touchpad wake: %i,%i", pad0, pad1);
  mutex_lock(orchestrator_mutex);
  if (pad0 && pad1) {
    enum OrchestratorMode mode = orchestrator_mode + 1;
    if (mode > ORCHESTRATOR_MODE_MAX) {
      mode = ORCHESTRATOR_MODE_MIN;
    }
    set_orchestrator_mode_locked(mode);
    xEventGroupSetBits(orchestrator_event_group, ORCHESTRATOR_EVENT_WAKEUP_BIT);
  } else {
    ModeBounceCoords coords;

    switch (orchestrator_mode) {
      case ORCHESTRATOR_MODE_SNAKE:
        mode_snake_rel_direction_input(pad0, pad1);
        break;
      case ORCHESTRATOR_MODE_BOUNCE:
        if (pad1) {
          coords.x = 1;
        } else {
          coords.x = -1;
        }
        mode_bounce_rel_direction_input(coords);
      default:
        break;
    }
  }
  mutex_unlock(orchestrator_mutex);
}
