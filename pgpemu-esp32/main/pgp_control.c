#include "pgp_control.h"

#include "config_secrets.h"  // reset_secrets()
#include "config_storage.h"  // write_global_settings_to_nvs, write_devices_settings_to_nvs
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "esp_system.h"  // esp_restart, esp_get_free_heap_size
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // vTaskList
#include "led_output.h"     // get_led_advertising
#include "log_tags.h"
#include "pgp_gap.h"              // pgp_advertise, pgp_advertise_stop
#include "pgp_gatts.h"            // MAX_VALUE_LENGTH
#include "pgp_handshake_multi.h"  // dump_client_states_format, get_active_connections, reset_client_states
#include "secrets.h"              // PGP_CLONE_NAME, PGP_MAC, PGP_DEVICE_KEY, PGP_BLOB
#include "settings.h"             // global_settings, get_setting*, set_setting_uint8, cycle_log_level, toggle_device_*
#include "stats.h"                // stats_format_runtime

#include <stdio.h>
#include <string.h>

#define CONTROL_CHAR_DECLARATION_SIZE (sizeof(uint8_t))
static const uint8_t CONTROL_INST_ID = 0;

// Standard BLE SIG UUIDs — local aliases matching pgp_gatts.c's
// primary_service_uuid/character_declaration_uuid/character_client_config_uuid;
// not duplicated logic, just the same ESP-IDF constant under this module's
// own name so this table has no cross-file coupling to pgp_gatts.c internals.
static const uint16_t control_primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t control_char_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t control_char_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t control_char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t control_char_prop_indicate = ESP_GATT_CHAR_PROP_BIT_INDICATE;
static const uint8_t control_cccd_initial_value[2] = { 0x00, 0x00 };

// Same base-suffix bytes as pgp_gatts.c's vendor UUIDs
// (0x8e,0x0d,0xef,0x8e,0x8b,0x7f,0xab,0x33,0x44,0x89,0x5b,0x09,0x77,0xe8,0xbb),
// next unused lead bytes after the certificate service's 0x37-0x3a.
static uint8_t GATTS_SERVICE_UUID_CONTROL[ESP_UUID_LEN_128] = { 0x40,
    0x8e,
    0x0d,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x09,
    0x77,
    0xe8,
    0xbb };
static uint8_t GATTS_CHAR_UUID_CONTROL_COMMAND[ESP_UUID_LEN_128] = { 0x41,
    0x8e,
    0x0d,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x09,
    0x77,
    0xe8,
    0xbb };
static uint8_t GATTS_CHAR_UUID_CONTROL_RESPONSE[ESP_UUID_LEN_128] = { 0x42,
    0x8e,
    0x0d,
    0xef,
    0x8e,
    0x8b,
    0x7f,
    0xab,
    0x33,
    0x44,
    0x89,
    0x5b,
    0x09,
    0x77,
    0xe8,
    0xbb };

uint16_t control_handle_table[CONTROL_LAST_IDX];
static uint8_t control_response_buffer[MAX_VALUE_LENGTH];  // {0x00,0x00} = status OK, opcode 0 until first response

static const esp_gatts_attr_db_t gatt_db_control[CONTROL_LAST_IDX] = {
    [IDX_CONTROL_SVC] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&control_primary_service_uuid,
            ESP_GATT_PERM_READ,
            ESP_UUID_LEN_128,
            ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_SERVICE_UUID_CONTROL } },

    [IDX_CHAR_CONTROL_COMMAND] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&control_char_declaration_uuid,
            ESP_GATT_PERM_READ,
            CONTROL_CHAR_DECLARATION_SIZE,
            CONTROL_CHAR_DECLARATION_SIZE,
            (uint8_t*)&control_char_prop_write } },
    [IDX_CHAR_CONTROL_COMMAND_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_CONTROL_COMMAND,
            ESP_GATT_PERM_WRITE_ENCRYPTED,
            MAX_VALUE_LENGTH,
            0,
            NULL } },  // write-only, never read back — no backing value needed

    [IDX_CHAR_CONTROL_RESPONSE] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&control_char_declaration_uuid,
            ESP_GATT_PERM_READ,
            CONTROL_CHAR_DECLARATION_SIZE,
            CONTROL_CHAR_DECLARATION_SIZE,
            (uint8_t*)&control_char_prop_indicate } },
    [IDX_CHAR_CONTROL_RESPONSE_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_CONTROL_RESPONSE,
            ESP_GATT_PERM_READ_ENCRYPTED,
            MAX_VALUE_LENGTH,
            2,
            (uint8_t*)control_response_buffer } },

    /* Client Characteristic Configuration Descriptor, same role as pgp_gatts.c's IDX_CHAR_SFIDA_COMMANDS_CFG */
    [IDX_CHAR_CONTROL_RESPONSE_CFG] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&control_char_client_config_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t),
            sizeof(control_cccd_initial_value),
            (uint8_t*)control_cccd_initial_value } },
};

