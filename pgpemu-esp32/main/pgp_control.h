#ifndef PGP_CONTROL_H
#define PGP_CONTROL_H

#include "esp_gatts_api.h"

#include <stdbool.h>
#include <stdint.h>

// Control service
enum {
    IDX_CONTROL_SVC,
    IDX_CHAR_CONTROL_COMMAND,
    IDX_CHAR_CONTROL_COMMAND_VAL,
    IDX_CHAR_CONTROL_RESPONSE,
    IDX_CHAR_CONTROL_RESPONSE_VAL,
    IDX_CHAR_CONTROL_RESPONSE_CFG,
    CONTROL_LAST_IDX
};

extern uint16_t control_handle_table[CONTROL_LAST_IDX];

// Response payload cap: MAX_VALUE_LENGTH (500, pgp_gatts.h) minus the
// 2-byte [status][opcode] response header.
#define CONTROL_MAX_RESPONSE_PAYLOAD (500 - 2)

typedef enum {
    CONTROL_OP_HELP = 0x01,
    CONTROL_OP_GET_GLOBAL_SETTINGS = 0x02,
    CONTROL_OP_SAVE_SETTINGS = 0x03,
    CONTROL_OP_GET_SECRETS = 0x04,
    CONTROL_OP_RESET_SECRETS = 0x05,
    CONTROL_OP_RESTART = 0x06,
    CONTROL_OP_GET_LED_STATE = 0x07,
    CONTROL_OP_CYCLE_LOG_LEVEL = 0x08,
    CONTROL_OP_GET_RUNTIME_STATS = 0x09,
    CONTROL_OP_GET_TASK_LIST = 0x0A,
    CONTROL_OP_ADVERTISE_START = 0x0B,
    CONTROL_OP_ADVERTISE_STOP = 0x0C,
    CONTROL_OP_GET_CLIENT_STATES = 0x0D,
    CONTROL_OP_RESET_CLIENT_STATES = 0x0E,
    CONTROL_OP_SET_MAX_CONNECTIONS = 0x0F,
    CONTROL_OP_TOGGLE_AUTOSPIN = 0x10,
    CONTROL_OP_TOGGLE_AUTOCATCH = 0x11,
    CONTROL_OP_GET_CLIENT_SUMMARY = 0x12,
} control_opcode_t;

typedef enum {
    CONTROL_STATUS_OK = 0x00,
    CONTROL_STATUS_ERR_UNKNOWN_OPCODE = 0x01,
    CONTROL_STATUS_ERR_MALFORMED_PAYLOAD = 0x02,
    CONTROL_STATUS_ERR_NOT_BONDED = 0x03,
    CONTROL_STATUS_ERR_BUSY = 0x04,
    CONTROL_STATUS_ERR_INTERNAL = 0x05,
} control_status_t;

// Called from pgp_gatts.c's ESP_GATTS_REG_EVT, alongside the other
// services' esp_ble_gatts_create_attr_tab calls.
void pgp_control_create_attr_table(esp_gatt_if_t gatts_if);

// Called from pgp_gatts.c's ESP_GATTS_CREAT_ATTR_TAB_EVT after the existing
// battery/led/cert checks find no match. Returns true (and finishes
// starting the service) if this event was for the Control Service.
bool pgp_control_handle_attr_tab_created(esp_ble_gatts_cb_param_t* param);

// Called from pgp_gatts.c's ESP_GATTS_WRITE_EVT handle-comparison chain,
// same slot as handle_pgp_handshake_first/second. Returns true if the
// write targeted a Control Service handle (and was handled); false lets
// pgp_gatts.c fall through to its existing unknown-handle logging.
bool pgp_control_try_handle_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);

#endif /* PGP_CONTROL_H */
