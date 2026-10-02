# Architecture

Single-responsibility firmware modules under `pgpemu-esp32/main/`. BLE, storage, timing, and game-protocol logic stay isolated. Module list: `AGENTS.md`, "Key Modules". Domain terms: `CONTEXT.md`.

## Layers

1. **HAL**: `button_input.c`, `setup_button.c`, `led_output.c`: GPIO only.
2. **BLE transport**: `pgp_bluetooth.c`, `pgp_gap.c`, `pgp_gatts.c`.
3. **Connection and protocol**: `pgp_handshake_multi.c`, `pgp_handshake.c`, `pgp_cert.c`, `pgp_control.c`.
4. **Storage**: `config_storage.c`, `config_secrets.c`, `nvs_helper.c`, `settings.c`.
5. **Features**: `pgp_led_handler.c`, `pgp_autobutton.c`.
6. **System**: `pgpemu.c`, `stats.c`, `log_tags.c`.

## Connection flow

1. Game connects over GATT (`pgp_gatts.c`); `pgp_handshake_multi.c` maps `conn_id` to a Device Profile (max 4).
2. First connection: full handshake with passkey, then session key persisted to NVS by MAC.
3. Reconnect: cached session key, expedited handshake, no passkey.
4. LED patterns (`pgp_led_handler.c`) trigger `pgp_autobutton.c` per the profile's settings.

## Control Service

Separate custom GATT service for the Companion App. One Command characteristic (opcode + payload) and one Response characteristic (notify/indicate). Requires an encrypted, bonded link. Handler: `pgp_control.c`.

## Companion App build

The companion is `:app` (Android/Compose, package `com.pgpemu.companion`) plus `:lint:checks` (custom lint rules). It has no `build-logic` convention plugins and no `core/`/`feature/` api/impl split: there is one Android module and 16 source files, so a convention plugin would have a single consumer.
Reopen when a second Android module would duplicate `:app` build configuration (`compileSdk`, `minSdk`, Kotlin, Compose).

## Rules

* No dynamic allocation in hot paths.
* ISR code minimal; state machines explicit.
* Debug features compile-time gated.
* Packet captures are the source of truth for protocol behavior.
