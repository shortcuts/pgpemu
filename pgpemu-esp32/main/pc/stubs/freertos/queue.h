// PC-test stub: xQueueSend is a counting mock defined in the test
#pragma once
#include "freertos/FreeRTOS.h"
int xQueueSend(QueueHandle_t q, const void* item, TickType_t wait);
