#include "forget_pairings.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "log_tags.h"
#include "nvs_flash.h"

#include <stdbool.h>
#include <stdint.h>

#define FORGET_MAGIC 0xF0261E57u

// RTC memory survives a software reset, so the request needs no flash write.
static RTC_NOINIT_ATTR uint32_t forget_request;

void forget_pairings_and_restart(void) {
    forget_request = FORGET_MAGIC;
    esp_restart();
}

void forget_pairings_if_requested(void) {
    // A cold power-up leaves RTC_NOINIT garbage; only a software reset can carry a real request.
    const bool requested = esp_reset_reason() == ESP_RST_SW && forget_request == FORGET_MAGIC;
    // Cleared before the erase so a crash during it cannot loop.
    forget_request = 0;
    if (!requested) {
        return;
    }
    // The Bluetooth stack holds nvs open while running, so the erase cannot happen at request time.
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(PGPEMU_TAG, "forget pairings: erase failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(PGPEMU_TAG, "forget pairings: erased nvs");
}
