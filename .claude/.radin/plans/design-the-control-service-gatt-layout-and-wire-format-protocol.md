# Plan: Control Service GATT layout and wire-format protocol

Source entry: `design-the-control-service-gatt-layout-and-wire-format-protocol`
(resolved design in `.claude/.radin/backlog/tasks/design-the-control-service-gatt-layout-and-wire-format-protocol.md`,
full rationale in `.scratch/ble-companion-control/issues/04-control-service-protocol.md`).
Depends on ticket 01 (`.scratch/ble-companion-control/issues/01-research-mtu-limits.md`,
already resolved, no code changed) — its read-blob + tiny-indicate pattern
(`pgp_handshake.c:52-114`) is the precedent this plan follows exactly.

This plan is implementation-ready: every value, file, and function signature
below is final. The executor makes no judgment calls.

## Design note: table ownership stays inside `pgp_control.c`, not `pgp_gatts.c`

`pgp_gatts.c` is already 867 lines and holds all three existing services'
GATT tables inline. Bolting a fourth table + its UUIDs + its
`ESP_GATTS_CREAT_ATTR_TAB_EVT` branch into that file the same way would add
~150-200 lines and push it past 1000 — and it would be the wrong owner
anyway: this plan is already creating `pgp_control.c` as the
single-responsibility home for the Control Service's protocol, so its GATT
table (plumbing that only this service needs) belongs there too, not
smeared across `pgp_gatts.c`.

`pgp_gatts.c` keeps its existing role: own the shared connection/GATTS
profile event loop, and delegate to per-service modules at each hook —
exactly the shape it already uses for `pgp_handshake.c`
(`handle_pgp_handshake_first`/`handle_pgp_handshake_second`,
`pgp_gatts.c:611-613`). `pgp_control.c` becomes a peer to `pgp_handshake.c`,
owning its own table, its own handle table, and its own attr-tab-created and
write-event handling — `pgp_gatts.c` only gains three one-line delegation
calls at its existing `ESP_GATTS_REG_EVT` / `ESP_GATTS_CREAT_ATTR_TAB_EVT` /
`ESP_GATTS_WRITE_EVT` hooks. Net growth to `pgp_gatts.c`: ~12 lines.

## Files touched

New:
- `pgpemu-esp32/main/pgp_control.h`
- `pgpemu-esp32/main/pgp_control.c`
- `pgpemu-esp32/main/pc/test_pgp_control_protocol.c`

Extended (small, delegation-only changes — see design note above):
- `pgpemu-esp32/main/pgp_gatts.h` — `MAX_VALUE_LENGTH` moved here (public)
- `pgpemu-esp32/main/pgp_gatts.c` — three delegation calls, no new table
- `pgpemu-esp32/main/pgp_gatts_debug.c` — control characteristic names for the debug handle dump
- `pgpemu-esp32/main/stats.h`, `pgpemu-esp32/main/stats.c` — one new formatter function (pure addition)
- `pgpemu-esp32/main/pgp_handshake_multi.h`, `pgpemu-esp32/main/pgp_handshake_multi.c` — one new formatter function (pure addition)
- `pgpemu-esp32/main/log_tags.h` — one new tag

No `CMakeLists.txt` change needed: `main/CMakeLists.txt` globs `main/*.c`
non-recursively, so `pgp_control.c` is picked up automatically and `pc/`
stays excluded from the firmware build, matching today's layout.

---

## 1. `pgp_gatts.h` changes

Move the existing `#define MAX_VALUE_LENGTH 500` from `pgp_gatts.c:36` into
`pgp_gatts.h` (it becomes the one shared source of truth for the GATT
attribute-value ceiling; `pgp_control.c` needs it too). Leave every other
use of it in `pgp_gatts.c` untouched — only the `#define` site moves.
No enum or handle table is added here — those live in `pgp_control.h`
(see §5).

## 2. `pgp_gatts.c` changes — three delegation calls only

Add `#include "pgp_control.h"` to the includes.

### 2.1 `ESP_GATTS_REG_EVT` — table creation

After the existing certificate `esp_ble_gatts_create_attr_tab` call
(`pgp_gatts.c:567-570`), add:

```c
pgp_control_create_attr_table(gatts_if);
```

(`pgp_control_create_attr_table` owns its own error logging internally —
see §5 — so no `create_attr_ret` handling needed here.)

### 2.2 `ESP_GATTS_CREAT_ATTR_TAB_EVT`

After the existing `if (param->add_attr_tab.svc_uuid.len == ESP_UUID_LEN_128) { ... }`
block (`pgp_gatts.c:775-799`), which ends by setting `found = 1` for a
matched service:

```c
if (!found) {
    found = pgp_control_handle_attr_tab_created(param);
}
```

### 2.3 `ESP_GATTS_WRITE_EVT`

Add one delegation branch before the final `else` (unknown handle) in the
handle-comparison chain at `pgp_gatts.c:609-621`:

```c
} else if (pgp_control_try_handle_write(gatts_if, param)) {
    // handled inside pgp_control.c
```

This mirrors the existing shape exactly (`handle_pgp_handshake_first`,
`handle_pgp_handshake_second`, `handle_led_notify_from_app` are all called
the same way from this chain) — no new branching pattern introduced.

That is the entire `pgp_gatts.c` diff: three call sites, ~12 lines total.

## 3. `pgp_gatts_debug.c` changes

`char_name_from_handle` is a shared debug helper called for every GATT
event regardless of service (`pgp_gatts.c` logs it on every `READ_EVT`/
`WRITE_EVT`) — it must know about Control Service handles too, or those
events log `<UNKNOWN HANDLE NAME>`. Add `#include "pgp_control.h"` and,
mirroring `cert_char_names`:

```c
static const char* control_char_names[] = { "CONTROL_SVC",
    "CHAR_CONTROL_COMMAND",
    "CHAR_CONTROL_COMMAND_VAL",
    "CHAR_CONTROL_RESPONSE",
    "CHAR_CONTROL_RESPONSE_VAL",
    "CHAR_CONTROL_RESPONSE_CFG" };
```

One more `find_handle_index` lookup in `char_name_from_handle`, after the
certificate one:

```c
idx = find_handle_index(handle, control_handle_table, CONTROL_LAST_IDX);
if (idx >= 0)
    return control_char_names[idx];
```

(`control_handle_table`/`CONTROL_LAST_IDX` come from `pgp_control.h`, §5.)

## 4. `log_tags.h` change

Add one line, alphabetically placed:

```c
static const char CONTROL_TAG[] = "pgp_control";
```

## 5. New module `pgp_control.h` / `pgp_control.c`

Single-responsibility module owning the Control Service end-to-end: its
GATT table, its handle table, and its opcode+payload wire protocol — the
same shape `pgp_handshake.c` already has for the Certificate service's
handshake logic (`pgp_gatts.c` only routes events by handle; each service
module owns everything below that).

### 5.1 `pgp_control.h`

```c
#ifndef PGP_CONTROL_H
#define PGP_CONTROL_H

#include "esp_gatts_api.h"

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
```

`CONTROL_STATUS_ERR_NOT_BONDED` is declared but never returned by any case
below — defensive/app-facing only, per the entry's own note (the GATT
permission bits already block an unencrypted write before this code runs).
`CONTROL_STATUS_ERR_BUSY` is likewise declared but unused: this protocol is
strictly one-command-at-a-time and synchronous (single BLE task context, no
concurrent command state to be busy on) — reserved for future pipelining,
never returned today.

### 5.2 `pgp_control.c` — GATT table (mirrors `pgp_gatts.c`'s existing table shape)

