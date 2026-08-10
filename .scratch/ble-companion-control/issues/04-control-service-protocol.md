# Design the Control Service GATT layout and wire-format protocol

Type: grilling
Status: resolved
Blocked by: 01

## Question

Define the Control Service: service UUID, Command/Response characteristic UUIDs and properties, and the opcode+payload wire format covering every existing `uart.c` command 1:1 (see investigation: `s`/`S`/`L`/`l`/`r`/`t`/`R`, `xs`/`xr`, `bA`/`ba`/`bs`/`br`/`b[1-4]`, `<idx>s`/`<idx>c`). For each: opcode value, request payload shape, response payload shape, and how ticket 01's fragmentation scheme applies to oversized responses (secrets blob, task list, stats). Include an error/status code convention (malformed opcode, busy, not-bonded).

## Answer

### Service / characteristics

- **Control Service** UUID: new 128-bit vendor UUID (pick one at implementation time, e.g. generated v4 UUID kept in `pgp_gatts.h` next to the existing service enums) — not a SIG-assigned 16-bit UUID, since this is proprietary.
- **Command characteristic**: `WRITE` property, permission `ESP_GATT_PERM_WRITE_ENCRYPTED` (per ticket 02: attribute-level encryption, doesn't touch existing unencrypted PGP characteristics).
- **Response characteristic**: `INDICATE` property (needs ack, not fire-and-forget notify — matches ticket 01's "tiny indicate as data-ready signal" pattern) + a backing `ESP_GATT_AUTO_RSP` value attribute for the actual payload, permission `ESP_GATT_PERM_READ_ENCRYPTED`. Client reads the value (auto-fragmented via ATT Read Blob per ticket 01) after the indicate fires.

### Wire format

Request (Command write):
```
[opcode: u8][payload: 0..N bytes, opcode-specific]
```

Response (value behind the indicate, per ticket 01's set-attr-value + indicate pattern):
```
[status: u8][opcode: u8][payload: 0..N bytes, opcode-specific]
```
Carrying the opcode back was the settled call — self-describing, cheap (1 byte), safe if a future version pipelines commands even though today's link is strictly one-command-at-a-time.

### Status codes

`OK = 0x00`, `ERR_UNKNOWN_OPCODE = 0x01`, `ERR_MALFORMED_PAYLOAD = 0x02`, `ERR_NOT_BONDED = 0x03` (defensive; GATT permission already blocks this at the stack level, but the opcode gives the app a clean error to show instead of a raw ATT error), `ERR_BUSY = 0x04` (command already in flight), `ERR_INTERNAL = 0x05` (e.g. `write_global_settings_to_nvs()` returned false).

### Opcode table

Direct 1:1 port of every `uart.c` command. Diagnostic dumps (`t`, `r`) ship as raw UTF-8 strings (today's format), not structured binary — smallest, most faithful port, zero new parsing code either side.

| Opcode | Name | Was | Request payload | Response payload |
|---|---|---|---|---|
| `0x01` | HELP | `?` | none | UTF-8 help string (same content as today's `?` log) |
| `0x02` | GET_GLOBAL_SETTINGS | `s` | none | `log_level:u8, advertising_enabled:u8(bool), active_connections:u8, target_active_connections:u8` |
| `0x03` | SAVE_SETTINGS | `S` | none | none (status only — `ERR_INTERNAL` if either NVS write fails, matching today's two-step log) |
| `0x04` | GET_SECRETS | `xs` | none | `clone_name:16B, mac:6B, device_key:16B, blob:256B` (fixed-size, matches `PGP_CLONE_NAME`/`PGP_MAC`/`PGP_DEVICE_KEY`/`PGP_BLOB`) |
| `0x05` | RESET_SECRETS | `xr` | none | none (status only) |
| `0x06` | RESTART | `R` | none | none (device restarts after sending status; app should expect the disconnect as confirmation) |
| `0x07` | GET_LED_STATE | `L` | none | `led_on:u8(bool)` |
| `0x08` | CYCLE_LOG_LEVEL | `l` | none | `new_log_level:u8` (1=debug/2=info/3=verbose, same cycle as today) |
| `0x09` | GET_RUNTIME_STATS | `r` | none | UTF-8 string, one line per connection (today's `---STATS---` format) — empty string if `stats_len == 0` |
| `0x0A` | GET_TASK_LIST | `t` | none | UTF-8 string: `vTaskList()` buffer + free-heap line (today's format) |
| `0x0B` | ADVERTISE_START | `bA` | none | none |
| `0x0C` | ADVERTISE_STOP | `ba` | none | none |
| `0x0D` | GET_CLIENT_STATES | `bs` | none | UTF-8 string (today's `dump_client_states()` log format — active_connections, conn_id_map, per-client state incl. hex nonces/keys) |
| `0x0E` | RESET_CLIENT_STATES | `br` | none | none (triggers disconnects, matching today) |
| `0x0F` | SET_MAX_CONNECTIONS | `b[1-4]` | `count:u8` (1-4, validated against `CONFIG_BT_ACL_CONNECTIONS`) | none |
| `0x10` | TOGGLE_AUTOSPIN | `[0-3]s` | `device_idx:u8` (0-3) | `new_value:u8(bool)` |
| `0x11` | TOGGLE_AUTOCATCH | `[0-3]c` | `device_idx:u8` (0-3) | `new_value:u8(bool)` |

### Notes carried from other tickets

- Oversized responses (GET_SECRETS at 280 bytes, GET_RUNTIME_STATS/GET_TASK_LIST/GET_CLIENT_STATES variable-length strings) use ticket 01's pattern: `esp_ble_gatts_set_attr_value()` on the Response characteristic's value, then a tiny `esp_ble_gatts_send_indicate()`; the stack's ATT Read Blob handles reassembly up to `GATT_MAX_ATTR_LEN` (512B) — GET_SECRETS' 280-byte payload and the string dumps must be capped/truncated at that ceiling if they'd exceed it (task list buffer is already capped at 1024B today — needs re-checking against the new 512B ceiling, flagged for the implementation pass, not blocking this spec).
- `ERR_NOT_BONDED` is defensive since `ESP_GATT_PERM_*_ENCRYPTED` (ticket 02) already refuses the operation at the GATT layer before firmware code runs; kept for a clean app-facing error path only if bonding drops mid-command.
