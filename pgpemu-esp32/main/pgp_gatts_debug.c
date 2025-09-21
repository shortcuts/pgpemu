#include "pgp_gatts_debug.h"

#include "pgp_gatts.h"

#include <stdint.h>

static const char* battery_char_names[] = {
    "BATTERY_SVC",
    "CHAR_BATTERY_LEVEL",
    "CHAR_BATTERY_LEVEL_VAL",
    "CHAR_BATTERY_LEVEL_CFG",
};

static const char* led_button_char_names[] = { "LED_BUTTON_SVC",
    "CHAR_LED",
    "CHAR_LED_VAL",
    "CHAR_BUTTON",
    "CHAR_BUTTON_VAL",
    "CHAR_BUTTON_CFG",
    "CHAR_UNKNOWN",
    "CHAR_UNKNOWN_VAL",
    "CHAR_UPDATE_REQUEST",
    "CHAR_UPDATE_REQUEST_VAL",
    "CHAR_FW_VERSION",
    "CHAR_FW_VERSION_VAL" };

static const char* cert_char_names[] = { "CERT_SVC",
    "CHAR_CENTRAL_TO_SFIDA",
    "CHAR_CENTRAL_TO_SFIDA_VAL",
    "CHAR_SFIDA_COMMANDS",
    "CHAR_SFIDA_COMMANDS_VAL",
    "CHAR_SFIDA_COMMANDS_CFG",
    "CHAR_SFIDA_TO_CENTRAL",
    "CHAR_SFIDA_TO_CENTRAL_VAL" };

static const char* UNKNOWN_HANDLE_NAME = "<UNKNOWN HANDLE NAME>";

static int find_handle_index(uint16_t handle, uint16_t* handle_table, int count) {
    for (int i = 0; i < count; i++) {
        if (handle_table[i] == handle)
            return i;
    }
    return -1;
}

// for debugging
const char* char_name_from_handle(uint16_t handle) {
    int idx;
    idx = find_handle_index(handle, battery_handle_table, BATTERY_LAST_IDX);
    if (idx >= 0)
        return battery_char_names[idx];
    idx = find_handle_index(handle, led_button_handle_table, LED_BUTTON_LAST_IDX);
    if (idx >= 0)
        return led_button_char_names[idx];

    idx = find_handle_index(handle, certificate_handle_table, CERT_LAST_IDX);
    if (idx >= 0)
        return cert_char_names[idx];

    return UNKNOWN_HANDLE_NAME;
}