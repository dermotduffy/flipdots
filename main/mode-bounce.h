#ifndef MODE_BOUNCE_H
#define MODE_BOUNCE_H

#include "mode-base.h"

#define BOUNCE_TIME_DELAY_BETWEEN_DRAWS_MS       1*50

typedef struct {
  double x;
  double y;
} ModeBounceCoords;

typedef struct {
  ModeBounceCoords pos;
  ModeBounceCoords rate;
} ModeBounceParameters;

void mode_bounce_setup();
int mode_bounce_draw();
bool mode_bounce_rel_direction_input(const ModeBounceCoords input);

#endif
