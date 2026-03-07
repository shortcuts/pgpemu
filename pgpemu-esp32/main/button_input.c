#include "button_input.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "pgp_gap.h"
#include "pgp_handshake_multi.h"
#include "settings.h"

static const int CONFIG_GPIO_INPUT_BUTTON0 = GPIO_NUM_3;
static const int CONFIG_GPIO_INPUT_BUTTON1 = GPIO_NUM_9;

typedef enum {
    BUTTON_EVENT_BUTTON0 = 0,
    BUTTON_EVENT_BUTTON1 = 1,
} button_event_type_t;

typedef struct {
    button_event_type_t type;
    uint32_t gpio_num;
} button_event_t;

static void button_input_task(void* pvParameters);
static QueueHandle_t button_input_queue;

int get_button_gpio() {
    return CONFIG_GPIO_INPUT_BUTTON0;
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    button_event_t event = { 0 };

    if (gpio_num == CONFIG_GPIO_INPUT_BUTTON0) {
        event.type = BUTTON_EVENT_BUTTON0;
    } else if (gpio_num == CONFIG_GPIO_INPUT_BUTTON1) {
        event.type = BUTTON_EVENT_BUTTON1;
    }
    event.gpio_num = gpio_num;

    xQueueSendFromISR(button_input_queue, &event, NULL);
}

void init_button_input() {
    button_input_queue = xQueueCreate(1, sizeof(button_event_t));

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1 << CONFIG_GPIO_INPUT_BUTTON0) | (1 << CONFIG_GPIO_INPUT_BUTTON1);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_GPIO_INPUT_BUTTON0, gpio_isr_handler, (void*)CONFIG_GPIO_INPUT_BUTTON0);
    gpio_isr_handler_add(CONFIG_GPIO_INPUT_BUTTON1, gpio_isr_handler, (void*)CONFIG_GPIO_INPUT_BUTTON1);

    xTaskCreate(button_input_task, "button_input", 2048, NULL, 15, NULL);
}

static void button_input_task(void* pvParameters) {
    button_event_t button_event;

    ESP_LOGI(BUTTON_INPUT_TAG, "task start");

    while (true) {
        if (xQueueReceive(button_input_queue, &button_event, portMAX_DELAY)) {
            if (button_event.type == BUTTON_EVENT_BUTTON0) {
                ESP_LOGV(BUTTON_INPUT_TAG, "button0 down");

                vTaskDelay(200 / portTICK_PERIOD_MS);
                if (gpio_get_level(CONFIG_GPIO_INPUT_BUTTON0) != 0) {
                    continue;
                }

                ESP_LOGD(BUTTON_INPUT_TAG, "button0 pressed");

                int target_active_connections = get_setting_uint8(&global_settings.target_active_connections);
                int active_connections = get_active_connections();
                if (active_connections < target_active_connections) {
                    ESP_LOGI(BUTTON_INPUT_TAG, "button0 -> don't advertise");
                    pgp_advertise_stop();
                } else if (active_connections + 1 <= CONFIG_BT_ACL_CONNECTIONS) {
                    ESP_LOGI(BUTTON_INPUT_TAG, "button0 -> advertise");
                    pgp_advertise();
                } else {
                    ESP_LOGW(BUTTON_INPUT_TAG, "button0 -> max. BT connections reached");
                }
            } else if (button_event.type == BUTTON_EVENT_BUTTON1) {
                ESP_LOGV(BUTTON_INPUT_TAG, "button1 down");

                vTaskDelay(200 / portTICK_PERIOD_MS);
                if (gpio_get_level(CONFIG_GPIO_INPUT_BUTTON1) != 0) {
                    continue;
                }

                ESP_LOGD(BUTTON_INPUT_TAG, "button1 pressed");

                if (!toggle_setting(&global_settings.advertising_enabled)) {
                    ESP_LOGE(BUTTON_INPUT_TAG, "failed to toggle advertising");
                    continue;
                }

                bool adv_enabled = get_setting(&global_settings.advertising_enabled);
                if (adv_enabled) {
                    ESP_LOGI(BUTTON_INPUT_TAG, "button1 -> advertising enabled");
                    pgp_advertise();
                } else {
                    ESP_LOGI(BUTTON_INPUT_TAG, "button1 -> advertising disabled");
                    pgp_advertise_stop();
                }
            }
        }
    }

    vTaskDelete(NULL);
}
