#ifndef STATUS_LED_H
#define STATUS_LED_H

#include "driver/gpio.h"

#define PIN_STATUS_LED GPIO_NUM_0

void status_led_setup() {
  gpio_set_direction(PIN_STATUS_LED, GPIO_MODE_OUTPUT);
}

void status_led_set(bool on) {
  gpio_set_level(PIN_STATUS_LED, on);
}

#endif
