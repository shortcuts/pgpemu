#pragma once
#include "sdkconfig.h"

#include <stdint.h>

void init_battery(void);
uint8_t battery_get_percent(void);
uint32_t battery_get_mv(void);
uint8_t battery_mv_to_percent(uint32_t mv, uint32_t empty_mv, uint32_t full_mv);
