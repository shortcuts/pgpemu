# Companion App Testing

## Commands

These targets need JDK 17. `mise install` provides it; see [CONTRIBUTING.md](../CONTRIBUTING.md#building-the-companion-app).

| Command | Purpose |
|---------|---------|
| `make companion-test` | Run unit tests |
| `make companion-coverage` | Kover HTML/XML reports in `companion-app/build/reports/kover/` |
| `make companion-coverage-open` | Open the Kover HTML report |
| `make companion-lint` | Android lint, including `HardcodedComposeString` |
| `make companion-install-release` | Build the minified release APK and install it on the connected phone |

## Installing the release build

`make companion-install-release` builds `:app:assembleRelease` with R8 minification, signs it with `~/.android/debug.keystore` (alias `androiddebugkey`), and runs `adb install -r`. Use it to catch minification crashes before a release. The debug keystore is created by the first debug build.

- The install replaces an app signed with the same key. A debug install is replaced without a prompt.
- An app signed with a different key cannot be replaced. Run `adb uninstall com.pgpemu.companion` first, then rerun the target. The uninstall also deletes the app's saved data, such as the last connected device. Remove the phone's Bluetooth pairing in the phone's Bluetooth settings if needed.

## Unit tests

- JUnit tests under `companion-app/app/src/test/kotlin`.
- ViewModel tests call `Dispatchers.setMain` before and `resetMain` after (see `ui/DeviceViewModelTest.kt`).
- Use `FakeBleControlRepository` (`core/testing`) instead of the real repository.
- Pure functions are tested directly (see `ble/DescribeConnectFailureTest.kt`).

## What to test

- External behavior: UI state after repository responses.
- Frame and opcode encoding in `ble/`, checked against `protocol/control_vectors.txt`.

## What not to test

- The real BLE radio or the Nordic BLE library. Needs a device; the maintainer tests on-device.
- Compose rendering.

## Required before done

`make companion-test` and `make companion-lint` must pass.

See also [companion-code-style.md](companion-code-style.md).
