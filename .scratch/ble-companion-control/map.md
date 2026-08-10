# Map: Android Companion Control for pgpemu

## Destination

A locked spec covering: the new BLE Control Service protocol (GATT layout, opcode/wire-format, pairing/security) that replaces `uart.c`, and the Kotlin Companion App architecture that consumes it. Full parity with today's UART command set. Implementation is a separate follow-up effort — this map produces the design doc, not the code.

## Notes

- Domain: `CONTEXT.md` at repo root (single context). Consult it for canonical terms (Control Service, Companion App, Device Profile, Session Secrets, Global/Device Settings).
- Every session: consult `AGENTS.md` — BLE fidelity/timing/Android-compatibility bar is non-negotiable; no dynamic allocation in hot paths; state machines explicit.
- Skills to reach for: `/grilling` + `/domain-modeling` for decision tickets; `/research` subagent for research tickets; `/prototype` for the UI-shape ticket.
- Investigation already done this session (see repo `Explore` output, not re-fetch): UART command surface lives in `pgpemu-esp32/main/uart.c`; existing GATT services (`pgp_gatts.c/.h`) are fixed PGP-protocol characteristics, not reusable for free-form control; `locationjoystick` has no BLE precedent, only module/DI/testing scaffold worth borrowing.
- Decided while naming the destination (not tickets — locked scope, not re-litigated):
  - UART fully removed, no compile-time debug fallback.
  - Full UART parity in the new control surface, including debug dumps (task list, heap/runtime stats, secret value display) — not just device-behavior controls.
  - New Control Service is a 4th custom GATT service, separate from the existing three; does not consume a `target_active_connections` slot.
  - Command/Response is a single opcode+payload characteristic pair, not one characteristic per setting.
  - Control Service requires BLE bonding/encryption.
  - App discovers the device by its existing PGP advertised name, then does GATT service discovery post-connect — advertisement payload is untouched (protects BLE fidelity of the GO Plus spoof).
  - Companion App: Android only, single device at a time (no multi-device list), simplified single-module architecture (skip locationjoystick's full feature/api+impl split).

## Decisions so far

- [Research Android BLE central APIs and libraries](issues/03-research-android-ble-library.md) — use `no.nordicsemi.android:ble:2.11.0` + `ble-ktx` (operation queueing, retries, bonding, coroutine/Flow support) over hand-rolled native `BluetoothGatt`.
- [Research BLE bonding and security options in ESP-IDF](issues/02-research-bonding-security.md) — per-characteristic `_ENCRYPTED` GATT permissions are attribute-level and won't affect the existing unencrypted Battery/LED-Button/Cert characteristics; `ESP_IO_CAP_NONE` (Just Works, already used) fits the headless devkit; bond storage is split between Bluedroid's own NVS security database (unused APIs `esp_ble_get_bond_device_list`/`esp_ble_remove_bond_device`) and this repo's separate `device_settings` session-key cache, and Android has no public un-pair API, so a full "forget device" needs both stores cleared plus app-side cooperation (reflection) or a user step in system Bluetooth settings.
- [Research BLE MTU and payload limits on ESP32-C3](issues/01-research-mtu-limits.md) — negotiated MTU is peer-driven (up to 517, this repo caps its own request at 500 via `esp_ble_gatt_set_local_mtu`) and indicate/notify payload is capped at MTU-3, but this codebase's existing precedent avoids notify-fragmentation entirely: it sets a large attribute value (up to 512-byte `GATT_MAX_ATTR_LEN` ceiling) via auto-response GATT and lets the stack's built-in ATT Read Blob reassembly handle chunking, using only a tiny indicate as a "data ready" signal.
- [Design the Control Service GATT layout and wire-format protocol](issues/04-control-service-protocol.md) — one Command (write, encrypted) + one Response (indicate + auto-rsp read, encrypted) characteristic pair; `[status:u8][opcode:u8][payload]` response framing; 17-opcode table 1:1 porting every `uart.c` command, diagnostic dumps (`t`/`r`) shipped as raw UTF-8 strings, `S` kept as an explicit save-to-NVS opcode.
- [Decide Android BLE library and Companion App connection-manager architecture](issues/06-android-ble-architecture.md) — `BleControlRepository` interface + Nordic-backed impl, explicit `ConnectionState` sealed class as one `StateFlow`, `suspend fun sendCommand(...): Result<ResponseFrame>` (not a raw Flow), manual-only reconnect on disconnect, name-scan then verify Control Service present post-discovery.
- [Design the pairing and bonding flow](issues/05-pairing-bonding-flow.md) — `ESP_IO_CAP_NONE` unchanged; app never calls `createBond()`, passively observes bonding driven by the firmware's existing connect-time encryption request; RESET_SECRETS stays scoped to Session Secrets only, no bond-clearing "forget device" feature (deferred, out of scope); bond-mismatch (stale Android bond vs wiped firmware bond) surfaces as an explicit error, no automatic reflection-based unpair.
- [Decide the fate of USB-Serial-JTAG console output after uart.c removal](issues/07-console-log-output-fate.md) — logs survive for free via `sdkconfig`'s existing `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (IDF default, independent of `uart.c`); `uart.c` deleted entirely, no replacement module, nothing to compile-gate.
- [Prototype the Companion App's screen/command coverage and destructive-action UX](issues/08-app-screen-coverage-prototype.md) — single scrolling screen (not tabbed), ordered status → profiles → settings → diagnostics → danger zone; destructive actions (RESET_SECRETS, RESTART) confirmed via bottom sheet with type-to-confirm text gate. [Prototype](https://claude.ai/code/artifact/4011ea4c-ae27-4a09-80bd-818934147b46).
- [Design the Companion App module skeleton and Hilt/Compose/BLE-repository wiring](issues/09-app-module-skeleton.md) — light package split (`ble/`, `ui/`, `di/`) in the single module; `di/BleModule.kt` binds the repository singleton; one `DeviceViewModel` for the single screen; `FakeBleControlRepository` test fake carried over from locationjoystick now (not deferred); locationjoystick's feature/api+impl split dropped as overkill at this scope.
- [Decide timeout/retry semantics for the Command/Response exchange](issues/10-command-response-timeout-retry.md) — 5s timeout, no auto-retry (commands aren't uniformly idempotent); mid-command disconnect fails immediately, doesn't wait out the timeout; firmware explicitly clears its in-flight-command flag on disconnect.
- [Decide migration/cutover sequencing from UART to the Control Service](issues/11-migration-cutover-sequencing.md) — same release, paired: firmware and Companion App ship together, no interim gap; README's Serial Menu Commands section fully rewritten into a Companion App usage guide in the same PR.
- [Decide the Companion App's broader testing strategy](issues/12-companion-app-testing-strategy.md) — unit tests via `FakeBleControlRepository` (ticket 09) plus real on-device instrumented testing for pairing/reconnection; no BLE-mock/fake-GATT layer, matches AGENTS.md's existing on-device-integration-test bar.

## Not yet specified

*(empty — all fog graduated into tickets 10-12 once architecture landed)*

## Out of scope

- Keeping UART as a compile-time-gated debug fallback — ruled out, "totally remove that part."
- Companion App managing multiple physical ESP32 units (device list/switcher) — ruled out, single-device-at-a-time model chosen.
- Kotlin Multiplatform / desktop targets — ruled out, Android only.
- Companion App connection counting against `target_active_connections` — ruled out, Control Service is a separate, uncounted connection.
- A "forget device"/full un-pair feature (clearing the Bluedroid bond DB and forcing the Android side to forget the peer) — ruled out while deciding [Design the pairing and bonding flow](issues/05-pairing-bonding-flow.md): Android has no public un-pair API, reflection-based `removeBond()` is unsupported/OEM-fragile.
