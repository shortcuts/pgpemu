#include "rom/ets_sys.h"
#include "esp_log.h"
#include "esp_system.h"

#include "setup_button.h"
#include "button_input.h"
#include "log_tags.h"
#include "nvs_flash.h"

void factory_reset_and_reboot(void) 
{
    // this will erase the NVS partition only, then re-init it
    nvs_flash_init();
    nvs_flash_erase();
    nvs_flash_init();
    esp_restart();
}

bool setup_button_pressed_on_boot(void)
{
    /* use the same GPIO the regular button-task will use later */
    const gpio_num_t BTN = get_button_gpio();

    /* configure temporarily as a plain input with pull-up */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BTN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    ets_delay_us(30);                         // let pull-up settle
    bool pressed = (gpio_get_level(BTN) == 0); /* active-low */
    ESP_LOGI(UART_TAG, "Setup Button: %s", pressed ? "pressed" : "not pressed");
    
    return pressed;
}