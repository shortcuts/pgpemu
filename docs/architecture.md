# Architecture

Single-responsibility firmware modules under `pgpemu-esp32/main/`. BLE, storage, timing, and game-protocol logic stay isolated. Module list: `AGENTS.md`, "Key Modules". Domain terms: `CONTEXT.md`.

## Layers

1. **HAL**: `button_input.c`, `setup_button.c`, `led_output.c`: GPIO only.
2. **BLE transport**: `pgp_bluetooth.c`, `pgp_gap.c`, `pgp_gatts.c`.
3. **Connection and protocol**: `pgp_handshake_multi.c`, `pgp_handshake.c`, `pgp_cert.c`, `pgp_control.c`.
4. **Storage**: `config_storage.c`, `config_secrets.c`, `known_devices.c`, `nvs_helper.c`, `settings.c`.
5. **Features**: `pgp_led_handler.c`, `pgp_autobutton.c`.
6. **System**: `pgpemu.c`, `stats.c`, `log_tags.c`.

## Connection flow

1. Game connects over GATT (`pgp_gatts.c`); `pgp_handshake_multi.c` maps `conn_id` to a Device Profile (max 4).
2. First connection: full handshake with passkey, then session key persisted to NVS by MAC.
3. Reconnect: cached session key, expedited handshake, no passkey.
   The cert characteristic read is answered per connection from its own buffer (`pgp_cert_read_slice`), never from the shared attribute value.
   If the client disconnects mid-reconnect (reconnect `cert_state`, reason peer-user), the device clears the cached session, so the next connection runs a full handshake. This rule is unproven against a capture.
4. LED patterns (`pgp_led_handler.c`) trigger `pgp_autobutton.c` per the profile's settings.

## Control Service

Separate custom GATT service for the Companion App. One Command characteristic (opcode + payload) and one Response characteristic (confirmed indications, one in flight per connection). Requires an encrypted, bonded link. Handler: `pgp_control.c`. The opcodes, status codes, UUIDs and reply bytes are golden vectors in `protocol/control_vectors.txt`, read by both the firmware and app tests.

Event characteristic (UUID lead byte `0x43`, notify, unconfirmed, protocol version 7) carries Device Events, so they never take the Response characteristic's in-flight slot and cannot complete or drop a command response. Frame: `[seq u8][type u8][slot u8]`. `seq` is per connection, starts at 0 when that connection enables the CCCD, and wraps at 256; the app treats a jump as a lost event and runs a full refresh. `slot` is the Device Profile index (the `client_states[]` index, `get_client_slot()`). Types: 1 GAME_CONNECTED, 2 GAME_DISCONNECTED, 3 POKEMON_IN_RANGE, 4 NEW_POKEMON_IN_RANGE, 5 POKESTOP_IN_RANGE, 6 CAUGHT, 7 FLED, 8 ITEMS_RECEIVED, 9 BAG_FULL, 10 BOX_FULL, 11 BALLS_EMPTY. `pgp_control_emit_event()` sends to each link that enabled notifications and is called only from the BTC task: the LED write handler, the two `PGP_CERT_DONE` sites in `pgp_handshake.c`, and the disconnect event (before the connection entry is deleted). The CCCD needs an encrypted link. The app polls every 30 s instead of 5 s on protocol 7 and refreshes on events that change counters or connections.

GET_DEVICE_INFO (0x13) returns a little-endian record: protocol version (u8, `CONTROL_PROTOCOL_VERSION`), `CONFIG_BT_ACL_CONNECTIONS` (u8), `esp_reset_reason()` (u8), uptime in seconds (u32), free heap (u32), minimum free heap (u32), flags (u8, bit 0 = Provisioning Mode), then the firmware version (`PGP_FW_VERSION` in `fw_version.h`, kept current by release-please) as UTF-8 to the end of the payload. New fields append before the string only with a protocol version bump. The app reads it once per connection. A device that answers ERR_UNKNOWN_OPCODE or reports a protocol version below the app's minimum gets the "update firmware" message.

