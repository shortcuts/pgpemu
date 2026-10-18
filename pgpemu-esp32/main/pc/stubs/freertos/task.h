// PC-test stub
#pragma once
#include "freertos/FreeRTOS.h"
static inline TickType_t xTaskGetTickCount(void) {
    return 0;
}
static inline void vTaskList(char* buf) {
    buf[0] = 0;
}
static inline unsigned uxTaskGetStackHighWaterMark(void* t) {
    (void)t;
    return 0;
}
