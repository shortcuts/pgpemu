# pgpemu

ESP32-C3 firmware emulating a Pokémon GO Plus BLE accessory, plus its companion control surface.

## Language

**Control Service**:
The new custom BLE GATT service added to the firmware, separate from the existing Battery/LED-Button/Cert services, that exposes device configuration and control to the Companion App. Requires an encrypted (bonded) link.
_Avoid_: config service, debug service

**Control protocol version**:
The u8 in byte 0 of the GET_DEVICE_INFO reply. The firmware bumps it on a breaking layout change; the app compares it with the oldest version it decodes.

**Command characteristic** / **Response characteristic**:
The two characteristics on the Control Service. The app writes an opcode+payload to Command; the firmware answers on Response (notify/indicate). One pair carries every control operation — no per-setting characteristic.
_Avoid_: request/reply characteristic

**Event characteristic** / **Device Event**:
The third Control Service characteristic (notify only, bonded link) and the 3-byte frame `[seq][type][slot]` the firmware pushes on it when a game link connects, disconnects, or the LED handler classifies a pattern. Never a command response. The Companion App shows them as its activity feed.
_Avoid_: notification, log line

**Companion App**:
The standalone Android (Kotlin, Jetpack Compose) app that replaces the UART/USB-console as the only way to read or change device state. Scans for nearby devices and lists them; the user picks one to connect to. Connects to one device at a time. Remembers the last connected device and reconnects to it; no saved multi-device list.
_Avoid_: config app, control app, mobile app

**Game Monitor**:
The opt-in Companion App feature that listens to the Pokémon GO notification on the phone and raises its own alerts (device lost, bag full, box full, out of balls, spin failed). Phone-only. Distinct from the activity feed, which shows Device Events.
_Avoid_: notification service

**Device Profile**:
One of the firmware's four emulated-accessory slots (index 0–3, shown as 1–4 in the app), each with its own `autospin` flag, Catch Mode and BLE identity. Distinct from a physical ESP32 unit — one unit hosts up to four Device Profiles.
_Avoid_: device, client, slot. See Known Device for the persisted per-phone record.

**Catch Mode**:
Per Device Profile, when the device presses the button for a wild Pokémon: off, all (green and yellow patterns), or new only (yellow patterns, the Pokémon the account has not registered). Stored under the legacy `catch` key, where 0 is off and 1 is all.

**Known Device**:
A persisted record of one game phone, keyed by its Bluetooth address: last-seen order, user-set name. The firmware indexes it when a handshake completes and keeps at most 15, evicting the least recently seen with its keys, settings and bond. Distinct from a Device Profile, which is a live connection slot.
_Avoid_: account, paired device

**Lifetime Stats**:
Six event counters per Known Device kept in NVS across reboots and reconnects: caught, fled, spin, bag full, box full, balls empty (also Pokéstop out of range). Erased with the Known Device and by Forget Pairings. Distinct from the session counters in `stats.c`, which live in RAM per connection and reset when it ends.
_Avoid_: totals, history

**Crash Report**:
A compact summary of the last panic, read once at boot from the core dump in the `coredump` partition: crash task, registers (`mcause`, `mtval`, `pc`, `ra`, `sp`), the first 9 characters of the build's ELF SHA-256, and up to 16 candidate return addresses scanned from the stack. It stays until the app clears it, across reboots and OTA updates. Distinct from the reset reason, which only says the last restart was a panic.
_Avoid_: crash log, core dump

**Session Secrets**:
The Pokémon-GO pairing material persisted per Device Profile in the dedicated `pgpsecret` NVS partition, never auto-erased: clone name, MAC, device key, blob. Readable/resettable through the Control Service once bonded; writable only in Provisioning Mode.
_Avoid_: pairing data, credentials

**Forget Pairings**:
Erases the default `nvs` partition (bonds, session keys, Global and Device Settings) and restarts. Keeps Session Secrets. Triggered by FORGET_PAIRINGS or a 10 s button hold; the erase runs at the next boot.
_Avoid_: factory reset

**OTA Arm**:
The 120 s window opened by holding the board button for 3 s to 10 s. OTA_BEGIN is refused outside it, because Control pairing is Just Works. A successful OTA_BEGIN closes it.
_Avoid_: unlock

**Firmware Update**:
A new image streamed by the Companion App with OTA_BEGIN, OTA_DATA, OTA_END into the inactive OTA slot. The bootloader returns to the old slot if the new image does not confirm itself within 60 s of boot.
_Avoid_: flash, upgrade

**Provisioning Mode**:
The boot mode of a device with no Session Secrets: BLE runs with only the Control Service, advertised as "PGPemu Setup". The Companion App uploads the secrets with PROVISION_SECRETS, then restarts the device. Not a stored flag: it is `!PGP_VALID()`.
_Avoid_: setup mode, recovery mode

**Status LED**:
The light on GPIO 8. A 50 ms timer shows one pattern for the device state: waiting for a phone, 1-4 game connections, no secrets, or a restart after a crash. The Status light switch (`led_enabled`) turns it off.
_Avoid_: advertising LED

**Update notice**:
The dismissible card the Companion App shows when the latest GitHub release is newer than the app or the connected firmware.
_Avoid_: update badge

**Advertising state**:
The Advertising flag (`advertising_enabled`, the only intent) plus the radio state (off, starting, on, stopping) reported by GET_GLOBAL_SETTINGS. The flag says what the user wants; the radio state says what the controller does.

**Global Settings** / **Device Settings**:
The two existing NVS-backed config structs (`global_settings`, `device_settings` namespaces) the Control Service reads and writes in place of the removed UART commands.
_Avoid_: config, settings (unqualified)