SET_AUTOSPIN (0x14) and SET_AUTOCATCH (0x15) take `[slot][0|1]`. SET_LOG_LEVEL (0x16) takes `[1-3]`. Each replies with the stored value (one byte), so a repeated frame cannot flip it back. A short payload, a slot above 3, a value out of range, or a slot without a Device Profile returns ERR_MALFORMED_PAYLOAD and changes nothing. ADVERTISE_START and ADVERTISE_STOP are the idempotent advertising setters. SET_LOG_LEVEL, SET_AUTOSPIN, SET_AUTOCATCH, SET_MAX_CONNECTIONS, ADVERTISE_START and ADVERTISE_STOP write the change to NVS before they reply, only when the value changed (the GPIO 9 button does the same). A failed write keeps the value in RAM and replies ERR_INTERNAL. SAVE_SETTINGS (0x03) is legacy and stays for older apps. The app needs protocol version 4. The legacy TOGGLE_AUTOSPIN (0x10), TOGGLE_AUTOCATCH (0x11) and CYCLE_LOG_LEVEL (0x08) stay for one release, then go away together with HELP.

FORGET_PAIRINGS (0x18) takes no payload and replies OK, then restarts. The handler only sets an `RTC_NOINIT` magic word and calls `esp_restart()`. `forget_pairings_if_requested()` runs first in `app_main()`, before anything opens the default `nvs` partition. It erases `nvs` only when `esp_reset_reason()` is a software reset and the magic matches, and it clears the magic before the erase so a crash cannot loop. `nvs` holds bonds, session keys and settings; `pgpsecret` is untouched, so the clone identity stays. Holding the board button (GPIO 9) for 10 s while the device runs does the same. It is not a power-up gesture: GPIO 9 is a strapping pin, and held low at reset the ROM enters download mode.

OTA_BEGIN (0x19), OTA_DATA (0x1A), OTA_END (0x1B) and OTA_ABORT (0x1C) stream a firmware image into the inactive OTA slot (`pgp_ota.c`, protocol version 5). The same Command and Response characteristics carry them, so each write waits for its confirmed indication; that is the flow control. OTA_BEGIN takes `[total_size u32]` and replies `[chunk_size u16]`, `min(mtu - 8, 480)` from the negotiated MTU. A chunk below 64 bytes (a link stuck at MTU 23) returns ERR_OTA_STATE (0x08). OTA_DATA takes `[offset u32][bytes]` and replies `[next_offset u32]`. The offset must equal the device's expected one, so a repeated or lost chunk returns ERR_OTA_STATE and writes nothing. OTA_END needs every byte, then `esp_ota_end` checks magic, checksum, SHA-256 and chip ID, and `pgp_ota.c` requires the app descriptor's `project_name` to be `pgpemu` before `esp_ota_set_boot_partition`. OTA_END does not restart: the app sends RESTART next. OTA_ABORT (owner only), an error, or the owner's disconnect drops the session. OTA_BEGIN also returns ERR_BUSY (0x04) while a game link is up (`cert_state == PGP_CERT_DONE`), because flash writes stall the cache.

LIST_KNOWN_DEVICES (0x1D), RENAME_KNOWN_DEVICE (0x1E) and FORGET_KNOWN_DEVICE (0x1F) expose the Known Device index (`known_devices.c`, protocol version 6). The index lives in the `known_devs` namespace of the `nvs` partition as 15 blobs `k00`..`k14` (`CONFIG_BT_SMP_MAX_BONDS`, the most bonds Bluedroid holds). A blob is 26 bytes: `bda[6]`, `seq` (u32 LE), `name[16]`. An all-zero `bda` marks a free slot. The device has no clock, so `seq` is a counter (highest in the table plus 1) and orders records by recency only. A record is touched when a handshake reaches `PGP_CERT_DONE`, at both sites in `pgp_handshake.c`. The Companion App link never completes a handshake, so it is never indexed. A touch writes flash only for a new record or one that is not already the newest. A new address in a full index evicts the least recently seen record that is not connected: `erase_device_data()` removes its session keys and Device Settings, `esp_ble_remove_bond_device()` removes its bond. LIST replies `count` (u8), then `count` records of 27 bytes: `bda[6]`, `seq` (u32 LE), flags (u8, bit 0 = connected now), `name[16]` NUL-padded, newest first (at most 406 bytes). RENAME takes `bda[6]` plus 0 to 15 printable ASCII bytes; an empty name clears it. FORGET takes `bda[6]`, erases the keys and settings, removes the bond, then drops the record; a failed erase keeps the record so a retry works. Errors: wrong length or a name byte outside 0x20 to 0x7E returns ERR_MALFORMED_PAYLOAD, an unknown address returns ERR_NOT_FOUND (0x09), forgetting a connected address returns ERR_BUSY, an NVS failure returns ERR_INTERNAL. Phones paired before protocol version 6 have keys but no record: they enter the index at their next completed handshake. Forget Pairings erases the index with the keys it points to. The index loads once at boot in `known_devices_init()`, after Forget Pairings runs and only outside Provisioning Mode, so LIST never reads flash.

