#ifndef MODE_CLOCK_H
#define MODE_CLOCK_H

#include "mode-base.h"

#define CLOCK_STYLE_MIN             0
#define CLOCK_STYLE_MAX             CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS
#define CLOCK_STYLE_DEFAULT         CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS

#define CLOCK_TIME_DELAY_BETWEEN_DRAWS_MS       1*500
#define CLOCK_TIME_DELAY_BETWEEN_DRAWS_ERROR_MS 10*1000

typedef enum {
  CLOCK_STYLE_DIGITAL = CLOCK_STYLE_MIN,
  CLOCK_STYLE_HOUR_ONLY,
  CLOCK_STYLE_DIGITAL_HOURS_ANALOG_MINS,
  CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS,
} ClockStyle;

typedef struct {
  ClockStyle clock_style;
} ModeClockParameters;

extern ModeClockParameters mode_clock_params;

void mode_clock_setup();
int mode_clock_draw();
bool mode_clock_set_style(int style);

#endif
