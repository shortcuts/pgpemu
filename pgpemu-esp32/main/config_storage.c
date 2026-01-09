#include "config_storage.h"

#include "config_secrets.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "log_tags.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_helper.h"
#include "pgp_handshake_multi.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>

// global settings keys
static const char KEY_CONNECTION_COUNT[] = "maxcon";
static const char KEY_LOG_LEVEL[] = "llevel";

// device settings keys
static const char KEY_AUTOCATCH[] = "catch";
static const char KEY_AUTOSPIN[] = "spin";
static const char KEY_AUTOSPIN_PROBABILITY[] = "spinp";

// Forward declaration
char* make_device_key_for_option(const char* key, const esp_bd_addr_t bda, char* out);

void init_settings_nvs_partition() {
    esp_err_t err;

    ESP_LOGD(CONFIG_STORAGE_TAG, "initializing config storage");

    // initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "NVS err, erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void read_stored_global_settings(bool use_mutex) {
    uint8_t log_level = 0;
    uint8_t connection_count = 0;

    if (use_mutex) {
        if (!xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get global settings mutex");
            return;
        }
    }

    nvs_handle_t global_settings_handle = {};
    esp_err_t err = nvs_open("global_settings", NVS_READONLY, &global_settings_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(CONFIG_STORAGE_TAG, "global settings partition doesn't exist, using default settings");
        } else {
            ESP_ERROR_CHECK(err);  // panic
        }

        if (use_mutex) {
            xSemaphoreGive(global_settings.mutex);
        }
        return;
    }

    err = nvs_get_u8(global_settings_handle, KEY_LOG_LEVEL, &log_level);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_LOG_LEVEL)) {
        global_settings.log_level = log_level;
    }
    err = nvs_get_u8(global_settings_handle, KEY_CONNECTION_COUNT, &connection_count);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_CONNECTION_COUNT)) {
        if (connection_count <= CONFIG_BT_ACL_CONNECTIONS && connection_count > 0) {
            global_settings.target_active_connections = connection_count;
        } else {
            ESP_LOGE(CONFIG_STORAGE_TAG,
                "invalid target active connections: %d (1-%d allowed)",
                connection_count,
                CONFIG_BT_ACL_CONNECTIONS);
        }
    }

    nvs_close(global_settings_handle);

    if (use_mutex) {
        xSemaphoreGive(global_settings.mutex);
    }

    ESP_LOGI(CONFIG_STORAGE_TAG, "global settings read from nvs");
}

static bool is_valid_bda(const esp_bd_addr_t bda) {
    if (!bda) {
        return false;
    }
    // Check that at least one byte is non-zero
    for (int i = 0; i < 6; i++) {
        if (bda[i] != 0) {
            return true;
        }
    }
    return false;
}

bool read_stored_device_settings(esp_bd_addr_t bda, DeviceSettings* out_settings) {
    if (!out_settings) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "read_stored_device_settings: out_settings is NULL");
        return false;
    }

    if (!is_valid_bda(bda)) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "read_stored_device_settings: invalid bda");
        return false;
    }

    // Initialize default values
    int8_t autocatch = 0;
    int8_t autospin = 0;
    uint8_t autospin_probability = 0;

    // Initialize retoggle fields
    out_settings->autospin_retoggle_pending = false;
    out_settings->autocatch_retoggle_pending = false;
    out_settings->autospin_retoggle_time = 0;
    out_settings->autocatch_retoggle_time = 0;

    // Set defaults
    out_settings->autocatch = 1;
    out_settings->autospin = 1;
    out_settings->autospin_probability = 0;
    memcpy(out_settings->bda, bda, sizeof(esp_bd_addr_t));

    // Create mutex if not already created
    if (out_settings->mutex == NULL) {
        out_settings->mutex = xSemaphoreCreateMutex();
        if (!out_settings->mutex) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "failed to create mutex for device settings");
            return false;
        }
    }

    // Take mutex to protect read
    if (!xSemaphoreTake(out_settings->mutex, portMAX_DELAY)) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get device_settings mutex");
        return false;
    }

    // open config partition
    nvs_handle_t device_settings_handle = {};
    esp_err_t err = nvs_open("device_settings", NVS_READONLY, &device_settings_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(CONFIG_STORAGE_TAG, "device_settings partition doesn't exist, using default settings");
        } else {
            ESP_LOGE(CONFIG_STORAGE_TAG, "failed to open device_settings partition: %s", esp_err_to_name(err));
        }

        xSemaphoreGive(out_settings->mutex);
        return false;  // Return false on error, but settings still has defaults
    }

    // read bool settings
    char key_out[16];

    make_device_key_for_option(KEY_AUTOCATCH, bda, key_out);
    err = nvs_get_i8(device_settings_handle, key_out, &autocatch);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_AUTOCATCH)) {
        out_settings->autocatch = (bool)autocatch;
    }

    make_device_key_for_option(KEY_AUTOSPIN, bda, key_out);
    err = nvs_get_i8(device_settings_handle, key_out, &autospin);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_AUTOSPIN)) {
        out_settings->autospin = (bool)autospin;
    }

    // read uint8_t settings
    make_device_key_for_option(KEY_AUTOSPIN_PROBABILITY, bda, key_out);
    err = nvs_get_u8(device_settings_handle, key_out, &autospin_probability);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_AUTOSPIN_PROBABILITY)) {
        if (autospin_probability > 9) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "invalid autospin probability: %d (0-9 allowed), using default 0", autospin_probability);
            out_settings->autospin_probability = 0;  // Set valid default instead of leaving uninitialized
        } else {
            out_settings->autospin_probability = autospin_probability;
        }
    }

    nvs_close(device_settings_handle);

    xSemaphoreGive(out_settings->mutex);

    ESP_LOGI(CONFIG_STORAGE_TAG, "device_settings read from nvs");
    return true;
}

