#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_rom_crc.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "config_secrets.h"

#include "log_tags.h"
#include "nvs_helper.h"
#include "secrets.h"

// nvs keys
static const char KEY_CLONE_NAME[] = "name";
static const char KEY_MAC[] = "mac";
static const char KEY_DEVICE_KEY[] = "dkey";
static const char KEY_BLOB[] = "blob";

bool is_valid_secrets_id(uint8_t id)
{
    return id <= 9;
}

static bool open_secrets_id(uint8_t id, nvs_open_mode_t openMode, nvs_handle_t *outHandle)
{
    if (!outHandle)
    {
        ESP_LOGE(CONFIG_SECRETS_TAG, "outHandle nullptr");
        return false;
    }
    if (!is_valid_secrets_id(id))
    {
        ESP_LOGE(CONFIG_SECRETS_TAG, "invalid secrets id %d", id);
        return false;
    }

    char namespace[] = "pgpsecret_x";
    namespace[strlen("pgpsecret_")] = '0' + id;

    esp_err_t err = nvs_open(namespace, openMode, outHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(CONFIG_SECRETS_TAG, "cannot open %s: %s", namespace, esp_err_to_name(err));
        return false;
    }

    return true;
}

void show_secrets_slots()
{
    char namespace[] = "pgpsecret_x";
    nvs_handle_t handle;
    esp_err_t err;

    for (int i = 0; i < 10; i++)
    {
        namespace[strlen("pgpsecret_")] = '0' + i;
        char name[sizeof(PGP_CLONE_NAME)] = {0};
        char mac[sizeof(PGP_MAC)] = {0};

        bool gotData = false;
        err = nvs_open(namespace, NVS_READONLY, &handle);
        if (err == ESP_OK)
        {
            bool all_ok = true;

            size_t size = sizeof(name);
            err = nvs_get_str(handle, KEY_CLONE_NAME, name, &size);
            all_ok = all_ok && (err == ESP_OK);

            size = sizeof(PGP_MAC);
            err = nvs_get_blob(handle, KEY_MAC, mac, &size);
            all_ok = all_ok && (err == ESP_OK);

            gotData = all_ok;

            nvs_close(handle);
        }

        if (gotData)
        {
            ESP_LOGI(CONFIG_SECRETS_TAG, "- %s: device=%s mac=%02x:%02x:%02x:%02x:%02x:%02x", namespace, name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
        else
        {
            ESP_LOGI(CONFIG_SECRETS_TAG, "- %s: (none)", namespace);
        }
    }
}

bool delete_secrets_id(uint8_t id)
{
    nvs_handle_t handle;
    if (!open_secrets_id(id, NVS_READWRITE, &handle))
    {
        return false;
    }

    nvs_erase_all(handle);
    nvs_commit(handle);

    ESP_LOGI(CONFIG_SECRETS_TAG, "deleted secrets slot %d", id);

    return true;
}

bool read_secrets_id(uint8_t id, char *name, uint8_t *mac, uint8_t *key, uint8_t *blob)
{
    esp_err_t err;
    nvs_handle_t handle;
    if (!open_secrets_id(id, NVS_READONLY, &handle))
    {
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

bool write_secrets_id(uint8_t id, char *name, uint8_t *mac, uint8_t *key, uint8_t *blob)
{
    if (!name || !mac || !key || !blob)
    {
        ESP_LOGE(CONFIG_SECRETS_TAG, "got nullptr");
        return false;
    }

    esp_err_t err;
    nvs_handle_t handle;
    if (!open_secrets_id(id, NVS_READWRITE, &handle))
    {
        return false;
    }

    bool all_ok = true;

    err = nvs_set_str(handle, KEY_CLONE_NAME, name);
    all_ok = all_ok && nvs_write_check(CONFIG_SECRETS_TAG, err, KEY_CLONE_NAME);

    err = nvs_set_blob(handle, KEY_MAC, mac, sizeof(PGP_MAC));
    all_ok = all_ok && nvs_write_check(CONFIG_SECRETS_TAG, err, KEY_MAC);

    err = nvs_set_blob(handle, KEY_DEVICE_KEY, key, sizeof(PGP_DEVICE_KEY));
    all_ok = all_ok && nvs_write_check(CONFIG_SECRETS_TAG, err, KEY_DEVICE_KEY);

    err = nvs_set_blob(handle, KEY_BLOB, blob, sizeof(PGP_BLOB));
    all_ok = all_ok && nvs_write_check(CONFIG_SECRETS_TAG, err, KEY_BLOB);

    if (all_ok)
    {
        nvs_commit(handle);
        ESP_LOGI(CONFIG_SECRETS_TAG, "[OK] wrote secrets to slot %d", id);
    }
    else
    {
        ESP_LOGE(CONFIG_SECRETS_TAG, "[FAIL] failed writing secrets to slot %d", id);
    }

    nvs_close(handle);
    return all_ok;
}

uint32_t get_secrets_crc32(uint8_t *mac, uint8_t *key, uint8_t *blob)
{
    // get CRC checksum for data in given slot
    // same settings as https://docs.python.org/3/library/binascii.html#binascii.crc32
    uint32_t crc = 0;
    crc = esp_rom_crc32_le(crc, mac, sizeof(PGP_MAC));
    crc = esp_rom_crc32_le(crc, key, sizeof(PGP_DEVICE_KEY));
    crc = esp_rom_crc32_le(crc, blob, sizeof(PGP_BLOB));
    return crc;
}