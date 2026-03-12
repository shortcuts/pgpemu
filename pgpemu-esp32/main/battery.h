#pragma once
#include "sdkconfig.h"
#include <stdint.h>

void init_battery(void);
uint8_t battery_get_percent(void);
uint32_t battery_get_mv(void);
