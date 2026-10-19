#include "config_secrets.h"

#include "esp_log.h"
#include "esp_system.h"
#include "log_tags.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_helper.h"
#include "secrets.h"

#include <string.h>

// nvs keys
static const char KEY_CLONE_NAME[] = "name";
static const char KEY_MAC[] = "mac";
static const char KEY_DEVICE_KEY[] = "dkey";
static const char KEY_BLOB[] = "blob";
static const char namespace[] = "pgpsecret";
// Own partition so the auto-erase of the default `nvs` partition never reaches the secrets.
static const char partition[] = "pgpsecret";

bool init_secrets_partition() {
    esp_err_t err = nvs_flash_init_partition(partition);
    if (err != ESP_OK) {
        // never erase: only a USB reflash restores the secrets
        ESP_LOGE(CONFIG_SECRETS_TAG, "cannot init %s partition: %s", partition, esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool open_secrets(nvs_open_mode_t openMode, nvs_handle_t* outHandle) {
    if (!outHandle) {
        ESP_LOGE(CONFIG_SECRETS_TAG, "outHandle nullptr");
        return false;
    }

    esp_err_t err = nvs_open_from_partition(partition, namespace, openMode, outHandle);
    if (err != ESP_OK) {
        ESP_LOGE(CONFIG_SECRETS_TAG, "cannot open %s: %s", namespace, esp_err_to_name(err));
        return false;
    }

    return true;
}

bool reset_secrets() {
    nvs_handle_t handle;
    if (!open_secrets(NVS_READWRITE, &handle)) {
        return false;
    }

    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(CONFIG_SECRETS_TAG, "deleted secrets");

    return true;
}

bool write_secrets(const char* name, const uint8_t* mac, const uint8_t* key, const uint8_t* blob) {
    nvs_handle_t handle;
    if (!open_secrets(NVS_READWRITE, &handle)) {
        return false;
    }

    // mac last: read_secrets() treats a zero mac as "no secrets", so a cut-off write stays in provisioning mode
    bool ok = nvs_set_str(handle, KEY_CLONE_NAME, name) == ESP_OK
              && nvs_set_blob(handle, KEY_DEVICE_KEY, key, sizeof(PGP_DEVICE_KEY)) == ESP_OK
              && nvs_set_blob(handle, KEY_BLOB, blob, sizeof(PGP_BLOB)) == ESP_OK
              && nvs_set_blob(handle, KEY_MAC, mac, sizeof(PGP_MAC)) == ESP_OK && nvs_commit(handle) == ESP_OK;
    if (!ok) {
        ESP_LOGE(CONFIG_SECRETS_TAG, "cannot write secrets");
        nvs_erase_all(handle);
        nvs_commit(handle);
    }

    nvs_close(handle);
    return ok;
}

bool read_secrets(char* name, uint8_t* mac, uint8_t* key, uint8_t* blob) {
    esp_err_t err;
    nvs_handle_t handle;
    if (!open_secrets(NVS_READONLY, &handle)) {
        return false;
    }

    bool all_ok = true;

    size_t size = sizeof(PGP_CLONE_NAME);
    memset(PGP_CLONE_NAME, 0, size);
    err = nvs_get_str(handle, KEY_CLONE_NAME, name, &size);
    all_ok = all_ok && nvs_read_check(CONFIG_SECRETS_TAG, err, KEY_CLONE_NAME);

    size = sizeof(PGP_MAC);
    memset(mac, 0, size);
    err = nvs_get_blob(handle, KEY_MAC, mac, &size);
    all_ok = all_ok && nvs_read_check(CONFIG_SECRETS_TAG, err, KEY_MAC);

    size = sizeof(PGP_DEVICE_KEY);
    memset(key, 0, size);
    err = nvs_get_blob(handle, KEY_DEVICE_KEY, key, &size);
    all_ok = all_ok && nvs_read_check(CONFIG_SECRETS_TAG, err, KEY_DEVICE_KEY);

    size = sizeof(PGP_BLOB);
    memset(blob, 0, size);
    err = nvs_get_blob(handle, KEY_BLOB, blob, &size);
    all_ok = all_ok && nvs_read_check(CONFIG_SECRETS_TAG, err, KEY_BLOB);

    nvs_close(handle);
    return all_ok;
}
