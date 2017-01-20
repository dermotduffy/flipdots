#ifndef MODE_CLOCK_H
#define MODE_CLOCK_H

#include "freertos/semphr.h"
#include "mode-base.h"
#include "displaybuffer.h"

#define TASK_MODE_CLOCK_NAME        "mode-clock"
#define TASK_MODE_CLOCK_STACK_WORDS 2<<11
#define TASK_MODE_CLOCK_PRIORITY    TASK_MODE_PRIORITY

typedef struct {
  enum style {
    CLOCK_STYLE_DIGITAL,
    CLOCK_STYLE_HOUR_ONLY,
  } clock_style;
} ModeClockParameters;

extern SemaphoreHandle_t mode_clock_mutex;

// mode_clock_params protected by mode_clock_mutex.
extern ModeClockParameters mode_clock_params;

void mode_clock_init();

#endif
