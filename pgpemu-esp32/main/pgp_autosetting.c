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
            ESP_LOGD(
                SETTING_TASK_TAG, "[%d] toggling setting %c after delay=%d ms", item.conn_id, item.setting, item.delay);
            vTaskDelay(item.delay / portTICK_PERIOD_MS);

            // Get device settings for this connection
            client_state_t* entry = get_client_state_entry(item.conn_id);
            if (!entry || !entry->settings) {
                ESP_LOGW(SETTING_TASK_TAG, "[%d] device disconnected before toggle", item.conn_id);
                continue;
            }

            switch (item.setting) {
            case 's':
                if (!entry->settings->autospin) {
                    if (!toggle_device_autospin(item.conn_id)) {
                        ESP_LOGW(SETTING_TASK_TAG, "[%d] failed to toggle autospin", item.conn_id);
                        break;
                    }
                    if (!write_devices_settings_to_nvs()) {
                        ESP_LOGW(SETTING_TASK_TAG, "[%d] failed to write device settings to NVS", item.conn_id);
                    }
                }
                break;
            case 'c':
                if (!entry->settings->autocatch) {
                    if (!toggle_device_autocatch(item.conn_id)) {
                        ESP_LOGW(SETTING_TASK_TAG, "[%d] failed to toggle autocatch", item.conn_id);
                        break;
                    }
                    if (!write_devices_settings_to_nvs()) {
                        ESP_LOGW(SETTING_TASK_TAG, "[%d] failed to write device settings to NVS", item.conn_id);
                    }
                }
                break;
            default:
                ESP_LOGW(SETTING_TASK_TAG, "[%d] unhandled toggle case: %c", item.conn_id, item.setting);
            }
        }
    }

    vTaskDelete(NULL);
}
