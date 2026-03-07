#include "led_output.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "log_tags.h"

static const int GPIO_LED_ADVERTISING = GPIO_NUM_8;
static bool led_state = false;

static const char* LED_OUTPUT_TAG = "led_output";

void init_led_output(void) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << GPIO_LED_ADVERTISING);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&io_conf);
    gpio_set_level(GPIO_LED_ADVERTISING, 0);
    led_state = false;

    ESP_LOGI(LED_OUTPUT_TAG, "initialized LED on GPIO %d", GPIO_LED_ADVERTISING);
}

void set_led_advertising(bool enabled) {
    gpio_set_level(GPIO_LED_ADVERTISING, enabled ? 1 : 0);
    led_state = enabled;
}

bool get_led_advertising(void) {
    return led_state;
}
