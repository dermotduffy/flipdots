#ifndef NETWORK_H
#define NETWORK_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

// Define these, or run make as ...
//   EXTRA_CPPFLAGS='-DWIFI_SSID=\"SSID\" -DWIFI_PASSWORD=\"PASSWORD\"' make
//
// TODO: Remove fake entries when ready for testing.
#define WIFI_SSID            "FakeTestSSID"
#define WIFI_PASSWORD        "FakeTestPassword"

extern EventGroupHandle_t network_event_group;
#define NETWORK_EVENT_CONNECTED_BIT  BIT0  // Connected?
#define NETWORK_EVENT_CHANGE_BIT     BIT1  // State changed?
#define NETWORK_EVENT_DATA_READY_BIT BIT2  // Data ready?

#define NETWORK_DATA_BUF_SIZE  100
// network_data_* protected by network_data_mutex.
extern SemaphoreHandle_t network_data_mutex;
extern uint8_t network_data_buf[NETWORK_DATA_BUF_SIZE];
extern uint8_t network_data_buf_used_size;

void networking_setup();
void networking_start();

#endif