bool write_global_settings_to_nvs() {
    if (!xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get global_settings mutex");
        return false;
    }

    nvs_handle_t global_settings_handle = {};
    esp_err_t err = nvs_open("global_settings", NVS_READWRITE, &global_settings_handle);
    ESP_ERROR_CHECK(err);

    bool all_ok = true;

    err = nvs_set_u8(global_settings_handle, KEY_LOG_LEVEL, global_settings.log_level);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_LOG_LEVEL);
    err = nvs_set_u8(global_settings_handle, KEY_CONNECTION_COUNT, global_settings.target_active_connections);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_CONNECTION_COUNT);

    // give it back in any of the following cases
    xSemaphoreGive(global_settings.mutex);

    err = nvs_commit(global_settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "commit failed");
        nvs_close(global_settings_handle);
        return false;
    }

    nvs_close(global_settings_handle);
    return all_ok;
}

// concatenates the given two strings and hash them so it fits in the nvs key space (15 char).
char* make_device_key_for_option(const char* key, const esp_bd_addr_t bda, char* out) {
    // 1. Concatenate safely into a temp buffer
    char buf[64];
    int len =
        snprintf(buf, sizeof(buf), "%s_%02x%02x%02x%02x%02x%02x", key, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    if (len == 0) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "unable to concatenate key and bda for %s", key);
        return out;
    }

    // 2. FNV-1a hash
    uint64_t hash = 1469598103934665603ULL;
    for (const char* p = buf; *p; p++)
        hash = (hash ^ (unsigned char)*p) * 1099511628211ULL;

    // 3. Convert to hex string, 15 chars max (truncate if needed)
    len = snprintf(out, 16, "%015llx", (unsigned long long)hash);
    if (len == 0) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "unable to hash final key for %s", key);
        return out;
    }

    return out;
}

bool write_devices_settings_to_nvs() {
    bool all_ok = true;

    for (int i = 0; i < get_max_connections(); i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry == NULL || entry->settings == NULL) {
            continue;
        }

        if (!xSemaphoreTake(entry->settings->mutex, portMAX_DELAY)) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "[%d] cannot get device settings mutex", entry->conn_id);
            continue;
        }

        nvs_handle_t device_settings_handle = {};
        esp_err_t err = nvs_open("device_settings", NVS_READWRITE, &device_settings_handle);
        if (err != ESP_OK) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "[%d] failed to open NVS: %s", entry->conn_id, esp_err_to_name(err));
            xSemaphoreGive(entry->settings->mutex);
            all_ok = false;
            continue;
        }

        char key_out[16];

        make_device_key_for_option(KEY_AUTOSPIN, entry->remote_bda, key_out);
        err = nvs_set_i8(device_settings_handle, key_out, entry->settings->autospin);

        make_device_key_for_option(KEY_AUTOCATCH, entry->remote_bda, key_out);
        err = nvs_set_i8(device_settings_handle, key_out, entry->settings->autocatch);

        make_device_key_for_option(KEY_AUTOSPIN_PROBABILITY, entry->remote_bda, key_out);
        err = nvs_set_u8(device_settings_handle, key_out, entry->settings->autospin_probability);

        // give it back in any of the following cases
        xSemaphoreGive(entry->settings->mutex);

        err = nvs_commit(device_settings_handle);
        if (err != ESP_OK) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "[%d] commit failed: %s", entry->conn_id, esp_err_to_name(err));
            nvs_close(device_settings_handle);
            all_ok = false;
            continue;
        }

        nvs_close(device_settings_handle);
    }

    return all_ok;
}

// Session key persistence functions for device reconnection
// These allow devices to reconnect without requiring passphrase re-entry

