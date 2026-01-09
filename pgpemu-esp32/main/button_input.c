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

static void button_input_task(void* pvParameters);
static QueueHandle_t button_input_queue;

int get_button_gpio() {
    return CONFIG_GPIO_INPUT_BUTTON0;
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(button_input_queue, &gpio_num, NULL);
}

void init_button_input() {
    // create a queue to handle gpio event from isr
    // use size 1 to drop further button events while a press is being handled in the task
    button_input_queue = xQueueCreate(1, sizeof(uint32_t));

    gpio_config_t io_conf = {};
    // interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // bit mask of the pins
    io_conf.pin_bit_mask = (1 << CONFIG_GPIO_INPUT_BUTTON0);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // install gpio isr service
    gpio_install_isr_service(0);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(CONFIG_GPIO_INPUT_BUTTON0, gpio_isr_handler, (void*)CONFIG_GPIO_INPUT_BUTTON0);

    // start gpio task
    xTaskCreate(button_input_task, "button_input", 2048, NULL, 15, NULL);
}

static void button_input_task(void* pvParameters) {
    uint32_t button_event;

    ESP_LOGI(BUTTON_INPUT_TAG, "task start");

    while (true) {
        if (xQueueReceive(button_input_queue, &button_event, portMAX_DELAY)) {
            ESP_LOGV(BUTTON_INPUT_TAG, "button0 down");

            // debounce
            vTaskDelay(200 / portTICK_PERIOD_MS);
            if (gpio_get_level(CONFIG_GPIO_INPUT_BUTTON0) != 0) {
                // not pressed anymore
                continue;
            }

            ESP_LOGD(BUTTON_INPUT_TAG, "button0 pressed");

            int target_active_connections = get_setting_uint8(&global_settings.target_active_connections);
            int active_connections = get_active_connections();
            if (active_connections < target_active_connections) {
                // target connections not reached, so we should be advertising currently
                ESP_LOGI(BUTTON_INPUT_TAG, "button -> don't advertise");
                pgp_advertise_stop();
            } else if (active_connections + 1 <= CONFIG_BT_ACL_CONNECTIONS) {
                // target connections reached but more connections still possible
                ESP_LOGI(BUTTON_INPUT_TAG, "button -> advertise");
                pgp_advertise();
            } else {
                ESP_LOGW(BUTTON_INPUT_TAG, "button -> max. BT connections reached");
            }
        }
    }

    vTaskDelete(NULL);
}
