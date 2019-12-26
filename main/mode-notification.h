#ifndef MODE_NOTIFICATION_H
#define MODE_NOTIFICATION_H

#include "mgos_bmp_loader.h"

#include "mode-base.h"

#define NOTIFICATION_DRAWS                        6
#define NOTIFICATION_TIME_DELAY_BETWEEN_DRAWS_MS  1*1000
#define NOTIFICATION_FILENAME_LEN_MAX 50
#define NOTIFICATION_DEFAULT_ICON                 "emoticon-happy"

typedef struct {
  bmpread_t* bmp;
  bool invert;
  int draws;
} ModeNotificationParameters;

void mode_notification_setup();
bool mode_notification_set_icon(const char* icon);
int mode_notification_draw();

#endif
