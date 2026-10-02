# Companion App Testing

## Commands

| Command | Purpose |
|---------|---------|
| `make companion-test` | Run unit tests |
| `make companion-coverage` | Kover HTML/XML reports in `companion-app/build/reports/kover/` |
| `make companion-coverage-open` | Open the Kover HTML report |
| `make companion-lint` | Android lint, including `HardcodedComposeString` |

## Unit tests

- JUnit tests under `companion-app/app/src/test/kotlin`.
- ViewModel tests call `Dispatchers.setMain` before and `resetMain` after (see `ui/DeviceViewModelTest.kt`).
- Use `FakeBleControlRepository` (`core/testing`) instead of the real repository.
- Pure functions are tested directly (see `ble/DescribeConnectFailureTest.kt`).

## What to test

- External behavior: UI state after repository responses.
- Frame and opcode encoding in `ble/`.

## What not to test

- The real BLE radio or the Nordic BLE library. Needs a device; the maintainer tests on-device.
- Compose rendering.

## Required before done

`make companion-test` and `make companion-lint` must pass.

See also [companion-code-style.md](companion-code-style.md).