void pgp_control_create_attr_table(esp_gatt_if_t gatts_if) {
    esp_err_t ret = esp_ble_gatts_create_attr_tab(gatt_db_control, gatts_if, CONTROL_LAST_IDX, CONTROL_INST_ID);
    if (ret) {
        ESP_LOGE(CONTROL_TAG, "create attr table for control failed, error code = %x", ret);
    }
}

bool pgp_control_handle_attr_tab_created(esp_ble_gatts_cb_param_t* param) {
    if (param->add_attr_tab.svc_uuid.len != ESP_UUID_LEN_128
        || memcmp(param->add_attr_tab.svc_uuid.uuid.uuid128, GATTS_SERVICE_UUID_CONTROL, ESP_UUID_LEN_128) != 0) {
        return false;
    }

    memcpy(control_handle_table, param->add_attr_tab.handles, sizeof(control_handle_table));
    esp_err_t err = esp_ble_gatts_start_service(control_handle_table[IDX_CONTROL_SVC]);
    if (err != ESP_OK) {
        ESP_LOGE(CONTROL_TAG, "failed starting service: %d", err);
    }
    ESP_LOGD(CONTROL_TAG, "create control attribute table success, handle = %d", param->add_attr_tab.num_handle);
    return true;
}

static void pgp_control_send_response(esp_gatt_if_t gatts_if,
    uint16_t conn_id,
    control_status_t status,
    uint8_t opcode,
    const uint8_t* payload,
    size_t payload_len) {
    if (payload_len > CONTROL_MAX_RESPONSE_PAYLOAD) {
        ESP_LOGW(CONTROL_TAG,
            "[%d] response payload %d truncated to %d",
            conn_id,
            (int)payload_len,
            CONTROL_MAX_RESPONSE_PAYLOAD);
        payload_len = CONTROL_MAX_RESPONSE_PAYLOAD;
    }

    uint8_t frame[2 + CONTROL_MAX_RESPONSE_PAYLOAD];
    frame[0] = (uint8_t)status;
    frame[1] = opcode;
    if (payload_len > 0) {
        memcpy(frame + 2, payload, payload_len);
    }

    esp_ble_gatts_set_attr_value(control_handle_table[IDX_CHAR_CONTROL_RESPONSE_VAL], 2 + payload_len, frame);
    esp_ble_gatts_send_indicate(
        gatts_if, conn_id, control_handle_table[IDX_CHAR_CONTROL_RESPONSE_VAL], 2 + payload_len, frame, false);
}