OTA Arm gate: Control is Just Works, so OTA_BEGIN returns ERR_OTA_NOT_ARMED (0x07) unless the user held the board button (GPIO 9) for 3 s to 10 s, which arms OTA for 120 s and skips the advertising toggle. A successful OTA_BEGIN clears the arm. A device in Provisioning Mode has no button task, so it cannot be armed.

Rollback: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. A new image boots as pending-verify; `pgp_ota_init()` confirms it with `esp_ota_mark_app_valid_cancel_rollback()` 60 s after Bluetooth starts. A panic or reset inside that window returns to the old slot. Rollback needs a bootloader built with the option, so a device flashed before this change updates fine but cannot roll back until one USB flash. There is no signature or downgrade check: release assets are unsigned, and `app_desc.version` is not `PGP_FW_VERSION`. The arm gate is the authenticity control.

### Provisioning Mode

When `PGP_VALID()` is false (no Session Secrets in `pgpsecret`), `app_main()` starts only BLE and the Control Service. It skips the button, autobutton and PGP GATT services. The device advertises as "PGPemu Setup" (flags and name only, no scan response) so Pokémon GO never sees an accessory, keeps the efuse Bluetooth address because `PGP_MAC` is zero, and advertises regardless of the saved Advertising flag.

PROVISION_SECRETS (0x17) takes the GET_SECRETS layout: name[16] NUL-padded, mac[6], dkey[16], blob[256] (294 bytes). It validates before any write: exact length, name of 1 to 15 printable ASCII bytes, non-zero mac. A bad payload returns ERR_MALFORMED_PAYLOAD. A device that already has secrets returns ERR_NOT_PROVISIONING (0x06) and writes nothing. `write_secrets()` stores the mac last, so a cut-off write still reads as no secrets. The app then sends RESTART. Control stays Just Works, so anyone in range can provision a device that has no secrets.

## Advertising

`global_settings.advertising_enabled` is the only advertising intent. The button, ADVERTISE_START and ADVERTISE_STOP set it, then call `advertise_if_needed()`. That function computes `want = enabled && connections < target` and lets `adv_decide()` (`pgp_adv_state.c`) move the radio state `OFF -> STARTING -> ON -> STOPPING`. A failed start retries with a doubling delay, from 1 s up to 30 s, until the intent changes. GET_GLOBAL_SETTINGS returns 5 bytes: log level, enabled flag, active connections, target connections, radio state (0 off, 1 starting, 2 on, 3 stopping). Bluedroid sends no stop event on connect, so `pgp_adv_on_connect()` resets the state to OFF.

## Flash layout

4 MB flash, `pgpemu-esp32/partitions.csv`:

| Partition | Offset | Size | Holds |
|-----------|--------|------|-------|
| `nvs` | 0x9000 | 128K | Bluedroid bonds, session keys, settings. Auto-erased on corruption. |
| `pgpsecret` | 0x29000 | 16K | Session Secrets. Never auto-erased. |
| `otadata` | 0x2d000 | 8K | OTA slot selection |
| `phy_init` | 0x2f000 | 4K | RF calibration |
| `coredump` | 0x30000 | 64K | Panic core dump |
| `ota_0` | 0x40000 | 1792K | App |
| `ota_1` | 0x200000 | 1792K | OTA target |

Only `init_settings_nvs_partition()` erases flash, and only the default `nvs` partition. `init_secrets_partition()` logs a failure and leaves `pgpsecret` alone, so a firmware update (bootloader, partition table, app) keeps bonds and secrets.

## Companion App build

The companion is `:app` (Android/Compose, package `com.pgpemu.companion`) plus `:lint:checks` (custom lint rules). It has no `build-logic` convention plugins and no `core/`/`feature/` api/impl split: there is one Android module and 22 source files, so a convention plugin would have a single consumer.
Reopen when a second Android module would duplicate `:app` build configuration (`compileSdk`, `minSdk`, Kotlin, Compose).

## Rules

* No dynamic allocation in hot paths.
* ISR code minimal; state machines explicit.
* Debug features compile-time gated.
* Packet captures are the source of truth for protocol behavior.
