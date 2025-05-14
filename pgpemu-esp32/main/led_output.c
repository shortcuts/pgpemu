#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "led_output.h"

#include "log_tags.h"
#include "settings.h"

typedef struct
{
    int red, green, blue;
    int duration_ms;
} LedEvent;

#if   CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
    static const int CONFIG_GPIO_OUTPUT_RED   = GPIO_NUM_27;
    static const int CONFIG_GPIO_OUTPUT_GREEN = GPIO_NUM_26;
    static const int CONFIG_GPIO_OUTPUT_BLUE  = GPIO_NUM_25;

#elif CONFIG_IDF_TARGET_ESP32C3
    /* Pick any three free C3 pins; these are just an example. */
    static const int CONFIG_GPIO_OUTPUT_RED   = GPIO_NUM_21;
    static const int CONFIG_GPIO_OUTPUT_GREEN = GPIO_NUM_20;
    static const int CONFIG_GPIO_OUTPUT_BLUE  = GPIO_NUM_19;

#else
    #error "Unsupported target: please define RGB-LED GPIOs for this chip"
#endif

static void leds_off();
static void led_output_task(void *pvParameters);

static QueueHandle_t led_queue;

static bool led_ready = false;

void init_led_output()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << CONFIG_GPIO_OUTPUT_RED) | (1 << CONFIG_GPIO_OUTPUT_GREEN) | (1 << CONFIG_GPIO_OUTPUT_BLUE);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    leds_off();

    led_queue = xQueueCreate(10, sizeof(LedEvent));
    led_ready = true;
    xTaskCreate(led_output_task, "led_output_task", 2048, NULL, 14, NULL);
}

void show_rgb_event(bool red, bool green, bool blue, int duration_ms)
{
    if (!led_ready)
    {
        return;
    }

    LedEvent led_event = {
        .red = 1 - red,
        .green = 1 - green,
        .blue = 1 - blue,
        .duration_ms = duration_ms,
    };
    ESP_LOGD(LEDOUTPUT_TAG, "adding to queue rgb=(%d, %d, %d)", red, green, blue);
    xQueueSend(led_queue, &led_event, 0);
}

static void leds_off()
{
    ESP_LOGD(LEDOUTPUT_TAG, "leds off");
    gpio_set_level(CONFIG_GPIO_OUTPUT_RED, 1);
    gpio_set_level(CONFIG_GPIO_OUTPUT_GREEN, 1);
    gpio_set_level(CONFIG_GPIO_OUTPUT_BLUE, 1);
}

static void led_output_task(void *pvParameters)
{
    LedEvent led_event;

    while (true)
    {
        if (xQueueReceive(led_queue, &led_event, portMAX_DELAY))
        {
            ESP_LOGD(LEDOUTPUT_TAG, "setting rgb=(%d, %d, %d)",
                     led_event.red, led_event.green, led_event.blue);
            gpio_set_level(CONFIG_GPIO_OUTPUT_RED, led_event.red);
            gpio_set_level(CONFIG_GPIO_OUTPUT_GREEN, led_event.green);
            gpio_set_level(CONFIG_GPIO_OUTPUT_BLUE, led_event.blue);

            if (led_event.duration_ms > 0)
            {
                vTaskDelay(pdMS_TO_TICKS(led_event.duration_ms));
                leds_off();
            }
        }
    }

    vTaskDelete(NULL);
}
