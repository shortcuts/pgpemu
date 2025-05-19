#pragma once
#include "sdkconfig.h"
#include "driver/gpio.h"

void factory_reset_and_reboot(void);
bool setup_button_pressed_on_boot(void);