```c
#include "pgp_control.h"

#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "log_tags.h"
#include "pgp_gatts.h" // MAX_VALUE_LENGTH

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
static uint8_t GATTS_SERVICE_UUID_CONTROL[ESP_UUID_LEN_128] = { 0x40, 0x8e, 0x0d, 0xef, 0x8e, 0x8b, 0x7f, 0xab, 0x33, 0x44, 0x89, 0x5b, 0x09, 0x77, 0xe8, 0xbb };
static uint8_t GATTS_CHAR_UUID_CONTROL_COMMAND[ESP_UUID_LEN_128] = { 0x41, 0x8e, 0x0d, 0xef, 0x8e, 0x8b, 0x7f, 0xab, 0x33, 0x44, 0x89, 0x5b, 0x09, 0x77, 0xe8, 0xbb };
static uint8_t GATTS_CHAR_UUID_CONTROL_RESPONSE[ESP_UUID_LEN_128] = { 0x42, 0x8e, 0x0d, 0xef, 0x8e, 0x8b, 0x7f, 0xab, 0x33, 0x44, 0x89, 0x5b, 0x09, 0x77, 0xe8, 0xbb };

uint16_t control_handle_table[CONTROL_LAST_IDX];
static uint8_t control_response_buffer[MAX_VALUE_LENGTH]; // {0x00,0x00} = status OK, opcode 0 until first response

static const esp_gatts_attr_db_t gatt_db_control[CONTROL_LAST_IDX] = {
    [IDX_CONTROL_SVC] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16, (uint8_t*)&control_primary_service_uuid, ESP_GATT_PERM_READ,
          ESP_UUID_LEN_128, ESP_UUID_LEN_128, (uint8_t*)&GATTS_SERVICE_UUID_CONTROL } },

    [IDX_CHAR_CONTROL_COMMAND] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16, (uint8_t*)&control_char_declaration_uuid, ESP_GATT_PERM_READ,
          CONTROL_CHAR_DECLARATION_SIZE, CONTROL_CHAR_DECLARATION_SIZE, (uint8_t*)&control_char_prop_write } },
    [IDX_CHAR_CONTROL_COMMAND_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128, (uint8_t*)&GATTS_CHAR_UUID_CONTROL_COMMAND, ESP_GATT_PERM_WRITE_ENCRYPTED,
          MAX_VALUE_LENGTH, 0, NULL } }, // write-only, never read back — no backing value needed

    [IDX_CHAR_CONTROL_RESPONSE] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16, (uint8_t*)&control_char_declaration_uuid, ESP_GATT_PERM_READ,
          CONTROL_CHAR_DECLARATION_SIZE, CONTROL_CHAR_DECLARATION_SIZE, (uint8_t*)&control_char_prop_indicate } },
    [IDX_CHAR_CONTROL_RESPONSE_VAL] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_128, (uint8_t*)&GATTS_CHAR_UUID_CONTROL_RESPONSE, ESP_GATT_PERM_READ_ENCRYPTED,
          MAX_VALUE_LENGTH, 2, (uint8_t*)control_response_buffer } },

    /* Client Characteristic Configuration Descriptor, same role as pgp_gatts.c's IDX_CHAR_SFIDA_COMMANDS_CFG */
    [IDX_CHAR_CONTROL_RESPONSE_CFG] = { { ESP_GATT_AUTO_RSP },
        { ESP_UUID_LEN_16, (uint8_t*)&control_char_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
          sizeof(uint16_t), sizeof(control_cccd_initial_value), (uint8_t*)control_cccd_initial_value } },
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
```

### 5.3 `pgp_control.c` — write dispatch and wire protocol

```c
#include "config_secrets.h"      // reset_secrets()
#include "config_storage.h"      // write_global_settings_to_nvs, write_devices_settings_to_nvs
#include "esp_system.h"          // esp_restart, esp_get_free_heap_size
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"       // vTaskList
#include "led_output.h"          // get_led_advertising
#include "pgp_gap.h"             // pgp_advertise, pgp_advertise_stop
#include "pgp_handshake_multi.h" // dump_client_states_format, get_active_connections, reset_client_states
#include "secrets.h"             // PGP_CLONE_NAME, PGP_MAC, PGP_DEVICE_KEY, PGP_BLOB
#include "settings.h"             // global_settings, get_setting*, set_setting_uint8, cycle_log_level, toggle_device_*
#include "stats.h"                // stats_format_runtime

#include <stdio.h>

static const uint8_t CONTROL_RESPONSE_READY[1] = { 0x01 };

static void pgp_control_send_response(esp_gatt_if_t gatts_if, uint16_t conn_id, control_status_t status,
    uint8_t opcode, const uint8_t* payload, size_t payload_len) {
    if (payload_len > CONTROL_MAX_RESPONSE_PAYLOAD) {
        ESP_LOGW(CONTROL_TAG, "[%d] response payload %d truncated to %d", conn_id, (int)payload_len,
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
    esp_ble_gatts_send_indicate(gatts_if, conn_id, control_handle_table[IDX_CHAR_CONTROL_RESPONSE_VAL],
        sizeof(CONTROL_RESPONSE_READY), (uint8_t*)CONTROL_RESPONSE_READY, false);
}

static void pgp_control_handle_command_write(esp_gatt_if_t gatts_if, uint16_t conn_id, const uint8_t* value, uint16_t len) {
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
        int n = snprintf((char*)resp, sizeof(resp),
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
            PGP_CLONE_NAME, CONFIG_BT_ACL_CONNECTIONS, get_setting_uint8(&global_settings.target_active_connections));
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
        memcpy(resp + sizeof(PGP_CLONE_NAME) + sizeof(PGP_MAC) + sizeof(PGP_DEVICE_KEY), PGP_BLOB, sizeof(PGP_BLOB)); // 256
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
        return; // unreachable
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
```

Notes for the executor:
- `resp[CONTROL_MAX_RESPONSE_PAYLOAD]` (498 bytes) is a stack buffer inside a
  single non-recursive function called from the Bluedroid callback task —
  same order of magnitude as the stack buffers `uart.c` already uses on its
  own task, no dynamic allocation anywhere in this module (AGENTS.md: no
  dynamic allocation in hot paths).
