#include "mgos.h"
#include "mgos_mqtt.h"

#include "orchestrator.h"

void mqtt_notification_handler(
    struct mg_connection *nc, const char *topic,
    int topic_len, const char *msg, int msg_len,
    void *ud) {
  LOG(LL_INFO, (
      "Received data of len %i: %.*s", msg_len, msg_len, msg));

  char* notification_icon = NULL;
  if (json_scanf(msg, msg_len, "{ icon:%Q }", &notification_icon) != 1) {
    LOG(LL_ERROR, ("Could not JSON parse data: %.*s", msg_len, msg));
    return;
  }

  orchestrator_activate_notification(notification_icon);
}

void mqtt_setup() {
  if (mgos_sys_config_get_flipdots_notification_mqtt_sub() == NULL) {
    LOG(LL_ERROR,
        ("Notification MQTT topic not set. "
         "Run 'mos config-set flipdots.notification_mqtt_sub=...'"));
    return;
  }

  mgos_mqtt_sub(mgos_sys_config_get_flipdots_notification_mqtt_sub(),
                mqtt_notification_handler,
                NULL);
}
