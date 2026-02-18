#include "settings.h"

#include "config_secrets.h"
#include "esp_log.h"
#include "esp_random.h"
#include "log_tags.h"
#include "mutex_helpers.h"
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
    if (!var || !mutex_acquire_timeout(global_settings.mutex, 10000)) {
        return false;
    }

    (*var)++;

    if (*var > 3) {
        *var = 1;
    }

    mutex_release(global_settings.mutex);
    return true;
}

bool toggle_setting(bool* var) {
    if (!var || !mutex_acquire_timeout(global_settings.mutex, 10000)) {
        return false;
    }

    *var = !*var;

    mutex_release(global_settings.mutex);

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
