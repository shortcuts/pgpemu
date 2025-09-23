#include "settings.h"

#include "config_secrets.h"
#include "esp_log.h"
#include "log_tags.h"

#include <stdint.h>

// runtime settings
Settings settings = {
    .mutex = NULL,
    .chosen_device = 0,
    .target_active_connections = 1,
    .autocatch = true,
    .autospin = true,
    .use_button = false,
    .log_level = 1,
};

void init_settings() {
    settings.mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(settings.mutex, portMAX_DELAY);  // block until end of this function
}

QueueHandle_t settings_queue;

static void autosettings_task(void* pvParameters);

bool init_autosetting() {
    settings_queue = xQueueCreate(10, sizeof(settings_queue_item_t));
    if (!settings_queue) {
        ESP_LOGE(SETTINGS_TAG, "%s creating settings queue failed", __func__);
        return false;
    }

    BaseType_t ret = xTaskCreate(autosettings_task, "autosettings_task", 3072, NULL, 11, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(SETTINGS_TAG, "%s creating task failed", __func__);
        vQueueDelete(settings_queue);
        return false;
    }

    return true;
}

static void autosettings_task(void* pvParameters) {
    settings_queue_item_t item;

    ESP_LOGI(SETTINGS_TAG, "task start");

    while (1) {
        if (xQueueReceive(settings_queue, &item, portMAX_DELAY)) {
            ESP_LOGD(
                SETTINGS_TAG, "[%d] toggling setting after delay=%d ms", item.conn_id, item.delay);
            vTaskDelay(item.delay / portTICK_PERIOD_MS);

            if (!get_setting(&settings.autospin)) {
                toggle_setting(&settings.autospin);
            }
        }
    }

    vTaskDelete(NULL);
}

void settings_ready() {
    xSemaphoreGive(settings.mutex);
}

bool cycle_setting(uint8_t* var, uint8_t min, uint8_t max) {
    if (!var || !min || !max || !xSemaphoreTake(settings.mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    (*var)++;

    if (*var > max) {
        *var = min;
    }

    xSemaphoreGive(settings.mutex);
    return true;
}

bool toggle_setting(bool* var) {
    if (!var || !xSemaphoreTake(settings.mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    *var = !*var;

    xSemaphoreGive(settings.mutex);
    return true;
}

bool get_setting(bool* var) {
    if (!var || !xSemaphoreTake(settings.mutex, portMAX_DELAY)) {
        return false;
    }

    bool result = *var;

    xSemaphoreGive(settings.mutex);
    return result;
}

char* get_setting_log_value(bool* var) {
    return get_setting(var) ? "on" : "off";
}

uint8_t get_setting_uint8(uint8_t* var) {
    if (!var || !xSemaphoreTake(settings.mutex, portMAX_DELAY)) {
        return 0;
    }

    uint8_t result = *var;

    xSemaphoreGive(settings.mutex);
    return result;
}

bool set_setting_uint8(uint8_t* var, const uint8_t val) {
    if (!var || !xSemaphoreTake(settings.mutex, portMAX_DELAY)) {
        return false;
    }

    *var = val;

    xSemaphoreGive(settings.mutex);
    return true;
}

bool set_chosen_device(uint8_t id) {
    if (!is_valid_secrets_id(id)) {
        return false;
    }
    if (!xSemaphoreTake(settings.mutex, portMAX_DELAY)) {
        return false;
    }

    settings.chosen_device = id;

    xSemaphoreGive(settings.mutex);
    return true;
}
