#include "pgp_autosetting.h"

#include "config_storage.h"
#include "esp_bt.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "log_tags.h"
#include "pgp_gatts.h"
#include "pgp_handshake_multi.h"
#include "settings.h"

QueueHandle_t setting_queue;

static void autosetting_task(void* pvParameters);
static void retoggle_timer_callback(TimerHandle_t xTimer);
static void execute_retoggle(uint32_t session_id, char setting);

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

static void retoggle_timer_callback(TimerHandle_t xTimer) {
    retoggle_timer_data_t* timer_data = (retoggle_timer_data_t*)pvTimerGetTimerID(xTimer);

    if (timer_data) {
        ESP_LOGI(SETTING_TASK_TAG,
            "Timer callback: executing retoggle %c (session=%lu)",
            timer_data->setting,
            (unsigned long)timer_data->session_id);

        execute_retoggle(timer_data->session_id, timer_data->setting);
        free(timer_data);
    }

    xTimerDelete(xTimer, 0);
}

static void execute_retoggle(uint32_t session_id, char setting) {
    switch (setting) {
    case 's':
        if (!toggle_device_autospin_by_session(session_id)) {
            ESP_LOGW(SETTING_TASK_TAG, "failed to toggle autospin (session=%lu)", (unsigned long)session_id);
        }
        break;

    case 'c':
        if (!toggle_device_autocatch_by_session(session_id)) {
            ESP_LOGW(SETTING_TASK_TAG, "failed to toggle autocatch (session=%lu)", (unsigned long)session_id);
        }
        break;

    default:
        ESP_LOGW(SETTING_TASK_TAG, "unhandled toggle case: %c (session=%lu)", setting, (unsigned long)session_id);
    }
}

static void autosetting_task(void* __attribute__((unused)) pvParameters) {
    setting_queue_item_t item;

    ESP_LOGI(SETTING_TASK_TAG, "task start");

    while (1) {
        if (xQueueReceive(setting_queue, &item, portMAX_DELAY)) {
            ESP_LOGI(SETTING_TASK_TAG,
                "[%d] received retoggle %c, delay=%d ms (session=%lu)",
                item.conn_id,
                item.setting,
                item.delay,
                (unsigned long)item.session_id);

            if (item.delay > 0) {
                retoggle_timer_data_t* timer_data = malloc(sizeof(retoggle_timer_data_t));
                if (!timer_data) {
                    ESP_LOGE(SETTING_TASK_TAG, "malloc failed for timer data");
                    continue;
                }

                timer_data->session_id = item.session_id;
                timer_data->setting = item.setting;

                TimerHandle_t timer = xTimerCreate(
                    "retoggle", pdMS_TO_TICKS(item.delay), pdFALSE, (void*)timer_data, retoggle_timer_callback);

                if (timer) {
                    if (xTimerStart(timer, 0) == pdPASS) {
                        ESP_LOGI(SETTING_TASK_TAG,
                            "[%d] scheduled retoggle %c in %d ms (session=%lu)",
                            item.conn_id,
                            item.setting,
                            item.delay,
                            (unsigned long)item.session_id);
                    } else {
                        ESP_LOGE(SETTING_TASK_TAG, "xTimerStart failed");
                        xTimerDelete(timer, 0);
                        free(timer_data);
                    }
                } else {
                    ESP_LOGE(SETTING_TASK_TAG, "xTimerCreate failed");
                    free(timer_data);
                }
            } else {
                execute_retoggle(item.session_id, item.setting);
            }
        }
    }

    vTaskDelete(NULL);
}
