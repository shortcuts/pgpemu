// PC-test stub: flash init always succeeds
#pragma once
#include "nvs.h"
static inline esp_err_t nvs_flash_init(void) {
    return ESP_OK;
}
static inline esp_err_t nvs_flash_erase(void) {
    return ESP_OK;
}
esp_err_t nvs_flash_init_partition(const char* part);
