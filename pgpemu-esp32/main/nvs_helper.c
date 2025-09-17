#include "nvs_helper.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"

bool nvs_read_check(const char* tag, esp_err_t err, const char* name) {
    switch (err) {
        case ESP_OK:
            return true;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(tag, "nvs value %s is not initialized yet!", name);
            break;
        default:
            ESP_LOGE(tag, "nvs error reading %s: %s", name, esp_err_to_name(err));
    }

    return false;
}

bool nvs_write_check(const char* tag, esp_err_t err, const char* name) {
    switch (err) {
        case ESP_OK:
            return true;
        default:
            ESP_LOGE(tag, "nvs error writing %s: %s", name, esp_err_to_name(err));
    }

    return false;
}
