#include "pgp_control.h"

#include "config_secrets.h"  // reset_secrets(), write_secrets()
#include "config_storage.h"  // write_global_settings_to_nvs, write_device(s)_settings_to_nvs
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "esp_system.h"       // esp_restart, esp_get_free_heap_size
#include "esp_timer.h"        // esp_timer_get_time
#include "forget_pairings.h"  // forget_pairings_and_restart
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // vTaskList
#include "fw_version.h"     // PGP_FW_VERSION
#include "known_devices.h"
#include "led_output.h"  // get_led_advertising
#include "lifetime_stats.h"
#include "log_tags.h"
#include "pgp_gap.h"              // advertise_if_needed, pgp_adv_get_state
#include "pgp_gatts.h"            // MAX_VALUE_LENGTH
#include "pgp_handshake_multi.h"  // dump_client_states_format, get_active_connections, reset_client_states
#include "pgp_ota.h"
#include "secrets.h"  // PGP_CLONE_NAME, PGP_MAC, PGP_DEVICE_KEY, PGP_BLOB
#include "settings.h"  // global_settings, get_setting*, set_setting_uint8, cycle_log_level, toggle_device_*, set_device_*
#include "stats.h"     // stats_format_runtime

#include <stdio.h>
#include <string.h>

#define CONTROL_CHAR_DECLARATION_SIZE (sizeof(uint8_t))
#define EVENT_FRAME_LEN 3
static const uint8_t CONTROL_INST_ID = 0;

static void apply_log_level(uint8_t level) {
    if (level == 3) {
        log_levels_verbose();
    } else if (level == 2) {
        log_levels_info();
    } else {
        log_levels_debug();
    }
}

// Persists only on change to spare flash; a failed write keeps the RAM value so the radio behaves as asked.
static control_status_t set_advertising_persisted(bool on) {
    bool old = get_setting(&global_settings.advertising_enabled);
    if (!set_setting_bool(&global_settings.advertising_enabled, on)) {
        return CONTROL_STATUS_ERR_INTERNAL;
    }
    return (old != on && !write_global_settings_to_nvs()) ? CONTROL_STATUS_ERR_INTERNAL : CONTROL_STATUS_OK;
}

static uint32_t get_u32_le(const uint8_t* in) {
    return (uint32_t)in[0] | (uint32_t)in[1] << 8 | (uint32_t)in[2] << 16 | (uint32_t)in[3] << 24;
}

static bool game_link_active(void) {
    for (int i = 0; i < CONFIG_BT_ACL_CONNECTIONS; i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->cert_state == PGP_CERT_DONE) {
            return true;
        }
    }
    return false;
}

static control_status_t known_devices_status(known_devices_result_t result) {
    switch (result) {
    case KNOWN_DEVICES_OK:
        return CONTROL_STATUS_OK;
    case KNOWN_DEVICES_NOT_FOUND:
        return CONTROL_STATUS_ERR_NOT_FOUND;
    case KNOWN_DEVICES_BUSY:
        return CONTROL_STATUS_ERR_BUSY;
    default:
        return CONTROL_STATUS_ERR_INTERNAL;
    }
}

static void put_u32_le(uint8_t* out, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        out[i] = (uint8_t)(v >> (8 * i));
    }
}

// Standard BLE SIG UUIDs — local aliases matching pgp_gatts.c's
// primary_service_uuid/character_declaration_uuid/character_client_config_uuid;
// not duplicated logic, just the same ESP-IDF constant under this module's
// own name so this table has no cross-file coupling to pgp_gatts.c internals.
static const uint16_t control_primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t control_char_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t control_char_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t control_char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t control_char_prop_indicate = ESP_GATT_CHAR_PROP_BIT_INDICATE;
static const uint8_t control_char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
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
static uint8_t GATTS_CHAR_UUID_CONTROL_EVENT[ESP_UUID_LEN_128] = { 0x43,
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

// ATT allows one unconfirmed indication per link; set on send, cleared by CONF_EVT or disconnect.
static bool indicate_in_flight[CONFIG_BT_ACL_CONNECTIONS];

// Events use their own unconfirmed characteristic so they never take the response's in-flight slot.
static bool event_notify[CONFIG_BT_ACL_CONNECTIONS];
static uint8_t event_seq[CONFIG_BT_ACL_CONNECTIONS];
static esp_gatt_if_t event_gatts_if;

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

    [IDX_CHAR_CONTROL_EVENT] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&control_char_declaration_uuid,
            ESP_GATT_PERM_READ,
            CONTROL_CHAR_DECLARATION_SIZE,
            CONTROL_CHAR_DECLARATION_SIZE,
            (uint8_t*)&control_char_prop_notify } },
    [IDX_CHAR_CONTROL_EVENT_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128,
            (uint8_t*)&GATTS_CHAR_UUID_CONTROL_EVENT,
            ESP_GATT_PERM_READ_ENCRYPTED,
            EVENT_FRAME_LEN,
            0,
            NULL } },
    [IDX_CHAR_CONTROL_EVENT_CFG] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16,
            (uint8_t*)&control_char_client_config_uuid,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED,
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

    if (conn_id >= CONFIG_BT_ACL_CONNECTIONS || indicate_in_flight[conn_id]) {
        ESP_LOGW(CONTROL_TAG, "[%d] response dropped, indication in flight", conn_id);
        return;
    }

    // Static: keeps 500 bytes off the BTC stack; safe because all writes run on BTC_TASK.
    static uint8_t frame[2 + CONTROL_MAX_RESPONSE_PAYLOAD];
    frame[0] = (uint8_t)status;
    frame[1] = opcode;
    if (payload_len > 0) {
        memcpy(frame + 2, payload, payload_len);
    }

    indicate_in_flight[conn_id] = true;
    // No CONF_EVT follows a send that never started, so free the slot on error.
    if (esp_ble_gatts_send_indicate(
            gatts_if, conn_id, control_handle_table[IDX_CHAR_CONTROL_RESPONSE_VAL], 2 + payload_len, frame, true)
        != ESP_OK) {
        indicate_in_flight[conn_id] = false;
    }
}

