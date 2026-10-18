// PC-test stub of the FreeRTOS types the firmware headers use
#pragma once
#include <stdint.h>
#define portMAX_DELAY 0xFFFFFFFF
typedef unsigned long TickType_t;
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
