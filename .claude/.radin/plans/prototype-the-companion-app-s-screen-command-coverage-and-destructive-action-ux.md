# Plan: Companion app full screen — command coverage + destructive-action UX

Task: `prototype-the-companion-app-s-screen-command-coverage-and-destructive-action-ux`
Branch: `ble-companion-control`. Builds on commit `93a4bba` (ble/ package) and
`72d6921` (Hilt DI, minimal DeviceScreen/DeviceViewModel).

Scope: replace the placeholder `DeviceScreen`/`DeviceViewModel` with the full
resolved layout from `.scratch/ble-companion-control/issues/08-app-screen-coverage-prototype.md`
— Status → Device Profiles → Settings → Diagnostics → Danger Zone, every
0x01–0x11 opcode routed through `BleControlRepository.sendCommand`, shared
type-to-confirm bottom sheet for the two destructive opcodes. No new screen
navigation, no new Gradle dependency, no build/run (plan only).

## Facts settled by reading source (not invented)

Cross-checked `pgpemu-esp32/main/pgp_control.h` + `pgp_control.c` (the actual
switch-case implementation, not just the header) against the Kotlin `Opcode`
enum — the enum already matches the header exactly, no changes needed there.

Per-opcode request/response payload shapes (from `pgp_control.c`'s
`pgp_control_handle_command_write`):

