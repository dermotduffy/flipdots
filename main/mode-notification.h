#ifndef MODE_NOTIFICATION_H
#define MODE_NOTIFICATION_H

#include "freertos/semphr.h"

#include "mode-base.h"
#include "displaybuffer.h"

#define TASK_MODE_NOTIFICATION_NAME             "mode-notification"
#define TASK_MODE_NOTIFICATION_STACK_WORDS      2<<11
#define TASK_MODE_NOTIFICATION_PRIORITY         TASK_MODE_PRIORITY

#define NOTIFICATION_ICON_MIN                   0
#define NOTIFICATION_ICON_MAX                   NOTIFICATION_ICON_DOORBELL
#define NOTIFICATION_ICON_DEFAULT               NOTIFICATION_ICON_DOORBELL

#define NOTIFICATION_ACTIVE_SECONDS             10

typedef struct {
  enum {
    NOTIFICATION_ICON_DOORBELL = NOTIFICATION_ICON_MIN,
  } notification_icon;
} ModeNotificationParameters;

extern SemaphoreHandle_t mode_notification_mutex;
extern ModeNotificationParameters mode_notification_params;  // Protected by mode_clock_mutex.

void mode_notification_setup();
void mode_notification_start();
bool mode_notification_network_input(const uint8_t* data, int bytes);

#endif
