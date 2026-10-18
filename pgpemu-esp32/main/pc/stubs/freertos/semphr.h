// PC-test stub: mutexes always succeed; the lock is a non-NULL token
#pragma once
#include "freertos/FreeRTOS.h"
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return (SemaphoreHandle_t)1;
}
static inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
    (void)s, (void)wait;
    return pdTRUE;
}
static inline int xSemaphoreGive(SemaphoreHandle_t s) {
    (void)s;
    return pdTRUE;
}
