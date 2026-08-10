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
    .advertising_enabled = true,
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

// Flips *var under mutex, with a 10s timeout. Writes the post-toggle value to
// *new_value_out (if given) while still holding the lock, since reading *var
// after release would race another toggle.
static bool toggle_bool_locked(SemaphoreHandle_t mutex, bool* var, bool* new_value_out) {
    if (!var || !mutex_acquire_timeout(mutex, 10000)) {
        return false;
    }

    *var = !*var;
    if (new_value_out) {
        *new_value_out = *var;
    }

    mutex_release(mutex);
    return true;
}

bool toggle_setting(bool* var) {
    return toggle_bool_locked(global_settings.mutex, var, NULL);
}

bool toggle_device_autospin(uint8_t c) {
    client_state_t* entry = get_client_state_entry_by_idx(c);
    if (entry == NULL || entry->settings == NULL) {
        return false;
    }

    bool new_value = false;
    toggle_bool_locked(entry->settings->mutex, &entry->settings->autospin, &new_value);
    return new_value;
}

bool toggle_device_autocatch(uint8_t c) {
    client_state_t* entry = get_client_state_entry_by_idx(c);
    if (entry == NULL || entry->settings == NULL) {
        return false;
    }

    bool new_value = false;
    toggle_bool_locked(entry->settings->mutex, &entry->settings->autocatch, &new_value);
    return new_value;
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
