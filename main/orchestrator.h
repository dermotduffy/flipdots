#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

void orchestrator_setup();
void orchestrator_start();
void orchestrator_touchpad_input(bool pad0, bool pad1);

bool orchestrator_activate_notification(const char* icon);

#endif
