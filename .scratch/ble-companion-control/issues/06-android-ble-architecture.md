# Decide Android BLE library and Companion App connection-manager architecture

Type: grilling
Status: resolved
Blocked by: 03

## Question

Using ticket 03's recommendation, decide the connection-manager shape for the Companion App: a single BLE repository/manager class exposing connection state as a `StateFlow` (mirroring locationjoystick's `FollowerSyncClient`/`LeaderSyncServer` pattern per the investigation), its scan → connect → bond → discover → ready state machine, and how it surfaces Command/Response round-trips to the UI layer (suspend functions returning parsed responses vs a raw Flow of frames). Confirm this fits the single-module architecture already decided (Notes on the map).

## Answer

- **Shape**: interface + impl split — `BleControlRepository` (interface) / `NordicBleControlRepository` (impl, built on `no.nordicsemi.android:ble`/`ble-ktx` from ticket 03). Gives locationjoystick's `core/testing`-style fake a seam without adding a second module.
- **Connection state**: explicit sealed `ConnectionState` — `Idle`, `Scanning`, `Connecting`, `Bonding`, `DiscoveringServices`, `Ready`, `Disconnected(reason)`, `Error(reason)` — exposed as one `StateFlow<ConnectionState>`.
- **Command/Response surface**: `suspend fun sendCommand(opcode: Int, payload: ByteArray): Result<ResponseFrame>`, one call per round trip, parsing ticket 04's `[status:u8][opcode:u8][payload]` frame. Not a raw Flow — firmware is strictly one-command-at-a-time (`ERR_BUSY`), and Nordic's request queue already serializes writes, so a suspend call matches the protocol 1:1.
- **Reconnect policy**: manual only. On an unexpected disconnect, drop straight to `Disconnected(reason)` and let the user tap reconnect — no automatic retry loop. Checked `Mygod/pogoplusle` (a related PGP-BLE Android project) for prior art: its `BluetoothReceiver` only passively observes `ACTION_ACL_CONNECTED`/`ACTION_ACL_DISCONNECTED` system broadcasts and reports state via `onAuxiliaryConnected`/`onAuxiliaryDisconnected` — no active reconnect loop — which matches this call rather than contradicting it. (Note: that project is a different problem shape — an accessibility-service pairing helper — not a GATT command client, so it's a validating data point, not a pattern to port directly.)
- **Scan matching**: filter by the existing PGP advertised name (advertisement payload is untouched per the map), connect, discover services, then check the Control Service UUID is present. If absent post-discovery: `Error("device not migrated")`, not a degraded-Ready state.
- Fits the single-module architecture: one repository interface/impl pair, no new module boundary.
