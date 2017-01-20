#ifndef MUTEX_UTIL_H
#define MUTEX_UTIL_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

inline void mutex_lock(SemaphoreHandle_t mutex) {
  configASSERT(xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE);
}

inline void mutex_unlock(SemaphoreHandle_t mutex) {
  configASSERT(xSemaphoreGive(mutex) == pdTRUE);
}

#endif
