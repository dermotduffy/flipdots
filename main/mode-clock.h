#ifndef MODE_CLOCK_H
#define MODE_CLOCK_H

#include "freertos/semphr.h"
#include "mode-base.h"
#include "displaybuffer.h"

#define TASK_MODE_CLOCK_NAME        "mode-clock"
#define TASK_MODE_CLOCK_STACK_WORDS 2<<11
#define TASK_MODE_CLOCK_PRIORITY    TASK_MODE_PRIORITY

extern SemaphoreHandle_t mode_clock_semaphore;

void mode_clock_init(displaybuffer_t* displaybuffer);

static void task_mode_clock();
static void draw_hours(int hours, displaybuffer_t* displaybuffer);
static void draw_minutes(int mins, displaybuffer_t* displaybuffer);

#endif
