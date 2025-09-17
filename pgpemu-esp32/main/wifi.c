#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

void init_wifi() {
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_start() { return esp_wifi_start() == ESP_OK; }

bool wifi_stop() { return esp_wifi_stop() == ESP_OK; }

bool wifi_scan_start() {
    wifi_scan_config_t scan_config = {.ssid = 0, .bssid = 0, .channel = 0, .show_hidden = false};

    return esp_wifi_scan_start(&scan_config, true) == ESP_OK;
}

bool wifi_scan_stop() { return esp_wifi_scan_stop() == ESP_OK; }
