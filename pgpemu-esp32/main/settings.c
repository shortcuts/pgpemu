#include "settings.h"

#include "config_secrets.h"
#include "esp_log.h"
#include "log_tags.h"
#include "pgp_handshake_multi.h"

#include <stdint.h>

// runtime settings
GlobalSettings global_settings = {
    .mutex = NULL,
    .target_active_connections = 1,
    .log_level = 1,
};

void init_global_settings() {
    global_settings.mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(global_settings.mutex, portMAX_DELAY);  // block until end of this function
}

void global_settings_ready() {
    xSemaphoreGive(global_settings.mutex);
}

bool cycle_log_level(uint8_t* var) {
    if (!var || !xSemaphoreTake(global_settings.mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    (*var)++;

    if (*var > 3) {
        *var = 1;
    }

    xSemaphoreGive(global_settings.mutex);
    return true;
}

bool toggle_setting(bool* var) {
    if (!var || !xSemaphoreTake(global_settings.mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    *var = !*var;

    xSemaphoreGive(global_settings.mutex);

    return true;
}

bool toggle_device_autospin(uint8_t c) {
    client_state_t* entry = get_client_state_entry_by_idx(c);

    if (entry == NULL || entry->settings == NULL) {
        return false;
    }

    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    entry->settings->autospin = !entry->settings->autospin;

    xSemaphoreGive(entry->settings->mutex);

    return entry->settings->autospin;
}

bool toggle_device_autocatch(uint8_t c) {
    client_state_t* entry = get_client_state_entry_by_idx(c);

    if (entry == NULL || entry->settings == NULL) {
        return false;
    }

    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    entry->settings->autocatch = !entry->settings->autocatch;

    xSemaphoreGive(entry->settings->mutex);

    return entry->settings->autocatch;
}

uint8_t set_device_autospin_probability(uint8_t c, uint8_t autospin_probability) {
    client_state_t* entry = get_client_state_entry_by_idx(c);

    if (entry == NULL || entry->settings == NULL) {
        return 0;
    }

    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return 0;
    }

    if (autospin_probability > 9) {
        ESP_LOGW(SETTING_TASK_TAG,
            "[%d] invalid autospin probability: %d (0-9 allowed)",
            entry->conn_id,
            autospin_probability);
        xSemaphoreGive(entry->settings->mutex);
        return entry->settings->autospin_probability;
    }

    entry->settings->autospin_probability = autospin_probability;

    xSemaphoreGive(entry->settings->mutex);

    return entry->settings->autospin_probability;
}

bool get_setting(bool* var) {
    if (!var || !xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return false;
    }

    bool result = *var;

    xSemaphoreGive(global_settings.mutex);
    return result;
}

char* get_setting_log_value(bool* var) {
    return get_setting(var) ? "on" : "off";
}

uint8_t get_setting_uint8(uint8_t* var) {
    if (!var || !xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return 0;
    }

    uint8_t result = *var;

    xSemaphoreGive(global_settings.mutex);
    return result;
}

bool set_setting_uint8(uint8_t* var, const uint8_t val) {
    if (!var || !xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return false;
    }

    *var = val;

    xSemaphoreGive(global_settings.mutex);
    return true;
}
