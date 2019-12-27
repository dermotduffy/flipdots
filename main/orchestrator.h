#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include "mode-clock.h"
#include "mode-bounce.h"
#include "mode-snake.h"

void orchestrator_setup();
void orchestrator_start();
void orchestrator_touchpad_input(bool pad0, bool pad1);

bool orchestrator_activate_notification(const char* icon);
bool orchestrator_activate_clock(ClockStyle style);
bool orchestrator_activate_bounce(ModeBounceCoords bounce_input);
bool orchestrator_activate_snake(SnakeDirection snake_input);

#endif
