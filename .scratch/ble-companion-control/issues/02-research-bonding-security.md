# Research BLE bonding and security options in ESP-IDF

Type: research
Status: resolved

## Question

How does ESP-IDF v5.4.1's BLE stack implement per-characteristic "requires encryption" GATT permissions alongside other characteristics on the same GAP link that don't require it (confirm this doesn't force encryption onto the existing Battery/LED-Button/Cert services used by the Pokémon GO client)? What IO-capability options fit a headless ESP32-C3 devkit (Just Works vs passkey), how are bonded devices stored/enumerated/cleared (`esp_ble_gap_security` APIs, NVS bonding storage), and what's the app-side implication (Android `BluetoothDevice.createBond()` flow) for pairing with and later un-pairing from the Control Service?

## Answer

### 1. Per-characteristic encryption permissions are attribute-level, not link-level

ESP-IDF's `esp_gatt_defs.h` (v5.4.1) defines encryption as a permission bit on each
attribute, separate from `ESP_GATT_PERM_READ`/`ESP_GATT_PERM_WRITE`:
`ESP_GATT_PERM_READ_ENCRYPTED`, `ESP_GATT_PERM_WRITE_ENCRYPTED`,
`ESP_GATT_PERM_READ_ENC_MITM`, `ESP_GATT_PERM_WRITE_ENC_MITM`, plus signed-write and
authorization variants.
(Source: ESP-IDF v5.4.1 docs, `esp_gatt_defs.h` API reference,
https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/bluetooth/esp_gatt_defs.html)

The official ESP-IDF `gatt_security_server` example — the same demo this repo's
`pgp_bluetooth.c` cites as its inspiration (`main/pgp_bluetooth.c:25`) — builds one
Heart Rate Service attribute table that mixes both kinds of characteristics side by
side: `HRS_IDX_HR_MEAS_VAL` uses plain `ESP_GATT_PERM_READ`, while
`HRS_IDX_BOBY_SENSOR_LOC_VAL`/`HRS_IDX_HR_CTNL_PT_VAL` use
`ESP_GATT_PERM_READ_ENCRYPTED`/`ESP_GATT_PERM_WRITE_ENCRYPTED` in the same table.
(Source: espressif/esp-idf, `examples/bluetooth/bluedroid/ble/gatt_security_server/main/example_ble_sec_gatts_demo.c`,
release/v5.4 branch.)

Bluedroid's ATT server checks the permission of the specific attribute handle being
accessed. If that attribute requires encryption and the link isn't currently
encrypted, only that access is rejected (Insufficient Authentication/Encryption ATT
error), which the client/stack can use to trigger pairing. Attributes without the
`_ENCRYPTED`/`_ENC_MITM` bit are served normally regardless of link encryption state.
This matches the BLE Core spec's ATT-layer security model, where permission is a
per-attribute property, not a per-link one.

**Confirmed against this repo:** none of the existing GATT tables in
`pgpemu-esp32/main/pgp_gatts.c` use the `_ENCRYPTED`/`_ENC_MITM` permission bits.
`gatt_db_battery`, `gatt_db_led_button`, and `gatt_db_certificate` all use plain
`ESP_GATT_PERM_READ` / `ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE`
(`pgp_gatts.c:153-528`). Adding `ESP_GATT_PERM_*_ENCRYPTED` only to new Control
Service characteristics therefore would not force encryption onto Battery,
LED/Button, or Cert reads/writes — each is checked independently.

Note this repo already forces link-wide encryption today anyway, ahead of any
per-attribute permission check: on `ESP_GATTS_CONNECT_EVT`, if there's no cached
session for the peer, it calls `esp_ble_set_encryption(remote_bda,
ESP_BLE_SEC_ENCRYPT_MITM)` unconditionally for the whole link
(`pgp_gatts.c:722-732`). That's a link-level action independent of which
characteristic gets accessed first; per-attribute `_ENCRYPTED` flags on a new
Control Service would be redundant with (not conflicting with) this existing
behavior, but would still be the correct/idiomatic way to declare the requirement
if the connect-time `esp_ble_set_encryption` call is ever removed or made
conditional.

### 2. IO capability for a headless ESP32-C3 devkit

`pgp_bluetooth.c:86` already sets `esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;` —
i.e. Just Works pairing, no passkey/PIN exchange — with `ESP_LE_AUTH_BOND` and no
MITM auth-req bit set (`pgp_bluetooth.c:85-96`). This fits a headless devkit with no
display and no keypad: Just Works is the only IO-capability option that doesn't
require the device to show or accept a 6-digit code.
Passkey-entry (`ESP_IO_CAP_OUT`, `ESP_IO_CAP_KBDISP`, etc.) or a static passkey
(`ESP_BLE_SM_SET_STATIC_PASSKEY`) require either a display, an input method, or a
fixed pre-shared code — none of which exist on this hardware — and would also
diverge from real Pokémon GO Plus / GO Plus+ pairing behavior, which itself uses
Just Works. Just Works trades MITM protection for zero user interaction; that
tradeoff is inherent to the hardware, not a firmware choice.

### 3. Bond storage/enumeration/clearing

ESP-IDF exposes bond management through `esp_gap_ble_api.h`:
`esp_ble_get_bond_device_num()`, `esp_ble_get_bond_device_list()`, and
`esp_ble_remove_bond_device()`. These operate on Bluedroid's own internal security
database, which persists bonded-device records (LTK/IRK/CSRK, address type) in
NVS across reboots.
(Source: ESP-IDF v5.4.1 docs, `esp_gap_ble.h` API reference,
https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/bluetooth/esp_gap_ble.html)

**Current repo usage:** only `esp_ble_get_bond_device_num()` is called, for a log
line after `ESP_GAP_BLE_AUTH_CMPL_EVT` (`pgp_gap.c:77-82`). There is no call to
`esp_ble_get_bond_device_list()` or `esp_ble_remove_bond_device()` anywhere in
`pgpemu-esp32/main/` (confirmed by repo-wide grep) — the firmware has no existing
way to enumerate or clear bonds. A Control Service "un-pair" feature would need to
add calls to these APIs.

Separately, this repo maintains its **own** app-level session-key cache in the
`device_settings` NVS namespace, keyed by peer MAC
(`config_storage.c:386-415`, `has_cached_session()`/`retrieve_device_session_keys()`).
This is distinct from Bluedroid's bond database: it's used to skip re-requesting
encryption on reconnect (`pgp_gatts.c:722-732`) and to skip re-running the PGP
handshake (`pgp_handshake.c:56`). Clearing a bond via `esp_ble_remove_bond_device()`
would **not** automatically clear this app-level cache, and vice versa — a
Control Service un-pair action needs to clear both stores to fully forget a device.

### 4. Android app-side implication (`BluetoothDevice.createBond()`)

`BluetoothDevice.createBond()` starts pairing; state moves through `BOND_NONE` →
`BOND_BONDING` → `BOND_BONDED`, broadcast via `ACTION_BOND_STATE_CHANGED` with
`EXTRA_BOND_STATE`/`EXTRA_PREVIOUS_BOND_STATE`/`EXTRA_DEVICE`.
(Source: Android developer docs, `BluetoothDevice` reference,
https://developer.android.com/reference/android/bluetooth/BluetoothDevice)

Because this firmware initiates `esp_ble_set_encryption(...)` from the peripheral
side on first connection (`pgp_gatts.c:728`), and responds to
`ESP_GAP_BLE_SEC_REQ_EVT` by auto-accepting (`pgp_gap.c:83-86`), pairing on Android
is driven by the ESP32 without the app needing to call `createBond()` explicitly —
Android surfaces the Just Works system pairing prompt/consent automatically when
the peripheral requests encryption.

**Important asymmetry for un-pairing:** the Android public API has **no**
`removeBond()` method. Apps can only unpair a device via a hidden/reflection-based
call (`BluetoothDevice.class.getMethod("removeBond")`), which is unsupported and can
break across Android versions/OEMs, or by directing the user to the system
Bluetooth settings screen. This means an ESP32-side "clear bonds" Control Service
command can wipe the firmware's bond record and session cache, but cannot make the
phone forget the device — if the app doesn't also call the hidden `removeBond()`
(or the user doesn't manually unpair in system settings), the two sides' bond state
will disagree, and reconnection will fail until the phone's stale bond is removed
too.

### Summary

- Per-characteristic `_ENCRYPTED`/`_ENC_MITM` GATT permissions are independent
  per-attribute checks; adding them to a new Control Service is safe and won't
  affect the existing unencrypted Battery/LED-Button/Cert characteristics.
- `ESP_IO_CAP_NONE` (Just Works, already in use) is the correct/only realistic
  choice for this headless hardware.
- Bond storage is split across two systems in this repo: Bluedroid's own NVS-backed
  security database (enumerable/clearable via `esp_ble_get_bond_device_list()` /
  `esp_ble_remove_bond_device()`, neither currently called) and this repo's own
  `device_settings` NVS session-key cache (`config_storage.c`). Both need clearing
  for a full un-pair.
- Android has no public un-pair API; a Control Service "forget this device"
  feature can only fully unpair with app-side cooperation (reflection) or a
  user-driven step in system Bluetooth settings.
