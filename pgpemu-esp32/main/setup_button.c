#include "setup_button.h"

#include "button_input.h"
#include "esp_log.h"
#include "log_tags.h"
#include "rom/ets_sys.h"

bool setup_button_pressed_on_boot(void) {
    /* use GPIO 9 (boot button) - same as button_input.c uses */
    const gpio_num_t BTN = get_button_gpio();

    /* configure temporarily as a plain input with pull-up */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BTN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    ets_delay_us(30);                          // let pull-up settle
    bool pressed = (gpio_get_level(BTN) == 0); /* active-low */
    ESP_LOGI(UART_TAG, "Setup Button: %s", pressed ? "pressed" : "not pressed");

    return pressed;
}