#include "mgos.h"
#include "mgos_blynk.h"

#include "orchestrator.h"

#define abs(x)  ( ( (x) < 0) ? -(x) : (x) )

#define BLYNK_PIN_CLOCK_STYLE        0
#define BLYNK_PIN_NOTIFICATION_ICON  1
#define BLYNK_PIN_DIRECTION_BOUNCE_X 2
#define BLYNK_PIN_DIRECTION_BOUNCE_Y 3
#define BLYNK_PIN_DIRECTION_SNAKE_X  4
#define BLYNK_PIN_DIRECTION_SNAKE_Y  5

#define BLYNK_DIRECTION_CUTOFF       0.5

// The order of items in the blynk notification menu, mapped to notification
// icon names.
const char* blynk_notification_menu[] = {
    "alarm-light",
    "dishwasher",
    "doorbell",
    "emoticon-happy",
    "shield-home",
    "tumble-dryer",
    "washing-machine",
};

void blynk_clock_style(int val) {
  orchestrator_activate_clock(val-1);
}

void blynk_notification_icon(int val) {
  orchestrator_activate_notification(blynk_notification_menu[val-1]);
}

void blynk_normalize_direction(int value, bool x_axis, double* x, double* y) {
  if (x_axis) {
    *x = (value - 512) / (double)512.0;
    *y = 0;
  } else {
    *x = 0;
    *y = (value - 512) / (double)512.0;
  }
}

bool blynk_sufficient_movement(bool x_axis, double x, double y) {
  return ((x_axis && abs(x) >= BLYNK_DIRECTION_CUTOFF) ||
          (!x_axis && abs(y) >= BLYNK_DIRECTION_CUTOFF));
}

void blynk_bounce_direction(int val, bool x_axis) {
  double x, y;
  blynk_normalize_direction(val, x_axis, &x, &y);
  if (!blynk_sufficient_movement(x_axis, x, y)) {
    return;
  }

  ModeBounceCoords bounce_input = {
    .x = x,
    .y = -y,
  };

  orchestrator_activate_bounce(bounce_input);
}

void blynk_snake_direction(int val, bool x_axis) {
  double x, y;
  blynk_normalize_direction(val, x_axis, &x, &y);
  if (!blynk_sufficient_movement(x_axis, x, y)) {
    return;
  }

  SnakeDirection snake_input;

  if (abs(x) > abs(y)) {
    snake_input = x > 0 ? SNAKE_EAST : SNAKE_WEST;
  } else {
    snake_input = y > 0 ? SNAKE_NORTH : SNAKE_SOUTH;
  }

  orchestrator_activate_snake(snake_input);
}

void blynk_event(
    struct mg_connection *conn, const char *cmd,
    int pin, int val, int id, void *user_data) {
  // cmd: vw / vr
  // pin: The virtual pin number.
  // val: The value of the pin.
  // id: sequence id (?)

  LOG(LL_INFO, ("Blynk event: Cmd (%s), pin %i -> %i", cmd, pin, val));

  if (strcmp("vw", cmd) != 0) {
    LOG(LL_ERROR, ("Received unsupported blynk command: %s", cmd));
    return;
  }

  switch (pin) {
    case BLYNK_PIN_CLOCK_STYLE:
      blynk_clock_style(val);
      break;
    case BLYNK_PIN_NOTIFICATION_ICON:
      blynk_notification_icon(val);
      break;
    case BLYNK_PIN_DIRECTION_BOUNCE_X:
      blynk_bounce_direction(val, true);
      break;
    case BLYNK_PIN_DIRECTION_BOUNCE_Y:
      blynk_bounce_direction(val, false);
      break;
    case BLYNK_PIN_DIRECTION_SNAKE_X:
      blynk_snake_direction(val, true);
      break;
    case BLYNK_PIN_DIRECTION_SNAKE_Y:
      blynk_snake_direction(val, false);
      break;
    default:
      LOG(LL_WARN, ("Unknown blynk pin: %i", pin));
  }
}

void blynk_setup() {
  // Blynk init.
  mgos_blynk_init();
  blynk_set_handler(blynk_event, NULL);
}
