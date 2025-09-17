#pragma once
#include "driver/gpio.h"
#include "sdkconfig.h"

void factory_reset_and_reboot(void);
bool setup_button_pressed_on_boot(void);