bool persist_device_session_keys(esp_bd_addr_t bda, const uint8_t* session_key, const uint8_t* reconnect_challenge) {
    if (!session_key || !reconnect_challenge) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "persist_device_session_keys: null pointers");
        return false;
    }

    nvs_handle_t device_settings_handle = {};
    esp_err_t err = nvs_open("device_settings", NVS_READWRITE, &device_settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "persist_device_session_keys: cannot open device_settings partition");
        return false;
    }

    bool all_ok = true;
    char key_out[16];

    // Store session key
    make_device_key_for_option("sesskey", bda, key_out);
    err = nvs_set_blob(device_settings_handle, key_out, (const void*)session_key, 16);
    if (!nvs_write_check(CONFIG_STORAGE_TAG, err, "session_key")) {
        all_ok = false;
    }

    // Store reconnect challenge
    make_device_key_for_option("rechall", bda, key_out);
    err = nvs_set_blob(device_settings_handle, key_out, (const void*)reconnect_challenge, 32);
    if (!nvs_write_check(CONFIG_STORAGE_TAG, err, "reconnect_challenge")) {
        all_ok = false;
    }

    err = nvs_commit(device_settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "persist_device_session_keys: commit failed");
        nvs_close(device_settings_handle);
        return false;
    }

    nvs_close(device_settings_handle);
    if (all_ok) {
        ESP_LOGI(CONFIG_STORAGE_TAG, "device session keys persisted");
    }
    return all_ok;
}

bool retrieve_device_session_keys(esp_bd_addr_t bda, uint8_t* session_key_out, uint8_t* reconnect_challenge_out) {
    if (!session_key_out || !reconnect_challenge_out) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: null pointers");
        return false;
    }

    nvs_handle_t device_settings_handle = {};
    esp_err_t err = nvs_open("device_settings", NVS_READONLY, &device_settings_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: device_settings partition doesn't exist");
        } else {
            ESP_LOGE(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: cannot open device_settings partition");
        }
        return false;
    }

    bool all_ok = true;
    char key_out[16];
    size_t required_size = 0;

    // Retrieve session key
    make_device_key_for_option("sesskey", bda, key_out);
    err = nvs_get_blob(device_settings_handle, key_out, NULL, &required_size);
    if (err == ESP_OK && required_size == 16) {
        err = nvs_get_blob(device_settings_handle, key_out, session_key_out, &required_size);
        if (err != ESP_OK) {
            ESP_LOGW(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: failed to read session_key");
            all_ok = false;
        }
    } else {
        ESP_LOGD(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: session_key not found or invalid size");
        all_ok = false;
    }

    // Retrieve reconnect challenge
    make_device_key_for_option("rechall", bda, key_out);
    required_size = 0;
    err = nvs_get_blob(device_settings_handle, key_out, NULL, &required_size);
    if (err == ESP_OK && required_size == 32) {
        err = nvs_get_blob(device_settings_handle, key_out, reconnect_challenge_out, &required_size);
        if (err != ESP_OK) {
            ESP_LOGW(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: failed to read reconnect_challenge");
            all_ok = false;
        }
    } else {
        ESP_LOGD(CONFIG_STORAGE_TAG, "retrieve_device_session_keys: reconnect_challenge not found or invalid size");
        all_ok = false;
    }

    nvs_close(device_settings_handle);
    if (all_ok) {
        ESP_LOGI(CONFIG_STORAGE_TAG, "device session keys retrieved");
    }
    return all_ok;
}

bool has_cached_session(esp_bd_addr_t bda) {
    nvs_handle_t device_settings_handle = {};
    esp_err_t err = nvs_open("device_settings", NVS_READONLY, &device_settings_handle);
    if (err != ESP_OK) {
        return false;
    }

    char key_out[16];
    size_t required_size = 0;

    // Check if session key exists with correct size
    make_device_key_for_option("sesskey", bda, key_out);
    err = nvs_get_blob(device_settings_handle, key_out, NULL, &required_size);
    
    nvs_close(device_settings_handle);
    
    return (err == ESP_OK && required_size == 16);
}

bool clear_device_session(esp_bd_addr_t bda) {
    nvs_handle_t device_settings_handle = {};
    esp_err_t err = nvs_open("device_settings", NVS_READWRITE, &device_settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "clear_device_session: cannot open device_settings partition");
        return false;
    }

    bool all_ok = true;
    char key_out[16];

    // Clear session key
    make_device_key_for_option("sesskey", bda, key_out);
    err = nvs_erase_key(device_settings_handle, key_out);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(CONFIG_STORAGE_TAG, "clear_device_session: failed to erase session_key");
        all_ok = false;
    }

    // Clear reconnect challenge
    make_device_key_for_option("rechall", bda, key_out);
    err = nvs_erase_key(device_settings_handle, key_out);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(CONFIG_STORAGE_TAG, "clear_device_session: failed to erase reconnect_challenge");
        all_ok = false;
    }

    err = nvs_commit(device_settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "clear_device_session: commit failed");
        nvs_close(device_settings_handle);
        return false;
    }

    nvs_close(device_settings_handle);
    if (all_ok) {
        ESP_LOGI(CONFIG_STORAGE_TAG, "device session cleared");
    }
    return all_ok;
}