| Opcode | Request payload | Response payload (status OK) |
|---|---|---|
| `HELP` 0x01 | none | text (not surfaced in UI per resolved design) |
| `GET_GLOBAL_SETTINGS` 0x02 | none | 4 bytes: `[logLevel, advertisingEnabled(0/1), activeConnections, targetMaxConnections]` |
| `SAVE_SETTINGS` 0x03 | none | none |
| `GET_SECRETS` 0x04 | none | 294 bytes (not surfaced in UI — resolved design's GET list is 0x02/0x07/0x09/0x0A/0x0D only, excludes secrets) |
| `RESET_SECRETS` 0x05 | none | none |
| `RESTART` 0x06 | none | none (device sends this frame, then calls `esp_restart()` — connection drops right after) |
| `GET_LED_STATE` 0x07 | none | 1 byte: led on/off (0/1) |
| `CYCLE_LOG_LEVEL` 0x08 | none | 1 byte: new log level (wraps 1→2→3→1) |
| `GET_RUNTIME_STATS` 0x09 | none | UTF-8 text |
| `GET_TASK_LIST` 0x0A | none | UTF-8 text |
| `ADVERTISE_START` 0x0B | none | none |
| `ADVERTISE_STOP` 0x0C | none | none |
| `GET_CLIENT_STATES` 0x0D | none | UTF-8 text |
| `RESET_CLIENT_STATES` 0x0E | none | none |
| `SET_MAX_CONNECTIONS` 0x0F | 1 byte: target, range 1..4 (`CONFIG_BT_ACL_CONNECTIONS=4` in `pgpemu-esp32/sdkconfig`) | none |
| `TOGGLE_AUTOSPIN` 0x10 | 1 byte: device index 0..3 | 1 byte: new autospin state (0/1) |
| `TOGGLE_AUTOCATCH` 0x11 | 1 byte: device index 0..3 | 1 byte: new autocatch state (0/1) |

`CONFIG_BT_ACL_CONNECTIONS=4` is compile-time on the firmware and has no
opcode to query it at runtime — the app must hardcode `4` as the stepper's
upper bound and the profile-row length. Flagged with a `ponytail:` comment at
the point it's hardcoded.

**Known data gap (not fixable in this task, no new opcode exists):** there is
no `GET_*` opcode for per-device autospin/autocatch current state — only the
toggle opcodes, which return the state *after* flipping. The Device Profiles
chips therefore render as **unknown until first toggled** this session (shown
as an "—" dash, not a false-looking off state) rather than reflecting true
device state on connect. Documented, not silently guessed around.

**"Device Profile" in the resolved-design consequence-line wording**:
`reset_secrets()` (`config_secrets.c`) is global — the clone identity/MAC/key
blob, not one of the 4 autospin/autocatch device slots. `RESET_SECRETS` also
takes no payload (no device index). So the confirm-sheet consequence line for
Reset Secrets names "this device" (singular — the one paired PGP Plus clone
the app is connected to), not one of the 4 profile chips. This is a factual
read of the C code, not a design decision.

## File layout decision

Two files touched for UI, one rewritten for state — not one file per
section. Rationale (ladder rung 2/3): `DeviceScreen.kt` already exists as the
single screen file and this is one screen, not five independent widgets —
splitting into 5 section files buys nothing since none of them has a second
caller. The one genuinely reusable piece — the type-to-confirm bottom sheet,
shared verbatim between `RESET_SECRETS` and `RESTART` — gets its own file
because it *is* reused, which is the actual bar for a new file, not "this
concept sounds separable."

1. `companion-app/app/src/main/kotlin/com/pgpemu/companion/ui/DeviceViewModel.kt`
   — rewritten. Expanded `DeviceUiState`, command dispatch, decode.
2. `companion-app/app/src/main/kotlin/com/pgpemu/companion/ui/DeviceScreen.kt`
   — rewritten. `DeviceScreen` + one private composable per section, all in
   this file (they're 5 tightly-coupled views of one screen, not reusable
   components).
3. `companion-app/app/src/main/kotlin/com/pgpemu/companion/ui/ConfirmActionSheet.kt`
   — new. The shared type-to-confirm bottom sheet composable.

No change to `ble/` package, `di/BleModule.kt`, `FakeBleControlRepository`,
or `DeviceViewModelTest` (existing test only reads `uiState.connectionState`,
which keeps working since it's still a field on the expanded state — verify
by inspection after the rewrite, don't touch the test file).

## 1. `DeviceViewModel.kt`

Replace the whole file. State shape:

```kotlin
data class DeviceUiState(
    val connectionState: ConnectionState = ConnectionState.Idle,
    val status: StatusState = StatusState(),
    val profiles: List<ProfileState> = List(DEVICE_PROFILE_COUNT) { ProfileState(index = it) },
    val settings: SettingsState = SettingsState(),
    val diagnostics: DiagnosticsState = DiagnosticsState(),
    val isBusy: Boolean = false,
    val errorMessage: String? = null,
    val pendingConfirmation: ConfirmAction? = null,
)

data class StatusState(
    val ledOn: Boolean? = null,          // null = not loaded yet
    val advertisingEnabled: Boolean? = null,
    val activeConnections: Int? = null,
    val logLevel: Int? = null,           // 1=DEBUG, 2=INFO, 3=VERBOSE
)

data class ProfileState(
    val index: Int,
    val autospin: Boolean? = null,       // null = unknown (no GET opcode exists, see plan notes)
    val autocatch: Boolean? = null,
)

data class SettingsState(
    val maxConnections: Int? = null,
)

data class DiagnosticsState(
    val runtimeStats: String? = null,
    val taskList: String? = null,
    val clientStates: String? = null,
)

sealed interface ConfirmAction {
    val confirmWord: String
    val title: String
    val consequence: String
    data object ResetSecrets : ConfirmAction {
        override val confirmWord = "RESET"
        override val title = "Reset secrets"
        override val consequence =
            "This erases this device's clone identity, MAC, and keys. " +
                "It will need to be re-paired from scratch. This cannot be undone."
    }
    data object Restart : ConfirmAction {
        override val confirmWord = "RESTART"
        override val title = "Restart device"
        override val consequence =
            "This restarts the device immediately. The BLE connection will drop."
    }
}

// ponytail: CONFIG_BT_ACL_CONNECTIONS is compile-time on the firmware side
// (pgpemu-esp32/sdkconfig) with no opcode to read it at runtime; hardcode
// here, bump both sides together if the firmware value ever changes.
const val DEVICE_PROFILE_COUNT = 4
const val MAX_CONNECTIONS_LIMIT = 4
```

`DeviceViewModel`:

- Keeps `private val _uiState = MutableStateFlow(DeviceUiState())` +
  `val uiState: StateFlow<DeviceUiState> = _uiState.asStateFlow()` (switch
  from the current `.map(...).stateIn(...)` over `repository.connectionState`
  to an owned mutable state, since state now has many more fields the
  repository doesn't own). Keep a separate `viewModelScope.launch { repository.connectionState.collect { ... } }`
  that updates `_uiState.update { it.copy(connectionState = new) }` and,
  on transition into `ConnectionState.Ready`, calls `refreshStatus()`.
- `connect()` / `disconnect()` — unchanged behavior, still delegate to
  `repository.connect()` / `repository.disconnect()`.
- One private suspend helper used by every command:

```kotlin
private fun runCommand(opcode: Int, payload: ByteArray = ByteArray(0), onOk: (ResponseFrame) -> Unit) {
    if (_uiState.value.isBusy) return // one in-flight command at a time — NordicBleControlRepository has a single pendingResponse slot
    viewModelScope.launch {
        _uiState.update { it.copy(isBusy = true, errorMessage = null) }
        val result = repository.sendCommand(opcode, payload)
        result.fold(
            onSuccess = { frame ->
                if (frame.isOk) onOk(frame) else _uiState.update { it.copy(errorMessage = "device returned status ${frame.status}") }
            },
            onFailure = { e -> _uiState.update { it.copy(errorMessage = e.message ?: "command failed") } },
        )
        _uiState.update { it.copy(isBusy = false) }
    }
}
```

- `refreshStatus()`: fires `GET_GLOBAL_SETTINGS` (0x02) and `GET_LED_STATE`
  (0x07), decoding into `StatusState`/`SettingsState.maxConnections`:

```kotlin
fun refreshStatus() {
    runCommand(Opcode.GET_GLOBAL_SETTINGS) { frame ->
        val p = frame.payload
        _uiState.update {
            it.copy(
                status = it.status.copy(
                    logLevel = p[0].toInt(),
                    advertisingEnabled = p[1] == 1.toByte(),
                    activeConnections = p[2].toInt() and 0xFF,
                ),
                settings = it.settings.copy(maxConnections = p[3].toInt() and 0xFF),
            )
        }
    }
    runCommand(Opcode.GET_LED_STATE) { frame ->
        _uiState.update { it.copy(status = it.status.copy(ledOn = frame.payload[0] == 1.toByte())) }
    }
}
```

- Settings actions:

```kotlin
fun toggleAdvertising() {
    val turningOn = _uiState.value.status.advertisingEnabled != true
    val opcode = if (turningOn) Opcode.ADVERTISE_START else Opcode.ADVERTISE_STOP
    runCommand(opcode) { _uiState.update { it.copy(status = it.status.copy(advertisingEnabled = turningOn)) } }
}

fun setMaxConnections(value: Int) {
    val clamped = value.coerceIn(1, MAX_CONNECTIONS_LIMIT)
    runCommand(Opcode.SET_MAX_CONNECTIONS, byteArrayOf(clamped.toByte())) {
        _uiState.update { it.copy(settings = it.settings.copy(maxConnections = clamped)) }
    }
}

fun cycleLogLevel() {
    runCommand(Opcode.CYCLE_LOG_LEVEL) { frame ->
        _uiState.update { it.copy(status = it.status.copy(logLevel = frame.payload[0].toInt())) }
    }
}

fun saveSettings() {
    runCommand(Opcode.SAVE_SETTINGS) { /* no payload; status OK is the confirmation */ }
}
```

- Device Profiles actions:

```kotlin
fun toggleAutospin(index: Int) {
    runCommand(Opcode.TOGGLE_AUTOSPIN, byteArrayOf(index.toByte())) { frame ->
        updateProfile(index) { it.copy(autospin = frame.payload[0] == 1.toByte()) }
    }
}

fun toggleAutocatch(index: Int) {
    runCommand(Opcode.TOGGLE_AUTOCATCH, byteArrayOf(index.toByte())) { frame ->
        updateProfile(index) { it.copy(autocatch = frame.payload[0] == 1.toByte()) }
    }
}

private fun updateProfile(index: Int, transform: (ProfileState) -> ProfileState) {
    _uiState.update { s -> s.copy(profiles = s.profiles.map { if (it.index == index) transform(it) else it }) }
}
```

- Diagnostics actions (each card refreshed on-demand by its own button, per
  the resolved design's "refreshed on connect and on-demand" — the three
  dumps are independent and can be individually re-fetched):

Three near-identical dumps collapse into one parameterized helper instead of
three copy-pasted functions:

```kotlin
private fun refreshDiagnostic(opcode: Int, apply: (DiagnosticsState, String) -> DiagnosticsState) {
    runCommand(opcode) { frame ->
        val text = String(frame.payload, Charsets.UTF_8)
        _uiState.update { it.copy(diagnostics = apply(it.diagnostics, text)) }
    }
}

fun refreshRuntimeStats() = refreshDiagnostic(Opcode.GET_RUNTIME_STATS) { d, text -> d.copy(runtimeStats = text) }
fun refreshTaskList() = refreshDiagnostic(Opcode.GET_TASK_LIST) { d, text -> d.copy(taskList = text) }
fun refreshClientStates() = refreshDiagnostic(Opcode.GET_CLIENT_STATES) { d, text -> d.copy(clientStates = text) }

fun disconnectAllClients() {
    runCommand(Opcode.RESET_CLIENT_STATES) { /* no payload; refresh the dump so the UI reflects it */ refreshClientStates() }
}
```

- Danger zone — request/confirm split, so the screen never fires the
  destructive opcode without the sheet's gate:

```kotlin
fun requestResetSecrets() { _uiState.update { it.copy(pendingConfirmation = ConfirmAction.ResetSecrets) } }
fun requestRestart() { _uiState.update { it.copy(pendingConfirmation = ConfirmAction.Restart) } }
fun dismissConfirmation() { _uiState.update { it.copy(pendingConfirmation = null) } }

fun confirmPendingAction() {
    when (_uiState.value.pendingConfirmation) {
        ConfirmAction.ResetSecrets -> runCommand(Opcode.RESET_SECRETS) { dismissConfirmation() }
        ConfirmAction.Restart -> runCommand(Opcode.RESTART) { dismissConfirmation() }
        null -> Unit
    }
}
```

(`RESTART`'s response arrives before the firmware calls `esp_restart()` per
`pgp_control.c`, so `runCommand`'s normal success path fires; the subsequent
disconnect surfaces through the existing `connectionState` collector, no
special-case needed.)

## 2. `ConfirmActionSheet.kt` (new file)

`ModalBottomSheet` (material3, already a BOM dependency — no new library)
with a plain-language consequence line and a type-to-confirm `TextField`.
Confirm button disabled until the typed text exactly equals `action.confirmWord`.

```kotlin
package com.pgpemu.companion.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfirmActionSheet(
    action: ConfirmAction,
    isBusy: Boolean,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit,
) {
    var typed by remember(action) { mutableStateOf("") }
    ModalBottomSheet(onDismissRequest = onDismiss, sheetState = rememberModalBottomSheetState()) {
        Column(modifier = Modifier.fillMaxWidth().padding(24.dp)) {
            Text(text = action.title)
            Text(text = action.consequence)
            OutlinedTextField(
                value = typed,
                onValueChange = { typed = it },
                label = { Text("Type ${action.confirmWord} to confirm") },
                modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
            )
            Button(
                onClick = onConfirm,
                enabled = typed == action.confirmWord && !isBusy,
                modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
            ) {
                Text(action.title)
            }
        }
    }
}
```

`remember(action)` keys the typed text to the current action so switching
between Reset Secrets and Restart (dismiss one, open the other) doesn't leak
a stale confirm word across actions.

## 3. `DeviceScreen.kt`

Replace the whole file. Top-level `DeviceScreen` composable:

- Same connect/disconnect header as today (unchanged logic).
- Below it, only when `connectionState is ConnectionState.Ready`: a
  `LazyColumn` with the 5 sections as items, in risk/frequency order
  (Status → Device Profiles → Settings → Diagnostics → Danger Zone), each a
  `Card` with a section title.
- When `uiState.pendingConfirmation != null`, render `ConfirmActionSheet`
  with `onConfirm = viewModel::confirmPendingAction`,
  `onDismiss = viewModel::dismissConfirmation`.
- When `uiState.errorMessage != null`, show it as a `Text` under the header
  (no snackbar host exists yet in `MainActivity` — plain text keeps this
  change scoped to this one screen, don't add a `Scaffold`/`SnackbarHost`
  for one string).

Section composables (private, same file):

```kotlin
@Composable
private fun StatusSection(status: StatusState) {
    SectionCard(title = "Status") {
        LabeledValue("LED", status.ledOn?.let { if (it) "On" else "Off" } ?: "—")
        LabeledValue("Advertising", status.advertisingEnabled?.let { if (it) "On" else "Off" } ?: "—")
        LabeledValue("Active connections", status.activeConnections?.toString() ?: "—")
        LabeledValue("Log level", status.logLevel?.let { logLevelName(it) } ?: "—")
    }
}

private fun logLevelName(level: Int) = when (level) { 3 -> "VERBOSE"; 2 -> "INFO"; else -> "DEBUG" }

@Composable
private fun DeviceProfilesSection(profiles: List<ProfileState>, onToggleAutospin: (Int) -> Unit, onToggleAutocatch: (Int) -> Unit) {
    SectionCard(title = "Device Profiles") {
        profiles.forEach { profile ->
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
                Text(text = "Profile ${profile.index + 1}", modifier = Modifier.weight(1f))
                FilterChip(selected = profile.autospin == true, onClick = { onToggleAutospin(profile.index) }, label = { Text("Autospin") })
                FilterChip(selected = profile.autocatch == true, onClick = { onToggleAutocatch(profile.index) }, label = { Text("Autocatch") })
            }
        }
    }
}

@Composable
private fun SettingsSection(status: StatusState, settings: SettingsState, onToggleAdvertising: () -> Unit, onSetMaxConnections: (Int) -> Unit, onCycleLogLevel: () -> Unit, onSave: () -> Unit) {
    SectionCard(title = "Settings") {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(text = "Advertising", modifier = Modifier.weight(1f))
            Switch(checked = status.advertisingEnabled == true, onCheckedChange = { onToggleAdvertising() })
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(text = "Max connections: ${settings.maxConnections ?: "—"}", modifier = Modifier.weight(1f))
            Button(onClick = { onSetMaxConnections((settings.maxConnections ?: 1) - 1) }) { Text("-") }
            Button(onClick = { onSetMaxConnections((settings.maxConnections ?: 1) + 1) }) { Text("+") }
        }
        Button(onClick = onCycleLogLevel) { Text("Cycle log level") }
        Button(onClick = onSave) { Text("Save settings") }
    }
}

@Composable
private fun DiagnosticsSection(diagnostics: DiagnosticsState, onRefreshStats: () -> Unit, onRefreshTasks: () -> Unit, onRefreshClientStates: () -> Unit, onDisconnectAll: () -> Unit) {
    SectionCard(title = "Diagnostics") {
        DiagnosticDump("Runtime stats", diagnostics.runtimeStats, onRefreshStats)
        DiagnosticDump("Task list", diagnostics.taskList, onRefreshTasks)
        DiagnosticDump("Client states", diagnostics.clientStates, onRefreshClientStates)
        Button(onClick = onDisconnectAll) { Text("Disconnect all") }
    }
}

@Composable
private fun DangerZoneSection(onResetSecrets: () -> Unit, onRestart: () -> Unit) {
    SectionCard(title = "Danger Zone") {
        Button(onClick = onResetSecrets, colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.errorContainer)) { Text("Reset secrets") }
        Button(onClick = onRestart, colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.errorContainer)) { Text("Restart device") }
    }
}
```

No icon library is a dependency of this module — the +/- stepper buttons are
plain `Button`s with text glyphs; don't add `material-icons-extended` for two
glyphs.

`SectionCard`, `LabeledValue`, `DiagnosticDump` are small private helper
composables in the same file (a `Card` + `Column` wrapper, a label/value
`Row`, and a dump-with-refresh-button `Column` showing the text in a
`Text` with `maxLines`/scroll — no need for a dedicated file, each has one
caller).

`Diagnostics` dump text panels: wrap the `Text` in a fixed-height scrollable
`Column` (`Modifier.verticalScroll(rememberScrollState()).heightIn(max = 160.dp)`)
so a multi-hundred-line task list doesn't blow out the screen — `foundation`
is already a dependency, no new one needed.

## Ordering / verification

1. Write `DeviceViewModel.kt` first (state + logic) — it has no Compose
   dependency on the screen file, so it can be checked against
   `DeviceViewModelTest.kt`'s existing assertion in isolation.
2. Write `ConfirmActionSheet.kt` (depends only on the `ConfirmAction` type
   from step 1).
3. Write `DeviceScreen.kt` last (depends on both).
4. Since no build is available here: re-read the finished
   `DeviceViewModel.kt` against the opcode table above one more time,
   opcode by opcode, confirming every request payload length/index and every
   response payload index matches `pgp_control.c` exactly (this is the one
   category of bug a missing build can't catch — wrong byte index reads
   garbage silently).
5. Whoever executes this: `make format`, then hand to the maintainer for an
   on-device build/run — this repo's `AGENTS.md` reserves ESP-IDF-side
   builds for the maintainer, and there's no such restriction on Android
   Gradle builds, but no Gradle build is being run as part of *this* plan
   per the orchestrator's instruction. Add/extend a small
   `DeviceViewModelTest` case (existing file, e.g. asserting
   `refreshStatus()`'s decode against a `FakeBleControlRepository` stub
   response) is left as the natural next step for whoever executes, not
   written here since this plan produces no code.
