// PC-test stub of the FreeRTOS types the firmware headers use
#pragma once
#include <stdint.h>
#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1
#define pdTRUE 1
#define pdFALSE 0
typedef unsigned long TickType_t;
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
#define pdTICKS_TO_MS(t) ((unsigned long)(t))
