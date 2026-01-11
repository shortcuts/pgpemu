#include "pgp_autosetting.h"

#include "config_storage.h"
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
#include "pgp_handshake_multi.h"
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
            ESP_LOGD(SETTING_TASK_TAG,
                "[%d] toggling setting %c after delay=%d ms (session=%lu)",
                item.conn_id,
                item.setting,
                item.delay,
                (unsigned long)item.session_id);
            vTaskDelay(item.delay / portTICK_PERIOD_MS);

            switch (item.setting) {
            case 's':
                // Use session_id-based toggle, NO NVS write
                if (!toggle_device_autospin_by_session(item.session_id)) {
                    ESP_LOGW(
                        SETTING_TASK_TAG, "failed to toggle autospin (session=%lu)", (unsigned long)item.session_id);
                }
                break;

            case 'c':
                // Use session_id-based toggle, NO NVS write
                if (!toggle_device_autocatch_by_session(item.session_id)) {
                    ESP_LOGW(
                        SETTING_TASK_TAG, "failed to toggle autocatch (session=%lu)", (unsigned long)item.session_id);
                }
                break;

            default:
                ESP_LOGW(SETTING_TASK_TAG,
                    "unhandled toggle case: %c (session=%lu)",
                    item.setting,
                    (unsigned long)item.session_id);
            }
        }
    }

    vTaskDelete(NULL);
}
