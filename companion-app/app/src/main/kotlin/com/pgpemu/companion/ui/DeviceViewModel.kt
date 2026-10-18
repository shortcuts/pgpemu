package com.pgpemu.companion.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.pgpemu.companion.R
import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.DeviceInfo
import com.pgpemu.companion.ble.Opcode
import com.pgpemu.companion.ble.ResponseFrame
import com.pgpemu.companion.ble.ScannedDevice
import com.pgpemu.companion.ble.StatusCode
import com.pgpemu.companion.ble.decodeClientSummary
import com.pgpemu.companion.ble.decodeDeviceInfo
import com.pgpemu.companion.ble.decodeFlag
import com.pgpemu.companion.ble.decodeGlobalSettings
import com.pgpemu.companion.ble.decodeLogLevel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import javax.inject.Inject

// Used until GET_DEVICE_INFO answers, and for firmware too old to answer it (those builds have 4 slots).
const val DEFAULT_PROFILE_COUNT = 4

// Oldest GET_DEVICE_INFO layout version this app decodes.
const val MIN_PROTOCOL_VERSION = 1

// Poll interval while connected — other phones can connect/disconnect and
// toggle profiles without this app knowing, so state can go stale otherwise.
const val STATUS_POLL_INTERVAL_MS = 5000L

data class DeviceUiState(
    val connectionState: ConnectionState = ConnectionState.Idle,
    val discoveredDevices: List<ScannedDevice> = emptyList(),
    val hasScanned: Boolean = false,
    val status: StatusState = StatusState(),
    val profiles: List<ProfileState> = List(DEFAULT_PROFILE_COUNT) { ProfileState(index = it) },
    val settings: SettingsState = SettingsState(),
    val diagnostics: DiagnosticsState = DiagnosticsState(),
    val isBusy: Boolean = false,
    val message: UiMessage? = null,
    val pendingConfirmation: ConfirmAction? = null,
    val deviceInfo: DeviceInfo? = null,
    /** The device cannot answer GET_DEVICE_INFO, or speaks a protocol older than this app supports. */
    val firmwareOutdated: Boolean = false,
)

/** Text for the user; the screen maps each case to a string resource, raw codes ride along for bug reports. */
sealed interface UiMessage {
    val isError: Boolean get() = true

    data class DeviceStatus(
        val code: Int,
    ) : UiMessage

    data class CommandFailed(
        val detail: String?,
    ) : UiMessage

    data object UnexpectedReply : UiMessage

    data object FirmwareOutdated : UiMessage

    data object Saved : UiMessage {
        override val isError get() = false
    }
}

data class StatusState(
    val ledOn: Boolean? = null,
    val advertisingEnabled: Boolean? = null,
    /** Radio state (starting or on); null when the device firmware is too old to report it. */
    val advertisingActive: Boolean? = null,
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
    val titleRes: Int
    val bodyRes: Int

    data object ResetSecrets : ConfirmAction {
        override val confirmWord = "RESET"
        override val titleRes = R.string.confirm_reset_title
        override val bodyRes = R.string.confirm_reset_body
    }

    data object Restart : ConfirmAction {
        override val confirmWord = "RESTART"
        override val titleRes = R.string.danger_restart
        override val bodyRes = R.string.confirm_restart_body
    }
}

