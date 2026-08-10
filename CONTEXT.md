# pgpemu

ESP32-C3 firmware emulating a Pokémon GO Plus BLE accessory, plus its companion control surface.

## Language

**Control Service**:
The new custom BLE GATT service added to the firmware, separate from the existing Battery/LED-Button/Cert services, that exposes device configuration and control to the Companion App. Requires an encrypted (bonded) link.
_Avoid_: config service, debug service

**Command characteristic** / **Response characteristic**:
The two characteristics on the Control Service. The app writes an opcode+payload to Command; the firmware answers on Response (notify/indicate). One pair carries every control operation — no per-setting characteristic.
_Avoid_: request/reply characteristic

**Companion App**:
The standalone Android (Kotlin, Jetpack Compose) app that replaces the UART/USB-console as the only way to read or change device state. Connects to one device at a time; no saved multi-device list.
_Avoid_: config app, control app, mobile app

**Device Profile**:
One of the firmware's four emulated-accessory slots (index 0–3), each with its own `autospin`/`autocatch` flags and BLE identity. Distinct from a physical ESP32 unit — one unit hosts up to four Device Profiles.
_Avoid_: device, client, slot

**Session Secrets**:
The Pokémon-GO pairing material persisted per Device Profile in NVS (`pgpsecret` namespace): clone name, MAC, device key, blob. Readable/resettable through the Control Service once bonded.
_Avoid_: pairing data, credentials

**Global Settings** / **Device Settings**:
The two existing NVS-backed config structs (`global_settings`, `device_settings` namespaces) the Control Service reads and writes in place of the removed UART commands.
_Avoid_: config, settings (unqualified)