- `pgp_control_try_handle_write` only fires from the write-event chain when
  `param->write.is_prep` is false, matching the existing chain's own
  guard in `pgp_gatts.c` (`pgp_gatts.c:592`) — no separate check needed here.
- `CONTROL_OP_RESTART` sends its own response directly instead of falling
  through to the shared call at the bottom, because the device restarts
  immediately after — it must respond *before* `esp_restart()`, matching
  the entry's note that the app should expect the disconnect as
  confirmation.
- `CONTROL_OP_HELP`'s string literal is intentionally duplicated from
  `uart.c`'s `'?'` case rather than shared: it is a single `snprintf` call
  with no owning module (the string isn't backed by any static/private
  state), so extracting a shared helper for one format string is not
  justified. If the help text changes, both copies must be updated —
  acceptable, single point of divergence risk for a static string.
- Every opcode with a fixed-size response (`0x02`, `0x04`, `0x07`, `0x08`,
  `0x0F`, `0x10`, `0x11`) writes at most 294 bytes (`GET_SECRETS`), safely
  under the 498-byte cap — no truncation risk for these.
- Every opcode with a variable-length UTF-8 string response (`0x01`, `0x09`,
  `0x0A`, `0x0D`) can exceed the cap under enough connections/tasks;
  `pgp_control_send_response` truncates and logs a warning uniformly for
  all of them — this is the resolution to the entry's flagged "task-list
  buffer vs. 512B ceiling" concern, generalized to every string response
  rather than special-cased to just the task list.

## 6. `stats.h` / `stats.c` — new formatter (pure addition)

`stats[]`/`stats_len` are file-static in `stats.c`; `pgp_control.c` cannot
reach them directly, so a formatting entry point is required. Add it
alongside the existing `stats_get_runtime()` without modifying that
function — same array, a second read-only loop, zero risk to the existing
logged output.

`stats.h`: add

```c
size_t stats_format_runtime(char* buf, size_t buf_len);
```

`stats.c`: add

```c
size_t stats_format_runtime(char* buf, size_t buf_len) {
    if (stats_len == 0) {
        return 0;
    }

    size_t offset = 0;
    for (size_t i = 0; i < stats_len && offset < buf_len; i++) {
        int n = snprintf(buf + offset, buf_len - offset,
            "---STATS---\n"
            "Connection %d:\n"
            "- Caught: %d\n"
            "- Fled: %d\n"
            "- Spin: %d\n",
            stats[i].conn_id, stats[i].stats.caught, stats[i].stats.fled, stats[i].stats.spin);
        if (n < 0) {
            break;
        }
        offset += (size_t)n;
    }
    return offset > buf_len ? buf_len : offset;
}
```

## 7. `pgp_handshake_multi.h` / `pgp_handshake_multi.c` — new formatter (pure addition)

Same reasoning as stats: `conn_id_map`/`client_states`/`active_connections`
are file-static. `ESP_LOG_BUFFER_HEX` is log-only (writes straight to the
log sink, cannot capture to a buffer), so the formatter hex-encodes each
key/nonce field itself with `snprintf("%02x", ...)` per byte instead —
same fields, same order, as `dump_client_state()`'s `ESP_LOG_BUFFER_HEX`
calls (`pgp_handshake_multi.c:238-251`).

`pgp_handshake_multi.h`: add

```c
size_t dump_client_states_format(char* buf, size_t buf_len);
```

`pgp_handshake_multi.c`: add (near `dump_client_states()`)