@HiltViewModel
class DeviceViewModel
    @Inject
    constructor(
        private val repository: BleControlRepository,
    ) : ViewModel() {
        private val _uiState = MutableStateFlow(DeviceUiState())
        val uiState: StateFlow<DeviceUiState> = _uiState.asStateFlow()
        val associationRequests = repository.associationRequests

        private var pollJob: Job? = null

        // Default true: the ViewModel is created by a started screen, which then reports every change.
        private val foreground = MutableStateFlow(true)
        private val commandLock = Mutex() // user actions queue behind the running command instead of being dropped

        init {
            viewModelScope.launch {
                repository.discoveredDevices.collect { devices ->
                    _uiState.update { it.copy(discoveredDevices = devices) }
                }
            }
            viewModelScope.launch {
                repository.connectionState.collect { newState ->
                    _uiState.update {
                        // The next device may run other firmware, so forget what the last one reported.
                        if (newState is ConnectionState.Ready) {
                            it.copy(connectionState = newState)
                        } else {
                            it.copy(connectionState = newState, deviceInfo = null, firmwareOutdated = false)
                        }
                    }
                }
            }
            // Poll only while connected and visible: a stopped activity would keep the BLE link busy.
            viewModelScope.launch {
                combine(repository.connectionState, foreground) { state, fg -> state is ConnectionState.Ready && fg }
                    .distinctUntilChanged()
                    .collect { active ->
                        if (active) {
                            refreshStatus() // catch up on changes made while away
                            startPolling()
                        } else {
                            stopPolling()
                        }
                    }
            }
        }

        /** Called by the screen with its lifecycle: true at STARTED, false at STOPPED. */
        fun setForeground(visible: Boolean) {
            foreground.value = visible
        }

        /** Keeps connection count / active profile state fresh while connected — other
         * phones can connect or toggle profiles without this app being told directly. */
        private fun startPolling() {
            if (pollJob?.isActive == true) return
            pollJob =
                viewModelScope.launch {
                    while (isActive) {
                        delay(STATUS_POLL_INTERVAL_MS)
                        refreshStatus(clearError = false)
                    }
                }
        }

        private fun stopPolling() {
            pollJob?.cancel()
            pollJob = null
        }

        fun startScan() {
            _uiState.update { it.copy(hasScanned = true) }
            viewModelScope.launch { repository.startScan() }
        }

        fun connectTo(address: String) {
            viewModelScope.launch {
                repository.stopScan()
                repository.connect(address)
            }
        }

        fun consumeMessage() {
            _uiState.update { it.copy(message = null) }
        }

        fun disconnect() {
            viewModelScope.launch { repository.disconnect() }
        }

        private suspend fun runStep(
            opcode: Int,
            payload: ByteArray = ByteArray(0),
            onOk: (ResponseFrame) -> Unit,
        ) {
            val result = repository.sendCommand(opcode, payload)
            result.fold(
                onSuccess = { frame ->
                    if (frame.isOk) onOk(frame) else _uiState.update { it.copy(message = UiMessage.DeviceStatus(frame.status.toInt())) }
                },
                onFailure = { e -> _uiState.update { it.copy(message = UiMessage.CommandFailed(e.message)) } },
            )
        }

        private fun <T> decoded(
            result: Result<T>,
            apply: (T) -> Unit,
        ) {
            result.fold(
                onSuccess = apply,
                onFailure = { _uiState.update { s -> s.copy(message = UiMessage.UnexpectedReply) } },
            )
        }

        private fun runCommand(
            opcode: Int,
            payload: ByteArray = ByteArray(0),
            onOk: (ResponseFrame) -> Unit,
        ) {
            viewModelScope.launch {
                commandLock.withLock {
                    _uiState.update { it.copy(isBusy = true, message = null) }
                    runStep(opcode, payload, onOk)
                    _uiState.update { it.copy(isBusy = false) }
                }
            }
        }

        /** Requests current device state after connect — runs its GET commands sequentially,
         * since NordicBleControlRepository only has one pendingResponse slot at a time. */
        fun refreshStatus(clearError: Boolean = true) {
            // the poll skips a round when a command runs; user-initiated refreshes queue
            if (!clearError && commandLock.isLocked) return
            viewModelScope.launch {
                commandLock.withLock {
                    _uiState.update { it.copy(isBusy = true, message = if (clearError) null else it.message) }
                    if (_uiState.value.let { it.deviceInfo == null && !it.firmwareOutdated }) fetchDeviceInfo()
                    runStep(Opcode.GET_GLOBAL_SETTINGS) { frame ->
                        decoded(decodeGlobalSettings(frame.payload)) { g ->
                            _uiState.update {
                                it.copy(
                                    status =
                                        it.status.copy(
                                            logLevel = g.logLevel,
                                            advertisingEnabled = g.advertisingEnabled,
                                            advertisingActive = g.advertisingActive,
                                            activeConnections = g.activeConnections,
                                        ),
                                    settings = it.settings.copy(maxConnections = g.maxConnections),
                                )
                            }
                        }
                    }
                    runStep(Opcode.GET_LED_STATE) { frame ->
                        decoded(decodeFlag(frame.payload)) { on ->
                            _uiState.update { it.copy(status = it.status.copy(ledOn = on)) }
                        }
                    }
                    runStep(Opcode.GET_CLIENT_SUMMARY) { frame ->
                        decoded(decodeClientSummary(frame.payload)) { summaries ->
                            _uiState.update { s ->
                                s.copy(
                                    profiles =
                                        s.profiles.mapIndexed { i, p ->
                                            val summary = summaries.getOrNull(i) ?: return@mapIndexed p
                                            p.copy(
                                                connected = summary.connected,
                                                autospin = summary.autospin ?: p.autospin,
                                                autocatch = summary.autocatch ?: p.autocatch,
                                            )
                                        },
                                )
                            }
                        }
                    }
                    _uiState.update { it.copy(isBusy = false) }
                }
            }
        }

        /** Transport errors are ignored here: the next poll retries and the steps after it report them. */
        private suspend fun fetchDeviceInfo() {
            val frame = repository.sendCommand(Opcode.GET_DEVICE_INFO).getOrNull() ?: return
            when {
                frame.status == StatusCode.ERR_UNKNOWN_OPCODE -> markFirmwareOutdated()
                !frame.isOk -> _uiState.update { it.copy(message = UiMessage.DeviceStatus(frame.status.toInt())) }
                else ->
                    decoded(decodeDeviceInfo(frame.payload)) { info ->
                        if (info.protocolVersion < MIN_PROTOCOL_VERSION) {
                            markFirmwareOutdated()
                        } else {
                            _uiState.update { s ->
                                val slots = info.maxConnections.coerceAtLeast(1)
                                s.copy(
                                    deviceInfo = info,
                                    profiles = List(slots) { s.profiles.getOrNull(it) ?: ProfileState(index = it) },
                                )
                            }
                        }
                    }
            }
        }

        private fun markFirmwareOutdated() {
            _uiState.update { it.copy(firmwareOutdated = true, message = UiMessage.FirmwareOutdated) }
        }

        fun toggleAdvertising() {
            val turningOn = _uiState.value.status.advertisingEnabled != true
            val opcode = if (turningOn) Opcode.ADVERTISE_START else Opcode.ADVERTISE_STOP
            runCommand(opcode) { _uiState.update { it.copy(status = it.status.copy(advertisingEnabled = turningOn)) } }
        }

        fun setMaxConnections(value: Int) {
            val clamped = value.coerceIn(1, _uiState.value.deviceInfo?.maxConnections ?: DEFAULT_PROFILE_COUNT)
            runCommand(Opcode.SET_MAX_CONNECTIONS, byteArrayOf(clamped.toByte())) {
                _uiState.update { it.copy(settings = it.settings.copy(maxConnections = clamped)) }
            }
        }

        fun cycleLogLevel() {
            runCommand(Opcode.CYCLE_LOG_LEVEL) { frame ->
                decoded(decodeLogLevel(frame.payload)) { level ->
                    _uiState.update { it.copy(status = it.status.copy(logLevel = level)) }
                }
            }
        }

        fun saveSettings() {
            runCommand(Opcode.SAVE_SETTINGS) { _uiState.update { it.copy(message = UiMessage.Saved) } }
        }

        fun toggleAutospin(index: Int) {
            runCommand(Opcode.TOGGLE_AUTOSPIN, byteArrayOf(index.toByte())) { frame ->
                decoded(decodeFlag(frame.payload)) { on -> updateProfile(index) { it.copy(autospin = on) } }
            }
        }

        fun toggleAutocatch(index: Int) {
            runCommand(Opcode.TOGGLE_AUTOCATCH, byteArrayOf(index.toByte())) { frame ->
                decoded(decodeFlag(frame.payload)) { on -> updateProfile(index) { it.copy(autocatch = on) } }
            }
        }

        private fun updateProfile(
            index: Int,
            transform: (ProfileState) -> ProfileState,
        ) {
            _uiState.update { s -> s.copy(profiles = s.profiles.map { if (it.index == index) transform(it) else it }) }
        }

        private fun refreshDiagnostic(
            opcode: Int,
            apply: (DiagnosticsState, String) -> DiagnosticsState,
        ) {
            runCommand(opcode) { frame ->
                val text = String(frame.payload, Charsets.UTF_8)
                _uiState.update { it.copy(diagnostics = apply(it.diagnostics, text)) }
            }
        }

        fun refreshRuntimeStats() {
            viewModelScope.launch {
                commandLock.withLock {
                    _uiState.update { it.copy(isBusy = true, message = null) }

                    repository.sendCommand(Opcode.GET_RUNTIME_STATS, ByteArray(0)).fold(
                        onSuccess = { frame ->
                            if (frame.isOk) {
                                val text = String(frame.payload, Charsets.UTF_8)
                                _uiState.update { it.copy(diagnostics = it.diagnostics.copy(runtimeStats = text)) }
                            } else {
                                _uiState.update { it.copy(message = UiMessage.DeviceStatus(frame.status.toInt())) }
                            }
                        },
                        onFailure = { e -> _uiState.update { it.copy(message = UiMessage.CommandFailed(e.message)) } },
                    )

                    repository.sendCommand(Opcode.GET_CLIENT_SUMMARY, ByteArray(0)).fold(
                        onSuccess = { frame ->
                            if (frame.isOk) {
                                decoded(decodeClientSummary(frame.payload)) { summaries ->
                                    _uiState.update { s ->
                                        s.copy(
                                            profiles =
                                                s.profiles.mapIndexed { i, p ->
                                                    val summary = summaries.getOrNull(i) ?: return@mapIndexed p
                                                    p.copy(
                                                        connected = summary.connected,
                                                        caught = summary.caught,
                                                        fled = summary.fled,
                                                        spin = summary.spin,
                                                    )
                                                },
                                        )
                                    }
                                }
                            } else {
                                _uiState.update { it.copy(message = UiMessage.DeviceStatus(frame.status.toInt())) }
                            }
                        },
                        onFailure = { e -> _uiState.update { it.copy(message = UiMessage.CommandFailed(e.message)) } },
                    )

                    _uiState.update { it.copy(isBusy = false) }
                }
            }
        }

        fun refreshTaskList() = refreshDiagnostic(Opcode.GET_TASK_LIST) { d, text -> d.copy(taskList = text) }

        fun refreshClientStates() = refreshDiagnostic(Opcode.GET_CLIENT_STATES) { d, text -> d.copy(clientStates = text) }

        fun disconnectAllClients() {
            runCommand(Opcode.RESET_CLIENT_STATES) { refreshClientStates() }
        }

        fun requestResetSecrets() {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.ResetSecrets) }
        }

        fun requestRestart() {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.Restart) }
        }

        fun dismissConfirmation() {
            _uiState.update { it.copy(pendingConfirmation = null) }
        }

        fun confirmPendingAction() {
            when (_uiState.value.pendingConfirmation) {
                ConfirmAction.ResetSecrets -> runCommand(Opcode.RESET_SECRETS) { dismissConfirmation() }
                ConfirmAction.Restart -> runCommand(Opcode.RESTART) { dismissConfirmation() }
                null -> Unit
            }
        }
    }
