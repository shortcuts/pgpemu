#include "pgp_autosetting.h"

#include "esp_bt.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "pgp_gatts.h"
#include "settings.h"

QueueHandle_t setting_queue;

static void autosetting_task(void* pvParameters);

bool init_autosetting() {
    setting_queue = xQueueCreate(10, sizeof(setting_queue_item_t));
    if (!setting_queue) {
        ESP_LOGE(SETTING_TASK_TAG, "%s creating setting queue failed", __func__);
        return false;
    }

    BaseType_t ret = xTaskCreate(autosetting_task, "autosetting_task", 3072, NULL, 11, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(SETTING_TASK_TAG, "%s creating task failed", __func__);
        vQueueDelete(setting_queue);
        return false;
    }

    return true;
}

static void autosetting_task(void* pvParameters) {
    setting_queue_item_t item;

    ESP_LOGI(SETTING_TASK_TAG, "task start");

    while (1) {
        if (xQueueReceive(setting_queue, &item, portMAX_DELAY)) {
            ESP_LOGD(
                SETTING_TASK_TAG, "[%d] toggling setting %c after delay=%d ms", item.conn_id, item.setting, item.delay);
            vTaskDelay(item.delay / portTICK_PERIOD_MS);

            switch (item.setting) {
            case 's':
                if (!get_setting(&settings.autospin)) {
                    toggle_setting(&settings.autospin);
                }
                break;
            case 'c':
                if (!get_setting(&settings.autocatch)) {
                    toggle_setting(&settings.autocatch);
                }
                break;
            default:
                ESP_LOGW(SETTING_TASK_TAG, "[%d] unhandled toggle case: %c", item.conn_id, item.setting);
            }
        }
    }

    vTaskDelete(NULL);
}
