# Decide the Companion App's broader testing strategy

Type: grilling
Status: resolved

## Question

Ticket 09 already carries over a `FakeBleControlRepository` unit-test seam from locationjoystick. Beyond that: does this app need BLE-mock/integration-level testing (mirroring locationjoystick's MockK/Turbine conventions) for the connection-manager's state machine (ticket 06), or is `DeviceViewModel` + fake-repository unit coverage sufficient given AGENTS.md's existing on-device integration-test bar for pairing/reconnection? If integration-level testing is wanted, what's mocked — the Nordic library's `BleManager`, or a fake GATT server?

## Answer

**Unit tests + real on-device testing — no BLE-mock/fake-GATT layer.**

- Unit coverage: `DeviceViewModel` tested against the `FakeBleControlRepository` (ticket 09) — covers UI-state derivation, command dispatch, error surfacing, all without touching BLE.
- Integration coverage: real on-device instrumented testing against actual firmware for pairing/reconnection/bonding — matches AGENTS.md's existing bar ("on-device integration tests for pairing and reconnection", "packet captures are the source of truth"). This class of bug (bonding races, GATT quirks, real Android BLE stack behavior across OEMs) is exactly what a mocked `BleManager` or fake GATT server would fail to catch faithfully — real hardware is the more trustworthy signal, and this repo already treats it as such on the firmware side.
- No new mock/fake-GATT infrastructure. Simplest path, consistent with the project's existing testing philosophy.
