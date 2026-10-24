#include "button_input.h"
#include "config_secrets.h"
#include "config_storage.h"
#include "crash_report.h"
#include "esp_log.h"
#include "esp_system.h"
#include "forget_pairings.h"
#include "known_devices.h"
#include "led_output.h"
#include "log_tags.h"
#include "pgp_autobutton.h"
#include "pgp_bluetooth.h"
#include "pgp_gap.h"
#include "pgp_ota.h"
#include "secrets.h"
#include "settings.h"
#include "setup_button.h"

void app_main() {
    // set log levels which let init msgs through
    log_levels_debug();

    // check reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(PGPEMU_TAG, "reset reason: %d", reset_reason);

    if (reset_reason == ESP_RST_BROWNOUT) {
        // keep it from bootlooping too quick when powering from low battery
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }

    forget_pairings_if_requested();
    crash_report_init();

    init_settings_nvs_partition();
    init_secrets_partition();

    init_global_settings();
    read_stored_global_settings(false);

    // restore log levels
    if (global_settings.log_level == 3) {
        ESP_LOGI(PGPEMU_TAG, "log levels verbose");
        log_levels_verbose();
    } else if (global_settings.log_level == 2) {
        ESP_LOGI(PGPEMU_TAG, "log levels info");
        log_levels_info();
    } else {
        ESP_LOGI(PGPEMU_TAG, "log levels debug");
        log_levels_debug();
    }

    // read secrets from nvs (settings are safe to use because mutex is still locked)
    read_secrets(PGP_CLONE_NAME, PGP_MAC, PGP_DEVICE_KEY, PGP_BLOB);

    // Without secrets only the Control Service runs, so the app can upload them; no game logic starts.
    const bool provisioning = !PGP_VALID();
    if (provisioning) {
        ESP_LOGW(PGPEMU_TAG, "provisioning mode: no secrets");
    } else if (setup_button_pressed_on_boot()) {
        global_settings_ready();  // release mutex
        ESP_LOGI(PGPEMU_TAG, "setup button pressed on boot; continuing startup");
    }

    const bool crashed = reset_reason == ESP_RST_PANIC || reset_reason == ESP_RST_INT_WDT
                         || reset_reason == ESP_RST_TASK_WDT || reset_reason == ESP_RST_WDT;
    init_led_output(provisioning, crashed);

    if (!provisioning) {
        known_devices_init();
        init_button_input();

        // start autobutton task
        if (!init_autobutton()) {
            ESP_LOGI(PGPEMU_TAG, "creating button task failed");
            return;
        }
    }

    if (!pgp_adv_init()) {
        ESP_LOGE(PGPEMU_TAG, "advertising retry timer failed");
        return;
    }

    // set clone mac and start bluetooth
    if (!init_bluetooth()) {
        ESP_LOGI(PGPEMU_TAG, "bluetooth init failed");
        return;
    }

    pgp_ota_init();

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

    // Release settings mutex FIRST, before advertise_if_needed() calls get_setting().
    // This prevents deadlock during boot window when advertising needs to query settings.
    global_settings_ready();

    led_output_set_enabled(get_setting(&global_settings.led_enabled));
    advertise_if_needed();
}
