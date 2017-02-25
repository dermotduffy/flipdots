#ifndef MODE_CLOCK_H
#define MODE_CLOCK_H

#include "freertos/semphr.h"

#include "mode-base.h"
#include "displaybuffer.h"

#define TASK_MODE_CLOCK_NAME        "mode-clock"
#define TASK_MODE_CLOCK_STACK_WORDS 2<<11
#define TASK_MODE_CLOCK_PRIORITY    TASK_MODE_PRIORITY

#define CLOCK_STYLE_MIN             0
#define CLOCK_STYLE_MAX             CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS
#define CLOCK_STYLE_DEFAULT         CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS

typedef struct {
  enum style {
    CLOCK_STYLE_DIGITAL = CLOCK_STYLE_MIN,
    CLOCK_STYLE_HOUR_ONLY,
    CLOCK_STYLE_DIGITAL_HOURS_ANALOG_MINS,
    CLOCK_STYLE_SMALL_DIGITAL_HOURS_ANALOG_MINS,
  } clock_style;
} ModeClockParameters;

extern SemaphoreHandle_t mode_clock_mutex;
extern ModeClockParameters mode_clock_params;  // Protected by mode_clock_mutex.

void mode_clock_setup();  // Prepare task.
void mode_clock_start();  // Start task.
bool mode_clock_network_input(const uint8_t* data, int bytes);

#endif
