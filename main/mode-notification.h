#ifndef MODE_NOTIFICATION_H
#define MODE_NOTIFICATION_H

#include "mode-base.h"

#define NOTIFICATION_ICON_MIN                     0
#define NOTIFICATION_ICON_MAX                     NOTIFICATION_ICON_WASHING_MACHINE
#define NOTIFICATION_ICON_DEFAULT                 NOTIFICATION_ICON_DOORBELL

#define NOTIFICATION_TAG_DOORBELL                 "test-doorbell"
#define NOTIFICATION_TAG_DISHWASHER               "test-dishwasher_complete"
#define NOTIFICATION_TAG_TUMBLE_DRYER             "test-dryer_complete"
#define NOTIFICATION_TAG_WASHING_MACHINE          "test-wm_complete"

#define NOTIFICATION_DRAWS                        6
#define NOTIFICATION_TIME_DELAY_BETWEEN_DRAWS_MS  1*1000

enum NotificationIcon {
  NOTIFICATION_ICON_DOORBELL = NOTIFICATION_ICON_MIN,
  NOTIFICATION_ICON_DISHWASHER = NOTIFICATION_ICON_MIN+1,
  NOTIFICATION_ICON_TUMBLE_DRYER = NOTIFICATION_ICON_MIN+2,
  NOTIFICATION_ICON_WASHING_MACHINE = NOTIFICATION_ICON_MIN+3,
};

typedef struct {
  enum NotificationIcon notification_icon;
  bool invert;
  int draws;
} ModeNotificationParameters;

void mode_notification_setup();
bool mode_notification_set_icon(int icon);
bool mode_notification_get_icon_by_str(char* str, enum NotificationIcon* icon);
int mode_notification_draw();

#endif
