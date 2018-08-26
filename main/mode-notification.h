#ifndef MODE_NOTIFICATION_H
#define MODE_NOTIFICATION_H

#include "mode-base.h"

#define NOTIFICATION_ICON_MIN                   0
#define NOTIFICATION_ICON_MAX                   NOTIFICATION_ICON_DOORBELL
#define NOTIFICATION_ICON_DEFAULT               NOTIFICATION_ICON_DOORBELL

#define NOTIFICATION_DRAWS                        6
#define NOTIFICATION_TIME_DELAY_BETWEEN_DRAWS_MS  1*1000


typedef struct {
  enum {
    NOTIFICATION_ICON_DOORBELL = NOTIFICATION_ICON_MIN,
  } notification_icon;
  bool invert;
  int draws;
} ModeNotificationParameters;

void mode_notification_setup();
bool mode_notification_set_icon(int icon);
int mode_notification_draw();

#endif