```c
static size_t append_hex(char* buf, size_t buf_len, size_t offset, const char* label, const uint8_t* data, size_t len) {
    int n = snprintf(buf + offset, buf_len > offset ? buf_len - offset : 0, "%s: ", label);
    offset += (n > 0) ? (size_t)n : 0;
    for (size_t i = 0; i < len && offset < buf_len; i++) {
        n = snprintf(buf + offset, buf_len - offset, "%02x", data[i]);
        offset += (n > 0) ? (size_t)n : 0;
    }
    n = snprintf(buf + offset, buf_len > offset ? buf_len - offset : 0, "\n");
    offset += (n > 0) ? (size_t)n : 0;
    return offset;
}

size_t dump_client_states_format(char* buf, size_t buf_len) {
    size_t offset = 0;
    int n = snprintf(buf, buf_len, "active_connections: %d\nconn_id_map:\n", active_connections);
    offset += (n > 0) ? (size_t)n : 0;

    for (int i = 0; i < MAX_CONNECTIONS && offset < buf_len; i++) {
        n = snprintf(buf + offset, buf_len - offset, "%d: %04x\n", i, conn_id_map[i]);
        offset += (n > 0) ? (size_t)n : 0;
    }

    n = snprintf(buf + offset, buf_len > offset ? buf_len - offset : 0, "client_states:\n");
    offset += (n > 0) ? (size_t)n : 0;

    for (int i = 0; i < MAX_CONNECTIONS && offset < buf_len; i++) {
        client_state_t* entry = &client_states[i];
        n = snprintf(buf + offset, buf_len - offset, "[%d] conn_id=%d cert_state=%d\n", i, entry->conn_id, entry->cert_state);
        offset += (n > 0) ? (size_t)n : 0;
        offset = append_hex(buf, buf_len, offset, "state_0_nonce", entry->state_0_nonce, sizeof(entry->state_0_nonce));
        offset = append_hex(buf, buf_len, offset, "the_challenge", entry->the_challenge, sizeof(entry->the_challenge));
        offset = append_hex(buf, buf_len, offset, "main_nonce", entry->main_nonce, sizeof(entry->main_nonce));
        offset = append_hex(buf, buf_len, offset, "outer_nonce", entry->outer_nonce, sizeof(entry->outer_nonce));
        offset = append_hex(buf, buf_len, offset, "session_key", entry->session_key, sizeof(entry->session_key));
        offset = append_hex(buf, buf_len, offset, "reconnect_challenge", entry->reconnect_challenge, sizeof(entry->reconnect_challenge));
    }
    return offset > buf_len ? buf_len : offset;
}
```

`pgp_control.c` passes a 498-byte buffer (`resp`/`sizeof(resp)`) into this;
`pgp_control_send_response` truncates further only if somehow still over
cap (it isn't, since `buf_len` is already the cap) — the two truncation
points agree because both use the same `CONTROL_MAX_RESPONSE_PAYLOAD`.

## 8. New test: `pc/test_pgp_control_protocol.c`

Follows this codebase's existing PC-test convention exactly (self-contained
single file, no `#include` of the real `.c` files — confirmed pattern in
`pc/test_settings.c` and `pc/test_config_storage.c`; `run_tests.sh`
compiles each `test_*.c` alone with `gcc -std=c99`, no other translation
units). Duplicate only the pure wire-format logic under test — the
opcode/status enum values and the `[status][opcode][payload]` framing and
truncation rule — not any ESP-IDF/BLE calls.

Assertions (via `assert.h`, `main()` returns 0 on success — same shape as
the other `pc/test_*.c` files):
1. Opcode constants match the table: `0x01` through `0x11`, no gaps, no
   duplicates.
2. Status constants match: `OK=0x00` .. `ERR_INTERNAL=0x05`.
3. A response-frame builder (`build_frame(status, opcode, payload, len, out)`
   mirroring `pgp_control_send_response`'s `[status][opcode][payload]`
   layout) produces the right byte layout for a 0-length payload and a
   payload at exactly `CONTROL_MAX_RESPONSE_PAYLOAD` (498) bytes.
4. The same builder truncates a payload of `CONTROL_MAX_RESPONSE_PAYLOAD +
   50` bytes down to exactly `CONTROL_MAX_RESPONSE_PAYLOAD`, and the frame
   is still exactly `2 + CONTROL_MAX_RESPONSE_PAYLOAD` bytes total (never
   exceeds `MAX_VALUE_LENGTH` = 500).
5. A request parser (`parse_request(value, len, &opcode, &payload, &payload_len)`
   mirroring `pgp_control_handle_command_write`'s split of `value[0]` /
   `value+1`) rejects `len == 0` and correctly splits `len >= 1`.

## 9. Verification

1. `make format` (per AGENTS.md — the only build-adjacent command this
   plan's executor may run).
2. `./run_tests.sh test_pgp_control_protocol` — new test passes.
3. `./run_tests.sh` — full PC suite still passes (confirms the two pure
   additions to `stats.c`/`pgp_handshake_multi.c` didn't break existing
   PC-testable code; note neither new formatter is itself PC-testable today
   since `stats.c`/`pgp_handshake_multi.c` depend on FreeRTOS/BLE types not
   mocked in any existing `pc/test_*.c` — out of scope for this plan, no PC
   test exists for `dump_client_states()`/`stats_get_runtime()` either).
4. Do not run `idf.py build` or any esp-idf command — out of scope per
   AGENTS.md, left to the maintainer.

No code is written by this plan — it is handed to `radin-execute` or a
human to implement against.
