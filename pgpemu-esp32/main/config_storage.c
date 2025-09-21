#include "config_storage.h"

#include "config_secrets.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "log_tags.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_helper.h"
#include "settings.h"

static const char KEY_AUTOCATCH[] = "catch";
static const char KEY_AUTOSPIN[] = "spin";
static const char KEY_CHOSEN_DEVICE[] = "device";
static const char KEY_CONNECTION_COUNT[] = "conns";
static const char KEY_USE_BUTTON[] = "usebut";
static const char KEY_LOG_LEVEL[] = "log_level";

void init_config_storage() {
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

// use_mutex=false is for use in app_main when the mutex isn't available yet
void read_stored_settings(bool use_mutex) {
    esp_err_t err;
    int8_t autocatch = 0, autospin = 0, use_button = 0, log_level = 0;
    uint8_t chosen_device = 0, connection_count = 0;

    if (use_mutex) {
        if (!xSemaphoreTake(settings.mutex, portMAX_DELAY)) {
            ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get settings mutex");
            return;
        }
    }

    // open config partition
    nvs_handle_t user_settings_handle;
    err = nvs_open("user_settings", NVS_READONLY, &user_settings_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(CONFIG_STORAGE_TAG,
                "user settings partition doesn't exist, using default settings");
        } else {
            ESP_ERROR_CHECK(err);  // panic
        }

        xSemaphoreGive(settings.mutex);
        return;
    }

    // read bool settings
    err = nvs_get_i8(user_settings_handle, KEY_AUTOCATCH, &autocatch);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_AUTOCATCH)) {
        settings.autocatch = (bool)autocatch;
    }
    err = nvs_get_i8(user_settings_handle, KEY_AUTOSPIN, &autospin);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_AUTOSPIN)) {
        settings.autospin = (bool)autospin;
    }
    err = nvs_get_i8(user_settings_handle, KEY_USE_BUTTON, &use_button);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_USE_BUTTON)) {
        settings.use_button = (bool)use_button;
    }
    err = nvs_get_i8(user_settings_handle, KEY_LOG_LEVEL, &log_level);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_LOG_LEVEL)) {
        settings.log_level = log_level;
    }

    // read uint8_t settings
    err = nvs_get_u8(user_settings_handle, KEY_CHOSEN_DEVICE, &chosen_device);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_CHOSEN_DEVICE)) {
        if (is_valid_secrets_id(chosen_device)) {
            settings.chosen_device = chosen_device;
        } else {
            ESP_LOGE(CONFIG_STORAGE_TAG, "invalid chosen device %d", chosen_device);
        }
    }
    err = nvs_get_u8(user_settings_handle, KEY_CONNECTION_COUNT, &connection_count);
    if (nvs_read_check(CONFIG_STORAGE_TAG, err, KEY_CONNECTION_COUNT)) {
        if (connection_count <= CONFIG_BT_ACL_CONNECTIONS && connection_count > 0) {
            settings.target_active_connections = connection_count;
        } else {
            ESP_LOGE(CONFIG_STORAGE_TAG,
                "invalid target active connections: %d (1-%d allowed)",
                connection_count,
                CONFIG_BT_ACL_CONNECTIONS);
        }
    }

    nvs_close(user_settings_handle);

    if (use_mutex) {
        xSemaphoreGive(settings.mutex);
    }

    ESP_LOGI(CONFIG_STORAGE_TAG, "user settings read from nvs");
}

bool write_config_storage() {
    esp_err_t err;
    if (!xSemaphoreTake(settings.mutex, portMAX_DELAY)) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "cannot get settings mutex");
        return false;
    }

    // open config partition
    nvs_handle_t user_settings_handle;
    err = nvs_open("user_settings", NVS_READWRITE, &user_settings_handle);
    ESP_ERROR_CHECK(err);

    bool all_ok = true;

    // bools
    err = nvs_set_i8(user_settings_handle, KEY_AUTOCATCH, settings.autocatch);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_AUTOCATCH);
    err = nvs_set_i8(user_settings_handle, KEY_AUTOSPIN, settings.autospin);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_AUTOSPIN);
    err = nvs_set_i8(user_settings_handle, KEY_USE_BUTTON, settings.use_button);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_USE_BUTTON);
    err = nvs_set_i8(user_settings_handle, KEY_LOG_LEVEL, settings.log_level);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_LOG_LEVEL);

    // uint8s
    err = nvs_set_u8(user_settings_handle, KEY_CHOSEN_DEVICE, settings.chosen_device);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_CHOSEN_DEVICE);
    err =
        nvs_set_u8(user_settings_handle, KEY_CONNECTION_COUNT, settings.target_active_connections);
    all_ok = all_ok && nvs_write_check(CONFIG_STORAGE_TAG, err, KEY_CONNECTION_COUNT);

    // give it back in any of the following cases
    xSemaphoreGive(settings.mutex);

    err = nvs_commit(user_settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_STORAGE_TAG, "commit failed");
        nvs_close(user_settings_handle);
        return false;
    }

    nvs_close(user_settings_handle);
    return all_ok;
}
