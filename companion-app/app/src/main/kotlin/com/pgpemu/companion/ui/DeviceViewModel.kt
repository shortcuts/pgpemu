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
    val connected: Boolean = false,
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

    private suspend fun runStep(opcode: Int, payload: ByteArray = ByteArray(0), onOk: (ResponseFrame) -> Unit) {
        val result = repository.sendCommand(opcode, payload)
        result.fold(
            onSuccess = { frame ->
                if (frame.isOk) onOk(frame) else _uiState.update { it.copy(errorMessage = "device returned status ${frame.status}") }
            },
            onFailure = { e -> _uiState.update { it.copy(errorMessage = e.message ?: "command failed") } },
        )
    }

    private fun runCommand(opcode: Int, payload: ByteArray = ByteArray(0), onOk: (ResponseFrame) -> Unit) {
        if (_uiState.value.isBusy) return // one in-flight command at a time — NordicBleControlRepository has a single pendingResponse slot
        viewModelScope.launch {
            _uiState.update { it.copy(isBusy = true, errorMessage = null) }
            runStep(opcode, payload, onOk)
            _uiState.update { it.copy(isBusy = false) }
        }
    }

    /** Requests current device state after connect — runs its GET commands sequentially,
     * since NordicBleControlRepository only has one pendingResponse slot at a time. */
    fun refreshStatus() {
        if (_uiState.value.isBusy) return
        viewModelScope.launch {
            _uiState.update { it.copy(isBusy = true, errorMessage = null) }
            runStep(Opcode.GET_GLOBAL_SETTINGS) { frame ->
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
            runStep(Opcode.GET_LED_STATE) { frame ->
                _uiState.update { it.copy(status = it.status.copy(ledOn = frame.payload[0] == 1.toByte())) }
            }
            runStep(Opcode.GET_CLIENT_SUMMARY) { frame ->
                val summaries = parseClientSummary(frame.payload)
                _uiState.update { s ->
                    s.copy(profiles = s.profiles.mapIndexed { i, p ->
                        val summary = summaries[i]
                        p.copy(
                            connected = summary.connected,
                            autospin = summary.autospin ?: p.autospin,
                            autocatch = summary.autocatch ?: p.autocatch,
                        )
                    })
                }
            }
            _uiState.update { it.copy(isBusy = false) }
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

            repository.sendCommand(Opcode.GET_RUNTIME_STATS, ByteArray(0)).fold(
                onSuccess = { frame ->
                    if (frame.isOk) {
                        val text = String(frame.payload, Charsets.UTF_8)
                        _uiState.update { it.copy(diagnostics = it.diagnostics.copy(runtimeStats = text)) }
                    } else {
                        _uiState.update { it.copy(errorMessage = "device returned status ${frame.status}") }
                    }
                },
                onFailure = { e -> _uiState.update { it.copy(errorMessage = e.message ?: "command failed") } },
            )

            repository.sendCommand(Opcode.GET_CLIENT_SUMMARY, ByteArray(0)).fold(
                onSuccess = { frame ->
                    if (frame.isOk) {
                        val summaries = parseClientSummary(frame.payload)
                        _uiState.update { s ->
                            s.copy(profiles = s.profiles.mapIndexed { i, p ->
                                val summary = summaries[i]
                                p.copy(
                                    connected = summary.connected,
                                    caught = summary.caught,
                                    fled = summary.fled,
                                    spin = summary.spin,
                                )
                            })
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

private data class ClientSummary(
    val connected: Boolean,
    val autospin: Boolean?,
    val autocatch: Boolean?,
    val caught: Int?,
    val fled: Int?,
    val spin: Int?,
)

private const val CLIENT_SUMMARY_RECORD_SIZE = 11

// Decodes CONTROL_OP_GET_CLIENT_SUMMARY's fixed-layout binary payload —
// DEVICE_PROFILE_COUNT records of CLIENT_SUMMARY_RECORD_SIZE bytes each,
// see pgp_control.c's CONTROL_OP_GET_CLIENT_SUMMARY case for the layout.
private fun parseClientSummary(payload: ByteArray): List<ClientSummary> =
    (0 until DEVICE_PROFILE_COUNT).map { slot ->
        val base = slot * CLIENT_SUMMARY_RECORD_SIZE
        fun u16(offset: Int) = (payload[base + offset].toInt() and 0xFF) or
            ((payload[base + offset + 1].toInt() and 0xFF) shl 8)
        val connId = u16(0)
        val flags = payload[base + 2].toInt() and 0xFF
        val hasSettings = flags and 0x01 != 0
        val hasStats = flags and 0x02 != 0
        ClientSummary(
            connected = connId != 0xFFFF,
            autospin = if (hasSettings) payload[base + 3] == 1.toByte() else null,
            autocatch = if (hasSettings) payload[base + 4] == 1.toByte() else null,
            caught = if (hasStats) u16(5) else null,
            fled = if (hasStats) u16(7) else null,
            spin = if (hasStats) u16(9) else null,
        )
    }
