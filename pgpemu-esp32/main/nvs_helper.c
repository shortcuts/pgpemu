#include "nvs_helper.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"

#include <stdbool.h>
#include <stddef.h>

bool nvs_read_check(const char* tag, esp_err_t err, const char* name) {
    switch (err) {
    case ESP_OK:
        return true;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(tag, "nvs value %s is not initialized yet!", name);
        return false;
    default:
        ESP_LOGE(tag, "nvs error reading %s: %s", name, esp_err_to_name(err));
        return false;
    }
}

bool nvs_write_check(const char* tag, esp_err_t err, const char* name) {
    if (err == ESP_OK) {
        return true;
    }

    ESP_LOGE(tag, "nvs error writing %s: %s", name, esp_err_to_name(err));

    return false;
}

bool nvs_open_readonly(const char* tag, const char* namespace, nvs_handle_t* out_handle) {
    if (!out_handle) {
        ESP_LOGE(tag, "nvs_open_readonly: out_handle is NULL");
        return false;
    }

    *out_handle = 0;
    esp_err_t err = nvs_open(namespace, NVS_READONLY, out_handle);

    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(tag, "nvs partition %s doesn't exist, using defaults", namespace);
        } else {
            ESP_ERROR_CHECK(err);  // Panic for other errors
        }
        return false;
    }

    return true;
}

bool nvs_open_readwrite(const char* tag, const char* namespace, nvs_handle_t* out_handle) {
    if (!out_handle) {
        ESP_LOGE(tag, "nvs_open_readwrite: out_handle is NULL");
        return false;
    }

    *out_handle = 0;
    esp_err_t err = nvs_open(namespace, NVS_READWRITE, out_handle);

    if (err != ESP_OK) {
        ESP_ERROR_CHECK(err);  // Panic on any error
        return false;          // Unreachable, but for clarity
    }

    return true;
}

void nvs_safe_close(nvs_handle_t handle) {
    if (handle != 0) {
        nvs_close(handle);
    }
}

bool nvs_read_blob_checked(const char* tag,
    nvs_handle_t handle,
    const char* key,
    uint8_t* out_buf,
    size_t expected_size) {
    if (!out_buf || expected_size == 0) {
        ESP_LOGE(tag, "nvs_read_blob_checked: invalid parameters (buf=%p, size=%zu)", out_buf, expected_size);
        return false;
    }

    // First query the size
    size_t required_size = 0;
    esp_err_t err = nvs_get_blob(handle, key, NULL, &required_size);

    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(tag, "nvs_read_blob_checked: %s not found", key);
        } else {
            ESP_LOGE(tag, "nvs_read_blob_checked: failed to query size of %s: %s", key, esp_err_to_name(err));
        }
        return false;
    }

    // Validate size
    if (required_size != expected_size) {
        ESP_LOGW(tag,
            "nvs_read_blob_checked: %s has invalid size (expected %zu, got %zu)",
            key,
            expected_size,
            required_size);
        return false;
    }

    // Read the blob
    err = nvs_get_blob(handle, key, out_buf, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "nvs_read_blob_checked: failed to read %s: %s", key, esp_err_to_name(err));
        return false;
    }

    return true;
}

bool nvs_commit_and_close(const char* tag, nvs_handle_t handle, const char* key_name) {
    if (handle == 0) {
        ESP_LOGE(tag, "nvs_commit_and_close: invalid handle for %s", key_name);
        return false;
    }

    esp_err_t err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(tag, "nvs_commit_and_close: failed to commit %s: %s", key_name, esp_err_to_name(err));
        return false;
    }

    return true;
}