static void pgp_control_handle_command_write(esp_gatt_if_t gatts_if,
    uint16_t conn_id,
    const uint8_t* value,
    uint16_t len) {
    if (len < 1) {
        pgp_control_send_response(gatts_if, conn_id, CONTROL_STATUS_ERR_MALFORMED_PAYLOAD, 0, NULL, 0);
        return;
    }

    uint8_t opcode = value[0];
    const uint8_t* payload = value + 1;
    uint16_t payload_len = len - 1;

    uint8_t resp[CONTROL_MAX_RESPONSE_PAYLOAD];
    size_t resp_len = 0;
    control_status_t status = CONTROL_STATUS_OK;

    switch ((control_opcode_t)opcode) {
    case CONTROL_OP_HELP: {
        // Worst-case help text can exceed CONTROL_MAX_RESPONSE_PAYLOAD; snprintf
        // truncates safely and pgp_control_send_response() re-clamps payload_len,
        // so this is not a buffer overrun, just more text than gcc can prove fits.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        int n = snprintf((char*)resp,
            sizeof(resp),
            "---HELP---\n"
            "Secrets: %s\n"
            "Commands:\n"
            "- ? - help\n"
            "- L - show LED advertising state\n"
            "- l - cycle through log levels\n"
            "- r - show runtime counter\n"
            "- t - show FreeRTOS task list\n"
            "- s - show global settings values\n"
            "- S - save settings permanently\n"
            "- R - restart\n"
            "Secrets:\n"
            "- xs - show loaded secrets\n"
            "- xr - reset loaded secrets\n"
            "Bluetooth:\n"
            "- bA - start advertising\n"
            "- ba - stop advertising\n"
            "- bs - show client states\n"
            "- br - clear connections\n"
            "- b[1,4] - set maximum client connections (e.g. 3 clients max. with 'b3', up to %d, currently %d)\n"
            "Device Settings:\n"
            "- [1,4]s - toggle autospin\n"
            "- [1,4]c - toggle autocatch\n",
            PGP_CLONE_NAME,
            CONFIG_BT_ACL_CONNECTIONS,
            get_setting_uint8(&global_settings.target_active_connections));
        resp_len = (n > 0) ? (size_t)n : 0;
        break;
    }
    case CONTROL_OP_GET_GLOBAL_SETTINGS: {
        resp[0] = get_setting_uint8(&global_settings.log_level);
        resp[1] = get_setting(&global_settings.advertising_enabled) ? 1 : 0;
        resp[2] = (uint8_t)get_active_connections();
        resp[3] = get_setting_uint8(&global_settings.target_active_connections);
        resp_len = 4;
        break;
    }
    case CONTROL_OP_SAVE_SETTINGS: {
        bool ok = write_global_settings_to_nvs() && write_devices_settings_to_nvs();
        status = ok ? CONTROL_STATUS_OK : CONTROL_STATUS_ERR_INTERNAL;
        break;
    }
    case CONTROL_OP_GET_SECRETS: {
        memcpy(resp, PGP_CLONE_NAME, sizeof(PGP_CLONE_NAME));                                             // 16
        memcpy(resp + sizeof(PGP_CLONE_NAME), PGP_MAC, sizeof(PGP_MAC));                                  // 6
        memcpy(resp + sizeof(PGP_CLONE_NAME) + sizeof(PGP_MAC), PGP_DEVICE_KEY, sizeof(PGP_DEVICE_KEY));  // 16
        memcpy(resp + sizeof(PGP_CLONE_NAME) + sizeof(PGP_MAC) + sizeof(PGP_DEVICE_KEY),
            PGP_BLOB,
            sizeof(PGP_BLOB));                                                                            // 256
        resp_len = sizeof(PGP_CLONE_NAME) + sizeof(PGP_MAC) + sizeof(PGP_DEVICE_KEY) + sizeof(PGP_BLOB);  // 294
        break;
    }
    case CONTROL_OP_RESET_SECRETS: {
        status = reset_secrets() ? CONTROL_STATUS_OK : CONTROL_STATUS_ERR_INTERNAL;
        break;
    }
    case CONTROL_OP_RESTART: {
        pgp_control_send_response(gatts_if, conn_id, CONTROL_STATUS_OK, opcode, NULL, 0);
        fflush(stdout);
        esp_restart();
        return;  // unreachable
    }
    case CONTROL_OP_GET_LED_STATE: {
        resp[0] = get_led_advertising() ? 1 : 0;
        resp_len = 1;
        break;
    }
    case CONTROL_OP_CYCLE_LOG_LEVEL: {
        if (!cycle_log_level(&global_settings.log_level)) {
            status = CONTROL_STATUS_ERR_INTERNAL;
            break;
        }
        uint8_t log_level = get_setting_uint8(&global_settings.log_level);
        if (log_level == 3) {
            log_levels_verbose();
        } else if (log_level == 2) {
            log_levels_info();
        } else {
            log_levels_debug();
        }
        resp[0] = log_level;
        resp_len = 1;
        break;
    }
    case CONTROL_OP_GET_RUNTIME_STATS: {
        resp_len = stats_format_runtime((char*)resp, sizeof(resp));
        break;
    }
    case CONTROL_OP_GET_TASK_LIST: {
        // vTaskList requires a buffer sized for every task line; this size
        // is independent of the GATT response cap and must not shrink
        // below what vTaskList needs to write (AGENTS.md: no undersized
        // buffers on hot/stack paths). Kept at the same 1024 bytes uart.c's
        // 't' handler already uses.
        char task_buf[1024];
        vTaskList(task_buf);
        int n = snprintf((char*)resp, sizeof(resp), "%s\nHeap free: %lu bytes", task_buf, esp_get_free_heap_size());
        resp_len = (n > 0) ? (size_t)n : 0;
        break;
    }
    case CONTROL_OP_ADVERTISE_START: {
        pgp_advertise();
        break;
    }
    case CONTROL_OP_ADVERTISE_STOP: {
        pgp_advertise_stop();
        break;
    }
    case CONTROL_OP_GET_CLIENT_STATES: {
        resp_len = dump_client_states_format((char*)resp, sizeof(resp));
        break;
    }
    case CONTROL_OP_RESET_CLIENT_STATES: {
        reset_client_states();
        break;
    }
    case CONTROL_OP_SET_MAX_CONNECTIONS: {
        if (payload_len < 1 || payload[0] < 1 || payload[0] > CONFIG_BT_ACL_CONNECTIONS) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        if (!set_setting_uint8(&global_settings.target_active_connections, payload[0])) {
            status = CONTROL_STATUS_ERR_INTERNAL;
        }
        break;
    }
    case CONTROL_OP_TOGGLE_AUTOSPIN: {
        if (payload_len < 1 || payload[0] > 3) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        resp[0] = toggle_device_autospin(payload[0]) ? 1 : 0;
        resp_len = 1;
        break;
    }
    case CONTROL_OP_TOGGLE_AUTOCATCH: {
        if (payload_len < 1 || payload[0] > 3) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        resp[0] = toggle_device_autocatch(payload[0]) ? 1 : 0;
        resp_len = 1;
        break;
    }
    case CONTROL_OP_GET_CLIENT_SUMMARY: {
        size_t offset = 0;
        for (int i = 0; i < CONFIG_BT_ACL_CONNECTIONS; i++) {
            client_state_t* entry = get_client_state_entry_by_idx(i);
            uint16_t conn_id = entry ? entry->conn_id : 0xffff;
            uint8_t flags = 0;
            uint8_t autospin = 0;
            uint8_t autocatch = 0;
            Stats stats = { 0 };

            if (entry != NULL) {
                if (entry->settings != NULL) {
                    flags |= 0x01;
                    autospin = get_setting(&entry->settings->autospin) ? 1 : 0;
                    autocatch = get_setting(&entry->settings->autocatch) ? 1 : 0;
                }
                if (stats_get_for_conn(entry->conn_id, &stats)) {
                    flags |= 0x02;
                }
            }

            resp[offset++] = (uint8_t)(conn_id & 0xFF);
            resp[offset++] = (uint8_t)(conn_id >> 8);
            resp[offset++] = flags;
            resp[offset++] = autospin;
            resp[offset++] = autocatch;
            resp[offset++] = (uint8_t)(stats.caught & 0xFF);
            resp[offset++] = (uint8_t)(stats.caught >> 8);
            resp[offset++] = (uint8_t)(stats.fled & 0xFF);
            resp[offset++] = (uint8_t)(stats.fled >> 8);
            resp[offset++] = (uint8_t)(stats.spin & 0xFF);
            resp[offset++] = (uint8_t)(stats.spin >> 8);
        }
        resp_len = offset;
        break;
    }
    default:
        status = CONTROL_STATUS_ERR_UNKNOWN_OPCODE;
        break;
    }

    pgp_control_send_response(gatts_if, conn_id, status, opcode, resp, resp_len);
}

bool pgp_control_try_handle_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    if (control_handle_table[IDX_CHAR_CONTROL_COMMAND_VAL] == param->write.handle) {
        pgp_control_handle_command_write(gatts_if, param->write.conn_id, param->write.value, param->write.len);
        return true;
    }
    if (control_handle_table[IDX_CHAR_CONTROL_RESPONSE_CFG] == param->write.handle) {
        // Client toggling indications on/off — no action needed beyond the
        // CCCD write itself, which ESP_GATT_AUTO_RSP already handles.
        ESP_LOGD(CONTROL_TAG, "[%d] control response indicate CCCD write", param->write.conn_id);
        return true;
    }
    return false;
}
