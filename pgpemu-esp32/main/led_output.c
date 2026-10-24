#include "led_output.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "log_tags.h"
#include "pgp_gap.h"
#include "pgp_handshake_multi.h"
#include "pgp_led_pattern.h"

// GPIO 0-5 are strapping pins on ESP32-C3 (boot mode/JTAG control).
// GPIO 2 specifically is MTDI and cannot be used. GPIO 8 is valid and safe.
static const int GPIO_LED_ADVERTISING = GPIO_NUM_8;
#define LED_TICK_MS 50

static bool led_state = false;
// A plain flag: get_setting() blocks on the settings mutex until boot ends, which would stall the timer.
static volatile bool led_enabled = true;
static bool provisioning_mode = false;
static bool crashed_boot = false;

static const char* LED_OUTPUT_TAG = "led_output";

static void led_tick(void* arg) {
    (void)arg;
    static led_pattern_t pattern = LED_PAT_OFF;
    static uint8_t shown_connections = 0;
    static uint32_t cycle_ms = 0;
    static uint32_t uptime_ms = 0;

    const int count = get_active_connections();
    const uint8_t connections = count > UINT8_MAX ? UINT8_MAX : (uint8_t)count;
    const led_pattern_t next = led_pattern_select(led_enabled,
        provisioning_mode,
        crashed_boot && uptime_ms < LED_CRASH_WINDOW_MS,
        pgp_adv_get_state(),
        connections);
    const uint8_t next_connections = connections > 4 ? 4 : connections;
    if (next != pattern || (next == LED_PAT_CONNECTED && next_connections != shown_connections)) {
        pattern = next;
        shown_connections = next_connections;
        cycle_ms = 0;
    }

    const bool level = led_pattern_level(pattern, shown_connections, cycle_ms);
    if (level != led_state) {
        gpio_set_level(GPIO_LED_ADVERTISING, level ? 1 : 0);
        led_state = level;
    }
    cycle_ms += LED_TICK_MS;
    uptime_ms += LED_TICK_MS;
}

void init_led_output(bool provisioning, bool crashed) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << GPIO_LED_ADVERTISING);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&io_conf);
    gpio_set_level(GPIO_LED_ADVERTISING, 0);
    led_state = false;
    provisioning_mode = provisioning;
    crashed_boot = crashed;

    esp_timer_handle_t timer = NULL;
    esp_timer_create_args_t args = { .callback = led_tick, .name = "led_tick" };
    if (esp_timer_create(&args, &timer) != ESP_OK || esp_timer_start_periodic(timer, LED_TICK_MS * 1000) != ESP_OK) {
        ESP_LOGE(LED_OUTPUT_TAG, "LED timer failed");
        return;
    }

    ESP_LOGI(LED_OUTPUT_TAG, "initialized LED on GPIO %d", GPIO_LED_ADVERTISING);
}

void led_output_set_enabled(bool enabled) {
    led_enabled = enabled;
}

bool get_led_advertising(void) {
    return led_state;
}