void pgp_control_on_conf(uint16_t conn_id) {
    if (conn_id < CONFIG_BT_ACL_CONNECTIONS) {
        indicate_in_flight[conn_id] = false;
    }
}

void pgp_control_emit_event(pgp_event_t type, uint16_t game_conn_id) {
    int slot = get_client_slot(game_conn_id);
    if (slot < 0) {
        return;
    }
    for (uint16_t conn = 0; conn < CONFIG_BT_ACL_CONNECTIONS; conn++) {
        if (!event_notify[conn]) {
            continue;
        }
        uint8_t frame[EVENT_FRAME_LEN] = { event_seq[conn]++, (uint8_t)type, (uint8_t)slot };
        // A lost send shows up as a seq gap in the app, which then refreshes.
        esp_ble_gatts_send_indicate(
            event_gatts_if, conn, control_handle_table[IDX_CHAR_CONTROL_EVENT_VAL], sizeof(frame), frame, false);
    }
}

void pgp_control_on_disconnect(uint16_t conn_id) {
    if (conn_id < CONFIG_BT_ACL_CONNECTIONS) {
        event_notify[conn_id] = false;
    }
    pgp_control_on_conf(conn_id);
    pgp_ota_on_disconnect(conn_id);
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

    // Static: keeps 498 bytes off the BTC stack; safe because all writes run on BTC_TASK.
    static uint8_t resp[CONTROL_MAX_RESPONSE_PAYLOAD];
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
        resp[4] = (uint8_t)pgp_adv_get_state();
        resp_len = 5;
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
    case CONTROL_OP_PROVISION_SECRETS: {
        // Same layout as GET_SECRETS: name[16] mac[6] dkey[16] blob[256].
        const size_t name_len = sizeof(PGP_CLONE_NAME), mac_off = name_len, key_off = mac_off + sizeof(PGP_MAC),
                     blob_off = key_off + sizeof(PGP_DEVICE_KEY);
        if (PGP_VALID()) {
            // Only an empty device is open: Control is Just Works, so a provisioned one must not be overwritable.
            status = CONTROL_STATUS_ERR_NOT_PROVISIONING;
            break;
        }
        if (payload_len != blob_off + sizeof(PGP_BLOB)) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        // read_secrets() reads the name into 16 bytes, so it needs a NUL inside them; a zero mac would re-enter
        // provisioning after the restart.
        size_t n = strnlen((const char*)payload, name_len);
        bool mac_zero = true;
        for (size_t i = 0; i < sizeof(PGP_MAC); i++) {
            mac_zero = mac_zero && payload[mac_off + i] == 0;
        }
        bool printable = n > 0 && n < name_len;
        for (size_t i = 0; i < n; i++) {
            printable = printable && payload[i] >= 0x20 && payload[i] <= 0x7E;
        }
        if (!printable || mac_zero) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        // RAM secrets stay empty: the app restarts the device next.
        status = write_secrets((const char*)payload, payload + mac_off, payload + key_off, payload + blob_off)
                     ? CONTROL_STATUS_OK
                     : CONTROL_STATUS_ERR_INTERNAL;
        break;
    }
    case CONTROL_OP_RESET_SECRETS: {
        status = reset_secrets() ? CONTROL_STATUS_OK : CONTROL_STATUS_ERR_INTERNAL;
        break;
    }
    case CONTROL_OP_RESTART: {
        pgp_control_send_response(gatts_if, conn_id, CONTROL_STATUS_OK, opcode, NULL, 0);
        lifetime_stats_flush_all();
        fflush(stdout);
        esp_restart();
        return;  // unreachable
    }
    case CONTROL_OP_FORGET_PAIRINGS: {
        pgp_control_send_response(gatts_if, conn_id, CONTROL_STATUS_OK, opcode, NULL, 0);
        forget_pairings_and_restart();
        return;  // unreachable
    }
    case CONTROL_OP_OTA_BEGIN: {
        if (payload_len != 4) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        // Flash writes stall the cache, which a live game link cannot afford.
        if (game_link_active()) {
            status = CONTROL_STATUS_ERR_BUSY;
            break;
        }
        client_state_t* entry = get_or_create_client_state_entry(conn_id);
        uint16_t chunk = 0;
        status = pgp_ota_begin(conn_id, get_u32_le(payload), entry ? entry->mtu : 0, &chunk);
        if (status == CONTROL_STATUS_OK) {
            resp[0] = (uint8_t)chunk;
            resp[1] = (uint8_t)(chunk >> 8);
            resp_len = 2;
        }
        break;
    }
    case CONTROL_OP_OTA_DATA: {
        uint32_t next = 0;
        if (payload_len < 5) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        status = pgp_ota_data(conn_id, get_u32_le(payload), payload + 4, payload_len - 4, &next);
        if (status == CONTROL_STATUS_OK) {
            put_u32_le(resp, next);
            resp_len = 4;
        }
        // No per-chunk log: this runs thousands of times per update.
        pgp_control_send_response(gatts_if, conn_id, status, opcode, resp, resp_len);
        return;
    }
    case CONTROL_OP_OTA_END: {
        status = pgp_ota_end(conn_id);
        break;
    }
    case CONTROL_OP_OTA_ABORT: {
        status = pgp_ota_abort(conn_id);
        break;
    }
    case CONTROL_OP_LIST_KNOWN_DEVICES: {
        status = known_devices_status(known_devices_list(resp, sizeof(resp), &resp_len));
        break;
    }
    case CONTROL_OP_RENAME_KNOWN_DEVICE: {
        // bda[6] then 0..15 printable ASCII bytes, like the clone name.
        bool printable = payload_len >= 6 && payload_len < 6 + KNOWN_DEVICE_NAME_LEN;
        for (size_t i = 6; printable && i < payload_len; i++) {
            printable = payload[i] >= 0x20 && payload[i] <= 0x7E;
        }
        status = printable
                     ? known_devices_status(known_devices_rename(payload, (const char*)payload + 6, payload_len - 6))
                     : CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
        break;
    }
    case CONTROL_OP_FORGET_KNOWN_DEVICE: {
        status = payload_len == 6 ? known_devices_status(known_devices_forget(payload))
                                  : CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
        break;
    }
    case CONTROL_OP_GET_LIFETIME_STATS: {
        // flags u8 (bit 0: connected now), lifetime 6 x u32 LE, session 6 x u16 LE
        if (payload_len != 6) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
        } else if (!known_devices_contains(payload)) {
            status = CONTROL_STATUS_ERR_NOT_FOUND;
        } else {
            uint32_t total[LIFETIME_STAT_COUNT];
            uint16_t session[LIFETIME_STAT_COUNT];
            bool connected;
            lifetime_stats_get(payload, total, session, &connected);
            resp[0] = connected ? 1 : 0;
            for (int i = 0; i < LIFETIME_STAT_COUNT; i++) {
                put_u32_le(resp + 1 + 4 * i, total[i]);
                resp[25 + 2 * i] = (uint8_t)(session[i] & 0xFF);
                resp[26 + 2 * i] = (uint8_t)(session[i] >> 8);
            }
            resp_len = 37;
        }
        break;
    }
    case CONTROL_OP_RESET_LIFETIME_STATS: {
        if (payload_len != 6) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
        } else if (!known_devices_contains(payload)) {
            status = CONTROL_STATUS_ERR_NOT_FOUND;
        } else if (!lifetime_stats_reset(payload)) {
            status = CONTROL_STATUS_ERR_INTERNAL;
        }
        break;
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
        apply_log_level(log_level);
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
        static char task_buf[1024];  // static: BTC stack is small, see resp
        vTaskList(task_buf);
        int n = snprintf((char*)resp, sizeof(resp), "%s\nHeap free: %lu bytes", task_buf, esp_get_free_heap_size());
        resp_len = (n > 0) ? (size_t)n : 0;
        break;
    }
    case CONTROL_OP_ADVERTISE_START: {
        status = set_advertising_persisted(true);
        advertise_if_needed();
        break;
    }
    case CONTROL_OP_ADVERTISE_STOP: {
        status = set_advertising_persisted(false);
        advertise_if_needed();
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
        uint8_t old = get_setting_uint8(&global_settings.target_active_connections);
        if (!set_setting_uint8(&global_settings.target_active_connections, payload[0])) {
            status = CONTROL_STATUS_ERR_INTERNAL;
        } else if (old != payload[0] && !write_global_settings_to_nvs()) {
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
    case CONTROL_OP_SET_AUTOSPIN:
    case CONTROL_OP_SET_AUTOCATCH: {
        // [slot][0|1]; an empty slot is a bad slot index, not an internal error.
        if (payload_len < 2 || payload[0] > 3 || payload[1] > 1) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        client_state_t* entry = get_client_state_entry_by_idx(payload[0]);
        if (entry == NULL || !entry->has_settings) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        bool val = payload[1] == 1;
        bool old = opcode == CONTROL_OP_SET_AUTOSPIN ? entry->settings.autospin : entry->settings.autocatch;
        bool ok = opcode == CONTROL_OP_SET_AUTOSPIN ? set_device_autospin(payload[0], val)
                                                    : set_device_autocatch(payload[0], val);
        // The new value stays applied on a failed write; the error says it will not survive a reboot.
        if (!ok || (old != val && !write_device_settings_to_nvs(payload[0]))) {
            status = CONTROL_STATUS_ERR_INTERNAL;
            break;
        }
        resp[0] = payload[1];
        resp_len = 1;
        break;
    }
    case CONTROL_OP_SET_LOG_LEVEL: {
        if (payload_len < 1 || payload[0] < 1 || payload[0] > 3) {
            status = CONTROL_STATUS_ERR_MALFORMED_PAYLOAD;
            break;
        }
        uint8_t old = get_setting_uint8(&global_settings.log_level);
        if (!set_setting_uint8(&global_settings.log_level, payload[0])) {
            status = CONTROL_STATUS_ERR_INTERNAL;
            break;
        }
        apply_log_level(payload[0]);
        if (old != payload[0] && !write_global_settings_to_nvs()) {
            status = CONTROL_STATUS_ERR_INTERNAL;
            break;
        }
        resp[0] = payload[0];
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
                if (entry->has_settings) {
                    flags |= 0x01;
                    autospin = get_setting(&entry->settings.autospin) ? 1 : 0;
                    autocatch = get_setting(&entry->settings.autocatch) ? 1 : 0;
                }
                if (stats_get_for_conn(entry->conn_id, &stats)) {
                    flags |= 0x02;
                }
                // Only the game link completes the handshake; the companion link is not a Device Profile.
                if (entry->cert_state == PGP_CERT_DONE) {
                    flags |= 0x04;
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
    case CONTROL_OP_GET_DEVICE_INFO: {
        // Fixed prefix, then the version string up to the end of the payload; a new field goes before it with a
        // CONTROL_PROTOCOL_VERSION bump.
        resp[0] = CONTROL_PROTOCOL_VERSION;
        resp[1] = CONFIG_BT_ACL_CONNECTIONS;
        resp[2] = (uint8_t)esp_reset_reason();
        put_u32_le(resp + 3, (uint32_t)(esp_timer_get_time() / 1000000));
        put_u32_le(resp + 7, esp_get_free_heap_size());
        put_u32_le(resp + 11, esp_get_minimum_free_heap_size());
        resp[15] = PGP_VALID() ? 0 : 0x01;  // bit 0: provisioning mode
        memcpy(resp + 16, PGP_FW_VERSION, sizeof(PGP_FW_VERSION) - 1);
        resp_len = 16 + sizeof(PGP_FW_VERSION) - 1;
        break;
    }
    default:
        status = CONTROL_STATUS_ERR_UNKNOWN_OPCODE;
        break;
    }

    pgp_control_send_response(gatts_if, conn_id, status, opcode, resp, resp_len);
    ESP_LOGD(CONTROL_TAG, "[%d] btc stack hwm=%u words", conn_id, (unsigned)uxTaskGetStackHighWaterMark(NULL));
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
    if (control_handle_table[IDX_CHAR_CONTROL_EVENT_CFG] == param->write.handle) {
        uint16_t conn = param->write.conn_id;
        if (conn < CONFIG_BT_ACL_CONNECTIONS && param->write.len >= 1) {
            bool on = (param->write.value[0] & 1) != 0;
            if (on && !event_notify[conn]) {
                event_seq[conn] = 0;
            }
            event_notify[conn] = on;
            event_gatts_if = gatts_if;
        }
        return true;
    }
    return false;
}
