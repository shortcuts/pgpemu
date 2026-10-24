# Companion App Code Style

Rules for `companion-app/` (Kotlin, Jetpack Compose, package `com.pgpemu.companion`). Only rules the code already follows or CI enforces.

## General

- Kotlin only. Package name matches the directory.
- No wildcard imports.
- Formatting is owned by ktlint: run `make companion-format`.

## Compose

- State lives in `DeviceViewModel`; Composables render it and emit events.
- Collect state with `collectAsStateWithLifecycle()`.
- No business logic in Composables.
- User-facing text uses `stringResource()`. The `HardcodedComposeString` lint rule (`companion-app/lint/checks`) fails the build otherwise; check with `make companion-lint`. The rule also flags templates with literal words (`"Found $n devices"`), locals and same-file helpers that return literals, and `clickable(onClickLabel = ...)`. Helpers return a `@StringRes Int`.
- A new string goes into `values/strings.xml` and `values-fr/strings.xml` in the same change. A community language adds a `values-<tag>/` folder and nothing else.

## Coroutines

- Launch from ViewModels with `viewModelScope.launch`.
- Never `runBlocking` on the main thread.
- BLE I/O stays inside the repository.

## Repository pattern

- `BleControlRepository` is the seam. `NordicBleControlRepository` is the implementation.
- ViewModels never touch Nordic BLE types.
- Hilt binds the implementation in `di/BleModule.kt`.

## Error handling

- Repository methods return `Result<T>` (via `runCatching`) at the boundary.
- ViewModels map failures to UI state.
- Do not swallow exceptions.

## Naming

| Kind | Pattern |
|------|---------|
| ViewModel | `FeatureViewModel` |
| Screen | `FeatureScreen` |
| Repository interface | `FeatureRepository` |
| Repository implementation | `<Impl>FeatureRepository` (e.g. `NordicBleControlRepository`) |

See also [companion-testing.md](companion-testing.md).
