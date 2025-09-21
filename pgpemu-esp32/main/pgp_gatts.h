#ifndef PGP_GATTS_H
#define PGP_GATTS_H

#include "esp_bt.h"
#include "esp_gatts_api.h"

void gatts_event_handler(esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t* param);

// Battery service
enum {
    IDX_BATTERY_SVC,
    IDX_CHAR_BATTERY_LEVEL,
    IDX_CHAR_BATTERY_LEVEL_VAL,
    IDX_CHAR_BATTERY_LEVEL_CFG,
    BATTERY_LAST_IDX
};

// LED/BUTTON service
enum {
    IDX_LED_BUTTON_SVC,
    IDX_CHAR_LED,
    IDX_CHAR_LED_VAL,
    IDX_CHAR_BUTTON,
    IDX_CHAR_BUTTON_VAL,
    IDX_CHAR_BUTTON_CFG,
    IDX_CHAR_UNKNOWN,
    IDX_CHAR_UNKNOWN_VAL,
    IDX_CHAR_UPDATE_REQUEST,
    IDX_CHAR_UPDATE_REQUEST_VAL,
    IDX_CHAR_FW_VERSION,
    IDX_CHAR_FW_VERSION_VAL,
    LED_BUTTON_LAST_IDX
};

// Certificate service
enum {
    IDX_CERT_SVC,
    IDX_CHAR_CENTRAL_TO_SFIDA,
    IDX_CHAR_CENTRAL_TO_SFIDA_VAL,
    IDX_CHAR_SFIDA_COMMANDS,
    IDX_CHAR_SFIDA_COMMANDS_VAL,
    IDX_CHAR_SFIDA_COMMANDS_CFG,
    IDX_CHAR_SFIDA_TO_CENTRAL,
    IDX_CHAR_SFIDA_TO_CENTRAL_VAL,
    CERT_LAST_IDX
};

extern uint16_t battery_handle_table[BATTERY_LAST_IDX];
extern uint16_t led_button_handle_table[LED_BUTTON_LAST_IDX];
extern uint16_t certificate_handle_table[CERT_LAST_IDX];

#endif /* PGP_GATTS_H */
