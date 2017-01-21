#ifndef DISPLAYDRIVER_H
#define DISPLAYDRIVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "displaybuffer.h"

void displaydriver_setup();
void displaydriver_start();
void displaydriver_testpattern();

bool buffer_commit_drawing();

extern SemaphoreHandle_t buffer_staging_mutex; 

extern displaybuffer_t buffer_live;    // On the display currently.
extern displaybuffer_t buffer_prelive; // Being written to the display.
extern displaybuffer_t buffer_staging; // Complete frames held before writing.
extern displaybuffer_t buffer_draw;    // Drawing happens here.

EventGroupHandle_t display_event_group;
#define DISPLAY_EVENT_COMMIT_BIT                     BIT0
#define DISPLAY_EVENT_WORKER_TOP_ACTIVATE_BIT        BIT1
#define DISPLAY_EVENT_WORKER_BOT_ACTIVATE_BIT        BIT2
#define DISPLAY_EVENT_WORKER_TOP_ACTIVATE_FULL_BIT   BIT3
#define DISPLAY_EVENT_WORKER_BOT_ACTIVATE_FULL_BIT   BIT4
#define DISPLAY_EVENT_WORKER_TOP_COMPLETE_BIT        BIT5
#define DISPLAY_EVENT_WORKER_BOT_COMPLETE_BIT        BIT6

#endif
