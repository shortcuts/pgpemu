# Design the pairing and bonding flow

Type: grilling
Status: resolved
Blocked by: 02

## Question

Using ticket 02's findings, decide: the IO capability the firmware advertises, the app-side bonding UX (when `createBond()` fires relative to connect/service-discovery), how a bonded device is cleared on the firmware (does `xr`'s successor reset the BLE bond too, or only Session Secrets?), and what happens when the app connects to a device it has no Android-level bond record for (re-pair flow) versus a device whose firmware-side bond was wiped independently (NVS erase, factory reset).

## Answer

- **IO capability**: unchanged — `ESP_IO_CAP_NONE` (Just Works), already link-wide via `pgp_bluetooth.c`. Control Service characteristics use plain `_ENCRYPTED` GATT permissions (ticket 04) riding on that existing link encryption; no per-attribute MITM bump.
- **Bonding trigger**: passive. The app never calls `createBond()`. Firmware already requests link encryption unconditionally on `ESP_GATTS_CONNECT_EVT` (`pgp_gatts.c:722-732`), which surfaces Android's Just Works system pairing prompt automatically. The app (ticket 06's connection-manager) observes `ACTION_BOND_STATE_CHANGED`, sits in `Bonding` until `BOND_BONDED`, then moves to `DiscoveringServices`.
- **RESET_SECRETS (`0x05`, `xr` successor) scope**: Session Secrets only — clears the `pgpsecret` NVS namespace (clone name/mac/device key/blob), matching its name and today's behavior. Does **not** touch the Bluedroid bond DB or the `device_settings` session-key cache. No new opcode, no change to ticket 04's table.
- **No "forget device" feature in this spec.** A full un-pair (clearing both firmware-side stores plus forcing the Android side) is out of scope — Android has no public un-pair API, and a reflection-based `removeBond()` is unsupported/OEM-fragile. Deferred, not designed here.
- **Bond-mismatch recovery** (firmware bond wiped independently — NVS erase/factory reset — while Android still holds a stale bond record): encryption negotiation fails on reconnect attempt. App shows an explicit error state ("Pairing out of sync — forget this device in Android Bluetooth settings and reconnect"). No automatic reflection-based `removeBond()` call — silent hidden-API calls are the kind of surprise behavior AGENTS.md's BLE-fidelity bar rules out.
- **First-time connect, no Android-level bond**: normal case, same passive flow as above — nothing special, this *is* the first-pair path.
