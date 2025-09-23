#include "button_input.h"
#include "config_secrets.h"
#include "config_storage.h"
#include "esp_log.h"
#include "esp_system.h"
#include "log_tags.h"
#include "pgp_autobutton.h"
#include "pgp_bluetooth.h"
#include "pgp_gap.h"
#include "pgp_gatts.h"
#include "secrets.h"
#include "settings.h"
#include "setup_button.h"
#include "stats.h"
#include "uart.h"

void app_main() {
    // uart menu. put it first because it purges all logs
    init_uart();

    // set log levels which let init msgs through
    log_levels_debug();

    // check reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(PGPEMU_TAG, "reset reason: %d", reset_reason);

    if (reset_reason == ESP_RST_BROWNOUT) {
        // keep it from bootlooping too quick when powering from low battery
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }

    // init nvs storage
    init_config_storage();

    // init settings mutex
    init_settings();
    // restore saved settings from nvs
    read_stored_settings(false);

    // restore log levels
    if (settings.log_level == 3) {
        ESP_LOGI(PGPEMU_TAG, "log levels verbose");
        log_levels_verbose();
    } else if (settings.log_level == 2) {
        ESP_LOGI(PGPEMU_TAG, "log levels info");
        log_levels_info();
    } else {
        ESP_LOGI(PGPEMU_TAG, "log levels debug");
        log_levels_debug();
    }

    // read secrets from nvs (settings are safe to use because mutex is still locked)
    read_secrets_id(settings.chosen_device, PGP_CLONE_NAME, PGP_MAC, PGP_DEVICE_KEY, PGP_BLOB);

    if (!PGP_VALID()) {
        // release mutex
        settings_ready();
        ESP_LOGE(PGPEMU_TAG,
            "NO PGP SECRETS AVAILABLE IN SLOT %d! Set them using secrets_upload.py or chose "
            "another using the 'X' menu!",
            settings.chosen_device);
        return;
    }

    if (setup_button_pressed_on_boot()) {
        settings_ready();  // release mutex
        ESP_LOGI(PGPEMU_TAG, "setup button pressed on boot; continuing startup");
    }

    // push button
    if (settings.use_button) {
        init_button_input();
    } else {
        ESP_LOGI(PGPEMU_TAG, "input button disabled");
    }

    // runtime counter
    init_stats();

    // start autosetting task
    if (!init_autosetting()) {
        ESP_LOGI(PGPEMU_TAG, "creating setting task failed");
        return;
    }

    // start autobutton task
    if (!init_autobutton()) {
        ESP_LOGI(PGPEMU_TAG, "creating button task failed");
        return;
    }

    // set clone mac and start bluetooth
    if (!init_bluetooth()) {
        ESP_LOGI(PGPEMU_TAG, "bluetooth init failed");
        return;
    }

    // done
    ESP_LOGI(PGPEMU_TAG, "Device: %s", PGP_CLONE_NAME);
    ESP_LOGI(PGPEMU_TAG,
        "MAC: %02x:%02x:%02x:%02x:%02x:%02x",
        PGP_MAC[0],
        PGP_MAC[1],
        PGP_MAC[2],
        PGP_MAC[3],
        PGP_MAC[4],
        PGP_MAC[5]);
    ESP_LOGI(PGPEMU_TAG, "Ready.");

    // make settings available
    settings_ready();
}
