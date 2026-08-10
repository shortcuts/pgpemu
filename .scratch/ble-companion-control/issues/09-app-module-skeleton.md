# Design the Companion App module skeleton and Hilt/Compose/BLE-repository wiring

Type: grilling
Status: resolved

## Question

Using ticket 06's connection-manager design, lay out the actual package/module skeleton for the simplified single-module Companion App: package structure, where the BLE repository is provided (Hilt module), how Compose screens (from ticket 08's prototype) observe connection/command state, and what locationjoystick conventions carry over (its `di/XModule.kt` pattern, `core/testing` fake conventions) versus what's dropped as overkill at this scope.

## Answer

**Package layout** (single module, light split — not locationjoystick's feature/api+impl split):
```
ble/   — BleControlRepository (interface, ticket 06), NordicBleControlRepository (impl),
         ConnectionState, ResponseFrame, opcode constants (ticket 04's table)
ui/    — the single screen (ticket 08's layout) + DeviceViewModel
di/    — BleModule.kt (Hilt)
```

**DI**: `di/BleModule.kt`, `@Binds` `BleControlRepository` to `NordicBleControlRepository` as a singleton — carries over locationjoystick's `di/XModule.kt` naming convention.

**Compose/state**: one `DeviceViewModel`, not per-section. Exposes a single UI state (derived from the repository's `ConnectionState` `StateFlow` plus parsed command results) that the single Composable screen collects via `collectAsStateWithLifecycle`. Matches ticket 08's single-scrolling-screen call — no per-card ViewModel split to coordinate.

**Testing**: carry over locationjoystick's `core/testing` fake convention now, not deferred. A `FakeBleControlRepository` implementing the ticket 06 interface, living in a `test`/`androidTest` source set fake — cheap since the interface already exists from ticket 06, and it unblocks ViewModel tests without a separate testing-strategy ticket.

**Dropped as overkill**: locationjoystick's feature-module/`api`+`impl` split, its multi-module Gradle structure — this app is one module, one repository, one screen; that structure buys nothing at this scope.
