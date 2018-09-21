#include <math.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "displaybuffer.h"
#include "displaydriver.h"
#include "mode-bounce.h"

#define MODE_BOUNCE_RADIUS 2

const static char *LOG_TAG = "mode-bounce";

ModeBounceParameters mode_bounce_params;

int mode_bounce_draw() {
  buffer_wipe(&buffer_draw);
  buffer_fill_circle(
      mode_bounce_params.pos.x, mode_bounce_params.pos.y, MODE_BOUNCE_RADIUS,
      PIXEL_YELLOW, &buffer_draw);

  mode_bounce_params.pos.x += mode_bounce_params.rate.x;
  mode_bounce_params.pos.y += mode_bounce_params.rate.y;

  if (mode_bounce_params.pos.x <= MODE_BOUNCE_RADIUS) {
    mode_bounce_params.pos.x = MODE_BOUNCE_RADIUS;
    mode_bounce_params.rate.x = -mode_bounce_params.rate.x;
  } else if (mode_bounce_params.pos.x >= DISPLAY_WIDTH - 1 - MODE_BOUNCE_RADIUS) {
    mode_bounce_params.pos.x = DISPLAY_WIDTH - 1 - MODE_BOUNCE_RADIUS;
    mode_bounce_params.rate.x = -mode_bounce_params.rate.x;
  }

  if (mode_bounce_params.pos.y <= MODE_BOUNCE_RADIUS) {
    mode_bounce_params.pos.y = MODE_BOUNCE_RADIUS;
    mode_bounce_params.rate.y = -mode_bounce_params.rate.y;
  } else if (mode_bounce_params.pos.y >= DISPLAY_HEIGHT - 1 - MODE_BOUNCE_RADIUS) {
    mode_bounce_params.pos.y = DISPLAY_HEIGHT - 1 - MODE_BOUNCE_RADIUS;
    mode_bounce_params.rate.y = -mode_bounce_params.rate.y;
  }

  buffer_commit_drawing();
  
  return BOUNCE_TIME_DELAY_BETWEEN_DRAWS_MS;
}

void mode_bounce_setup() {
  mode_bounce_params.pos.x = esp_random() % DISPLAY_WIDTH;
  mode_bounce_params.pos.y = esp_random() % DISPLAY_HEIGHT;
  mode_bounce_params.rate.x = 0.5 + (((esp_random() % 50)+1) / (double)100);
  mode_bounce_params.rate.y = 0.5 + (((esp_random() % 50)+1) / (double)100);
}

bool mode_bounce_rel_direction_input(const ModeBounceCoords input) {
  double x = mode_bounce_params.rate.x + input.x;
  double y = mode_bounce_params.rate.y + input.y;

  if (x > 1) {
    x = 1;
  } else if (x < -1) {
    x = -1;
  }

  if (y > 1) {
    y = 1;
  } else if (y < -1) {
    y = -1;
  }

  mode_bounce_params.rate.x = x;
  mode_bounce_params.rate.y = y;
  return true;
}
