#include "powerbank.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "settings.h"
#include "wifi.h"

static void powerbank_task(void* pvParameters);

void init_powerbank() { xTaskCreate(powerbank_task, "powerbank_task", 4096, NULL, 10, NULL); }

static void powerbank_task(void* pvParameters) {
    TickType_t previousWakeTime = xTaskGetTickCount();

    ESP_LOGI(POWERBANK_TASK_TAG, "task start");

    // to avoid spamming early boot log
    vTaskDelayUntil(&previousWakeTime, 2000 / portTICK_PERIOD_MS);

    bool wifi_inited = false;
    bool ok;
    while (1) {
        if (get_setting(&settings.powerbank_ping)) {
            if (!wifi_inited) {
                wifi_inited = true;
                init_wifi();
            }

            ok = wifi_start();
            ESP_LOGI(POWERBANK_TASK_TAG, "on: %d", ok);

            vTaskDelay(1000 / portTICK_PERIOD_MS);

            ok = wifi_stop();
            ESP_LOGI(POWERBANK_TASK_TAG, "off: %d", ok);
        }

        vTaskDelayUntil(&previousWakeTime, 30000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}