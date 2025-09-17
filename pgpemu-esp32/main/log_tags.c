#include "log_tags.h"

#include "esp_log.h"

void log_levels_init() {
    esp_log_level_set(BT_GAP_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(BT_GATTS_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(BT_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(BUTTON_INPUT_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(BUTTON_TASK_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(CERT_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(CONFIG_SECRETS_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(CONFIG_STORAGE_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(HANDSHAKE_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(PGPEMU_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(LEDHANDLER_TAG, ESP_LOG_INFO);
    esp_log_level_set(LEDOUTPUT_TAG, ESP_LOG_INFO);
    esp_log_level_set(POWERBANK_TASK_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(STATS_TAG, ESP_LOG_DEBUG);
    esp_log_level_set(UART_TAG, ESP_LOG_INFO);
}

void log_levels_max() {
    esp_log_level_set(BT_GAP_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(BT_GATTS_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(BT_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(BUTTON_INPUT_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(BUTTON_TASK_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(CERT_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(CONFIG_SECRETS_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(CONFIG_STORAGE_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(HANDSHAKE_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(PGPEMU_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(LEDHANDLER_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(LEDOUTPUT_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(POWERBANK_TASK_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(STATS_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set(UART_TAG, ESP_LOG_VERBOSE);
}

void log_levels_min() {
    esp_log_level_set(BT_GAP_TAG, ESP_LOG_INFO);
    esp_log_level_set(BT_GATTS_TAG, ESP_LOG_INFO);
    esp_log_level_set(BT_TAG, ESP_LOG_INFO);
    esp_log_level_set(BUTTON_INPUT_TAG, ESP_LOG_INFO);
    esp_log_level_set(BUTTON_TASK_TAG, ESP_LOG_INFO);
    esp_log_level_set(CERT_TAG, ESP_LOG_INFO);
    esp_log_level_set(CONFIG_SECRETS_TAG, ESP_LOG_INFO);
    esp_log_level_set(CONFIG_STORAGE_TAG, ESP_LOG_INFO);
    esp_log_level_set(HANDSHAKE_TAG, ESP_LOG_INFO);
    esp_log_level_set(PGPEMU_TAG, ESP_LOG_INFO);
    esp_log_level_set(LEDHANDLER_TAG, ESP_LOG_INFO);
    esp_log_level_set(LEDOUTPUT_TAG, ESP_LOG_INFO);
    esp_log_level_set(POWERBANK_TASK_TAG, ESP_LOG_INFO);
    esp_log_level_set(STATS_TAG, ESP_LOG_INFO);
    esp_log_level_set(UART_TAG, ESP_LOG_INFO);
}
