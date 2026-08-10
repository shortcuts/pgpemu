package com.pgpemu.companion.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.Opcode
import com.pgpemu.companion.ble.ResponseFrame
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

// ponytail: CONFIG_BT_ACL_CONNECTIONS is compile-time on the firmware side
// (pgpemu-esp32/sdkconfig) with no opcode to read it at runtime; hardcode
// here, bump both sides together if the firmware value ever changes.
const val DEVICE_PROFILE_COUNT = 4
const val MAX_CONNECTIONS_LIMIT = 4

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
    val ledOn: Boolean? = null,
    val advertisingEnabled: Boolean? = null,
    val activeConnections: Int? = null,
    val logLevel: Int? = null,
)

data class ProfileState(
    val index: Int,
    val autospin: Boolean? = null,
    val autocatch: Boolean? = null,
    val caught: Int? = null,
    val fled: Int? = null,
    val spin: Int? = null,
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

@HiltViewModel
class DeviceViewModel @Inject constructor(
    private val repository: BleControlRepository,
) : ViewModel() {

    private val _uiState = MutableStateFlow(DeviceUiState())
    val uiState: StateFlow<DeviceUiState> = _uiState.asStateFlow()

    init {
        viewModelScope.launch {
            repository.connectionState.collect { newState ->
                _uiState.update { it.copy(connectionState = newState) }
                if (newState is ConnectionState.Ready) refreshStatus()
            }
        }
    }

    fun connect() {
        viewModelScope.launch { repository.connect() }
    }

    fun disconnect() {
        viewModelScope.launch { repository.disconnect() }
    }

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
        runCommand(Opcode.GET_CLIENT_STATES) { frame ->
            val autoStates = parseProfileAutoStates(String(frame.payload, Charsets.UTF_8))
            _uiState.update { s ->
                s.copy(profiles = s.profiles.map { p ->
                    autoStates[p.index]?.let { p.copy(autospin = it.autospin, autocatch = it.autocatch) } ?: p
                })
            }
        }
    }

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

    private fun refreshDiagnostic(opcode: Int, apply: (DiagnosticsState, String) -> DiagnosticsState) {
        runCommand(opcode) { frame ->
            val text = String(frame.payload, Charsets.UTF_8)
            _uiState.update { it.copy(diagnostics = apply(it.diagnostics, text)) }
        }
    }

    fun refreshRuntimeStats() {
        if (_uiState.value.isBusy) return
        viewModelScope.launch {
            _uiState.update { it.copy(isBusy = true, errorMessage = null) }

            val slotToConnId = repository.sendCommand(Opcode.GET_CLIENT_STATES, ByteArray(0))
                .getOrNull()
                ?.takeIf { it.isOk }
                ?.let { parseSlotToConnId(String(it.payload, Charsets.UTF_8)) }
                ?: emptyMap()

            repository.sendCommand(Opcode.GET_RUNTIME_STATS, ByteArray(0)).fold(
                onSuccess = { frame ->
                    if (frame.isOk) {
                        val text = String(frame.payload, Charsets.UTF_8)
                        val connStats = parseConnStats(text)
                        _uiState.update { s ->
                            s.copy(
                                diagnostics = s.diagnostics.copy(runtimeStats = text),
                                profiles = s.profiles.map { profile ->
                                    val stat = slotToConnId[profile.index]?.let { connStats[it] }
                                    profile.copy(caught = stat?.caught, fled = stat?.fled, spin = stat?.spin)
                                },
                            )
                        }
                    } else {
                        _uiState.update { it.copy(errorMessage = "device returned status ${frame.status}") }
                    }
                },
                onFailure = { e -> _uiState.update { it.copy(errorMessage = e.message ?: "command failed") } },
            )

            _uiState.update { it.copy(isBusy = false) }
        }
    }
    fun refreshTaskList() = refreshDiagnostic(Opcode.GET_TASK_LIST) { d, text -> d.copy(taskList = text) }
    fun refreshClientStates() = refreshDiagnostic(Opcode.GET_CLIENT_STATES) { d, text -> d.copy(clientStates = text) }

    fun disconnectAllClients() {
        runCommand(Opcode.RESET_CLIENT_STATES) { refreshClientStates() }
    }

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
}

private data class ConnRuntimeStats(val caught: Int, val fled: Int, val spin: Int)

// Matches "<slot>: <conn_id in hex>" lines from GET_CLIENT_STATES's
// conn_id_map section (pgp_handshake_multi.c: dump_client_states_format).
// "ffff" is the firmware's empty-slot sentinel.
private val CONN_ID_MAP_LINE = Regex("""^(\d+): ([0-9a-f]{4})$""")

private fun parseSlotToConnId(text: String): Map<Int, Int> {
    val result = mutableMapOf<Int, Int>()
    for (line in text.lineSequence()) {
        val match = CONN_ID_MAP_LINE.matchEntire(line.trim()) ?: continue
        val connIdHex = match.groupValues[2]
        if (connIdHex == "ffff") continue
        result[match.groupValues[1].toInt()] = connIdHex.toInt(16)
    }
    return result
}

// Matches GET_RUNTIME_STATS blocks from stats.c: stats_format_runtime.
private val RUNTIME_STATS_BLOCK = Regex(
    "Connection (\\d+):\\n- Caught: (\\d+)\\n- Fled: (\\d+)\\n- Spin: (\\d+)",
)

private fun parseConnStats(text: String): Map<Int, ConnRuntimeStats> =
    RUNTIME_STATS_BLOCK.findAll(text).associate { m ->
        m.groupValues[1].toInt() to ConnRuntimeStats(
            caught = m.groupValues[2].toInt(),
            fled = m.groupValues[3].toInt(),
            spin = m.groupValues[4].toInt(),
        )
    }

private data class ProfileAutoState(val autospin: Boolean, val autocatch: Boolean)

// Matches a "[i] conn_id=... / handshake=... / autospin=on autocatch=off"
// block from GET_CLIENT_STATES's client_states section
// (pgp_handshake_multi.c: dump_client_states_format). The autospin/autocatch
// line is only present when the firmware slot has settings attached
// (entry->settings != NULL), so it's an optional group here too.
private val CLIENT_STATE_AUTO_BLOCK = Regex(
    """^\[(\d+)] conn_id=.*\n {2}handshake=.*\n(?: {2}autospin=(on|off) autocatch=(on|off)\n)?""",
    RegexOption.MULTILINE,
)

private fun parseProfileAutoStates(text: String): Map<Int, ProfileAutoState> =
    CLIENT_STATE_AUTO_BLOCK.findAll(text).mapNotNull { m ->
        val spin = m.groups[2]?.value ?: return@mapNotNull null
        val catch = m.groups[3]?.value ?: return@mapNotNull null
        m.groupValues[1].toInt() to ProfileAutoState(spin == "on", catch == "on")
    }.toMap()
