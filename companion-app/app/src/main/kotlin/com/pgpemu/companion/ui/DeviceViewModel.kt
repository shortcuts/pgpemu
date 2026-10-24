package com.pgpemu.companion.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.pgpemu.companion.R
import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.CrashReport
import com.pgpemu.companion.ble.DeviceEvent
import com.pgpemu.companion.ble.DeviceInfo
import com.pgpemu.companion.ble.EventType
import com.pgpemu.companion.ble.KnownDevice
import com.pgpemu.companion.ble.LastDeviceStore
import com.pgpemu.companion.ble.LifetimeStats
import com.pgpemu.companion.ble.Opcode
import com.pgpemu.companion.ble.OtaException
import com.pgpemu.companion.ble.OtaImage
import com.pgpemu.companion.ble.OtaTransfer
import com.pgpemu.companion.ble.ResponseFrame
import com.pgpemu.companion.ble.ScannedDevice
import com.pgpemu.companion.ble.StatusCode
import com.pgpemu.companion.ble.decodeClientSummary
import com.pgpemu.companion.ble.decodeCrashReport
import com.pgpemu.companion.ble.decodeDeviceInfo
import com.pgpemu.companion.ble.decodeFlag
import com.pgpemu.companion.ble.decodeGlobalSettings
import com.pgpemu.companion.ble.decodeKnownDevices
import com.pgpemu.companion.ble.decodeLifetimeStats
import com.pgpemu.companion.ble.decodeLogLevel
import com.pgpemu.companion.ble.encodeRename
import com.pgpemu.companion.ble.parseAddress
import com.pgpemu.companion.ble.parseOtaImage
import com.pgpemu.companion.ble.parseSecretsCsv
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
const val MIN_PROTOCOL_VERSION = 4

// First GET_DEVICE_INFO layout version with the OTA opcodes.
const val OTA_MIN_PROTOCOL_VERSION = 5

// First GET_DEVICE_INFO layout version with the known-device opcodes.
const val KNOWN_DEVICES_MIN_PROTOCOL_VERSION = 6

// First GET_DEVICE_INFO layout version with the Event characteristic.
const val EVENTS_MIN_PROTOCOL_VERSION = 7

// First GET_DEVICE_INFO layout version with the lifetime stats opcodes.
const val LIFETIME_MIN_PROTOCOL_VERSION = 8

// First GET_DEVICE_INFO layout version with the crash report opcodes.
const val CRASH_MIN_PROTOCOL_VERSION = 9

// Event-capable firmware pushes changes; the poll only covers what raises no event (other phones' toggles).
const val EVENT_POLL_INTERVAL_MS = 30_000L

const val ACTIVITY_LIMIT = 50

const val NO_WARNING = 0

private val WARNING_EVENTS = setOf(EventType.BAG_FULL, EventType.BOX_FULL, EventType.BALLS_EMPTY)

// The firmware clears a warning on these patterns; the other events leave it alone.
private val WARNING_CLEARING_EVENTS =
    setOf(
        EventType.POKEMON_IN_RANGE,
        EventType.NEW_POKEMON_IN_RANGE,
        EventType.POKESTOP_IN_RANGE,
        EventType.CAUGHT,
        EventType.FLED,
        EventType.ITEMS_RECEIVED,
    )

// Events after which counters or connections changed; the in-range events only feed the activity list.
private val REFRESH_EVENTS =
    setOf(
        EventType.GAME_CONNECTED,
        EventType.GAME_DISCONNECTED,
        EventType.CAUGHT,
        EventType.FLED,
        EventType.ITEMS_RECEIVED,
        EventType.BAG_FULL,
        EventType.BOX_FULL,
        EventType.BALLS_EMPTY,
    )

// Poll interval while connected — other phones can connect/disconnect and
// toggle profiles without this app knowing, so state can go stale otherwise.
const val STATUS_POLL_INTERVAL_MS = 5000L

// Waits before each reconnect attempt after an unexpected drop; the loop stops after the last one.
val RECONNECT_BACKOFF_MS = longArrayOf(1000, 2000, 4000, 8000, 16000)

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
    /** Percent of the firmware image sent; null while no update runs. */
    val otaProgress: Int? = null,
    /** Game phones the device has seen, newest first. */
    val knownDevices: List<KnownDevice> = emptyList(),
    /** Lifetime and session counters by phone address; empty when the firmware has none. */
    val knownLifetime: Map<String, LifetimeStats> = emptyMap(),
    /** Core dump summary of the last panic; null when the device holds none. */
    val crashReport: CrashReport? = null,
    /** Device Events since the last connect, newest first; the phone clock stamps them. */
    val activity: List<ActivityItem> = emptyList(),
) {
    val eventsSupported: Boolean
        get() = (deviceInfo?.protocolVersion ?: 0) >= EVENTS_MIN_PROTOCOL_VERSION && deviceInfo?.provisioning == false
    val otaSupported: Boolean get() = (deviceInfo?.protocolVersion ?: 0) >= OTA_MIN_PROTOCOL_VERSION
    val knownDevicesSupported: Boolean get() = (deviceInfo?.protocolVersion ?: 0) >= KNOWN_DEVICES_MIN_PROTOCOL_VERSION
    val crashSupported: Boolean get() = (deviceInfo?.protocolVersion ?: 0) >= CRASH_MIN_PROTOCOL_VERSION
    val lifetimeSupported: Boolean get() = knownDevicesSupported && (deviceInfo?.protocolVersion ?: 0) >= LIFETIME_MIN_PROTOCOL_VERSION
}

data class ActivityItem(
    val timeMillis: Long,
    val type: Int,
    val slot: Int,
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

    data class InvalidSecretsFile(
        val detail: String?,
    ) : UiMessage

    data class InvalidFirmwareFile(
        val detail: String?,
    ) : UiMessage

    /** The new image is selected and the device is restarting into it. */
    data object FirmwareInstalled : UiMessage {
        override val isError = false
    }

    /** The device stored the secrets and is restarting; it reappears under its new Bluetooth address. */
    data object SecretsProvisioned : UiMessage {
        override val isError = false
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
    /** Last bag/box/balls warning of the game; an [EventType] value, or [NO_WARNING]. */
    val warning: Int = NO_WARNING,
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

    data object ForgetPairings : ConfirmAction {
        override val confirmWord = "FORGET"
        override val titleRes = R.string.confirm_forget_title
        override val bodyRes = R.string.confirm_forget_body
    }

    data object UpdateFirmware : ConfirmAction {
        override val confirmWord = "UPDATE"
        override val titleRes = R.string.confirm_ota_title
        override val bodyRes = R.string.confirm_ota_body
    }

    data class ForgetKnownDevice(
        val address: String,
    ) : ConfirmAction {
        override val confirmWord = "FORGET"
        override val titleRes = R.string.confirm_forget_known_title
        override val bodyRes = R.string.confirm_forget_known_body
    }

    data class ResetLifetimeStats(
        val address: String,
    ) : ConfirmAction {
        override val confirmWord = "RESET"
        override val titleRes = R.string.confirm_reset_stats_title
        override val bodyRes = R.string.confirm_reset_stats_body
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
        private val lastDevice: LastDeviceStore,
    ) : ViewModel() {
        private val _uiState = MutableStateFlow(DeviceUiState())
        val uiState: StateFlow<DeviceUiState> = _uiState.asStateFlow()
        val associationRequests = repository.associationRequests

        private var pollJob: Job? = null
        private var pendingOta: OtaImage? = null
        private var reconnectJob: Job? = null
        private var currentAddress: String? = null
        private var userDisconnected = false
        private var resumed = false
        private var lastEventSeq: Int? = null

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
                var previous: ConnectionState = ConnectionState.Idle
                repository.connectionState.collect { newState ->
                    _uiState.update {
                        // The next device may run other firmware, so forget what the last one reported.
                        if (newState is ConnectionState.Ready) {
                            it.copy(connectionState = newState)
                        } else {
                            it.copy(connectionState = newState, deviceInfo = null, firmwareOutdated = false)
                        }
                    }
                    lastEventSeq = null // the device restarts the sequence for each link
                    if (newState is ConnectionState.Ready) {
                        reconnectJob?.cancel()
                        currentAddress?.let { lastDevice.address = it }
                    } else if (previous is ConnectionState.Ready && newState is ConnectionState.Disconnected && !userDisconnected) {
                        reconnect()
                    }
                    previous = newState
                }
            }
            viewModelScope.launch { repository.events.collect(::onEvent) }
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

        private fun onEvent(event: DeviceEvent) {
            val lost = event.seq != (((lastEventSeq ?: -1) + 1) and 0xFF)
            lastEventSeq = event.seq
            _uiState.update {
                it.copy(
                    activity =
                        (
                            listOf(
                                ActivityItem(System.currentTimeMillis(), event.type, event.slot),
                            ) + it.activity
                        ).take(ACTIVITY_LIMIT),
                    profiles =
                        it.profiles.map { p ->
                            when {
                                p.index != event.slot -> p
                                event.type in WARNING_EVENTS -> p.copy(warning = event.type)
                                event.type in WARNING_CLEARING_EVENTS -> p.copy(warning = NO_WARNING)
                                else -> p
                            }
                        },
                )
            }
            if ((lost || event.type in REFRESH_EVENTS) && foreground.value) refreshStatus(clearError = false)
        }

        fun clearActivity() {
            _uiState.update { it.copy(activity = emptyList()) }
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
                    var idleMs = 0L
                    while (isActive) {
                        delay(STATUS_POLL_INTERVAL_MS)
                        idleMs += STATUS_POLL_INTERVAL_MS
                        // Device info arrives after polling starts, so the interval is decided per tick.
                        if (!_uiState.value.eventsSupported || idleMs >= EVENT_POLL_INTERVAL_MS) {
                            idleMs = 0
                            refreshStatus(clearError = false)
                        }
                    }
                }
        }

        private fun stopPolling() {
            pollJob?.cancel()
            pollJob = null
        }

        private fun reconnect() {
            val address = currentAddress ?: return
            reconnectJob?.cancel()
            reconnectJob =
                viewModelScope.launch {
                    for (wait in RECONNECT_BACKOFF_MS) {
                        delay(wait)
                        repository.connect(address)
                        if (repository.connectionState.value is ConnectionState.Ready) return@launch
                    }
                }
        }

        /** Connects to the remembered device once per ViewModel, without a scan. */
        fun resumeLastDevice() {
            val address = lastDevice.address ?: return
            if (resumed || _uiState.value.connectionState !is ConnectionState.Idle) return
            resumed = true
            connectTo(address)
        }

        fun startScan() {
            reconnectJob?.cancel()
            _uiState.update { it.copy(hasScanned = true) }
            viewModelScope.launch { repository.startScan() }
        }

        fun connectTo(address: String) {
            reconnectJob?.cancel()
            currentAddress = address
            userDisconnected = false
            _uiState.update { it.copy(activity = emptyList()) }
            viewModelScope.launch {
                repository.stopScan()
                repository.connect(address)
            }
        }

        fun consumeMessage() {
            _uiState.update { it.copy(message = null) }
        }

        fun disconnect() {
            userDisconnected = true
            reconnectJob?.cancel()
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
                    // A device without secrets runs only the Control Service: nothing else to read.
                    if (_uiState.value.deviceInfo?.provisioning == true) {
                        _uiState.update { it.copy(isBusy = false) }
                        return@withLock
                    }
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
                                                warning = summary.warning,
                                            )
                                        },
                                )
                            }
                        }
                    }
                    if (clearError) {
                        fetchKnownDevices()
                        fetchCrashReport()
                    }
                    _uiState.update { it.copy(isBusy = false) }
                }
            }
        }

        /** Runs inside the command lock. The list changes only on a handshake, rename or forget, so the poll skips it. */
        private suspend fun fetchKnownDevices() {
            if (!_uiState.value.knownDevicesSupported) return
            runStep(Opcode.LIST_KNOWN_DEVICES) { frame ->
                decoded(decodeKnownDevices(frame.payload)) { devices -> _uiState.update { it.copy(knownDevices = devices) } }
            }
            if (_uiState.value.lifetimeSupported) fetchLifetime()
        }

        /** Runs inside the command lock. The report changes only at boot or on dismiss, so the poll skips it. */
        private suspend fun fetchCrashReport() {
            if (!_uiState.value.crashSupported) return
            runStep(Opcode.GET_CRASH_REPORT) { frame ->
                decoded(decodeCrashReport(frame.payload)) { report -> _uiState.update { it.copy(crashReport = report) } }
            }
        }

        fun dismissCrashReport() {
            runCommand(Opcode.CLEAR_CRASH_REPORT) { _uiState.update { it.copy(crashReport = null) } }
        }

        /** Runs inside the command lock; one request per phone, since a list reply would not fit one response. */
        private suspend fun fetchLifetime() {
            val fetched = mutableMapOf<String, LifetimeStats>()
            for (device in _uiState.value.knownDevices) {
                runStep(Opcode.GET_LIFETIME_STATS, parseAddress(device.address)) { frame ->
                    decoded(decodeLifetimeStats(frame.payload)) { fetched[device.address] = it }
                }
            }
            _uiState.update { it.copy(knownLifetime = fetched) }
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
            // Send the target, not "next": a retried frame then cannot step the level twice.
            val target = ((_uiState.value.status.logLevel ?: 0) % 3) + 1
            runCommand(Opcode.SET_LOG_LEVEL, byteArrayOf(target.toByte())) { frame ->
                decoded(decodeLogLevel(frame.payload)) { level ->
                    _uiState.update { it.copy(status = it.status.copy(logLevel = level)) }
                }
            }
        }

        fun toggleAutospin(index: Int) {
            val target =
                _uiState.value.profiles
                    .getOrNull(index)
                    ?.autospin != true
            runCommand(Opcode.SET_AUTOSPIN, byteArrayOf(index.toByte(), if (target) 1 else 0)) { frame ->
                decoded(decodeFlag(frame.payload)) { on -> updateProfile(index) { it.copy(autospin = on) } }
            }
        }

        fun toggleAutocatch(index: Int) {
            val target =
                _uiState.value.profiles
                    .getOrNull(index)
                    ?.autocatch != true
            runCommand(Opcode.SET_AUTOCATCH, byteArrayOf(index.toByte(), if (target) 1 else 0)) { frame ->
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

        fun requestForgetPairings() {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.ForgetPairings) }
        }

        fun requestResetSecrets() {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.ResetSecrets) }
        }

        /** Uploads the picked secrets.csv text, then restarts the device into normal mode. */
        fun importSecrets(text: String?) {
            val parsed = parseSecretsCsv(text.orEmpty())
            val secrets = parsed.getOrNull()
            if (secrets == null) {
                _uiState.update { it.copy(message = UiMessage.InvalidSecretsFile(parsed.exceptionOrNull()?.message)) }
                return
            }
            runCommand(Opcode.PROVISION_SECRETS, secrets.toPayload()) {
                runCommand(Opcode.RESTART) {
                    _uiState.update { it.copy(message = UiMessage.SecretsProvisioned) }
                }
            }
        }

        /** Checks the picked pgpemu.bin and asks for confirmation; the device must be armed with its board button. */
        fun requestFirmwareUpdate(bytes: ByteArray?) {
            if (!_uiState.value.otaSupported) return
            val parsed = parseOtaImage(bytes ?: ByteArray(0))
            val image = parsed.getOrNull()
            if (image == null) {
                _uiState.update { it.copy(message = UiMessage.InvalidFirmwareFile(parsed.exceptionOrNull()?.message)) }
                return
            }
            pendingOta = image
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.UpdateFirmware) }
        }

        private fun startOta() {
            val image = pendingOta ?: return
            pendingOta = null
            viewModelScope.launch {
                commandLock.withLock {
                    _uiState.update { it.copy(isBusy = true, message = null, otaProgress = 0) }
                    try {
                        OtaTransfer(repository).run(image).collect { p -> _uiState.update { it.copy(otaProgress = p.percent) } }
                        // The new image only boots after a restart; END already acknowledged it.
                        runStep(Opcode.RESTART) { _uiState.update { it.copy(message = UiMessage.FirmwareInstalled) } }
                    } catch (e: OtaException) {
                        val status = e.status
                        _uiState.update {
                            it.copy(
                                message =
                                    if (status != null) UiMessage.DeviceStatus(status.toInt()) else UiMessage.CommandFailed(e.message),
                            )
                        }
                    } finally {
                        _uiState.update { it.copy(isBusy = false, otaProgress = null) }
                    }
                }
            }
        }

        fun renameKnownDevice(
            address: String,
            name: String,
        ) {
            runCommand(Opcode.RENAME_KNOWN_DEVICE, encodeRename(address, name)) { refreshKnownDevices() }
        }

        fun requestForgetKnownDevice(address: String) {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.ForgetKnownDevice(address)) }
        }

        fun requestResetLifetimeStats(address: String) {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.ResetLifetimeStats(address)) }
        }

        private fun refreshKnownDevices() {
            viewModelScope.launch { commandLock.withLock { fetchKnownDevices() } }
        }

        fun requestRestart() {
            _uiState.update { it.copy(pendingConfirmation = ConfirmAction.Restart) }
        }

        fun dismissConfirmation() {
            pendingOta = null
            _uiState.update { it.copy(pendingConfirmation = null) }
        }

        fun confirmPendingAction() {
            when (val action = _uiState.value.pendingConfirmation) {
                ConfirmAction.ResetSecrets -> runCommand(Opcode.RESET_SECRETS) { dismissConfirmation() }
                ConfirmAction.ForgetPairings -> runCommand(Opcode.FORGET_PAIRINGS) { dismissConfirmation() }
                ConfirmAction.UpdateFirmware -> {
                    _uiState.update { it.copy(pendingConfirmation = null) }
                    startOta()
                }
                is ConfirmAction.ForgetKnownDevice ->
                    runCommand(Opcode.FORGET_KNOWN_DEVICE, parseAddress(action.address)) {
                        dismissConfirmation()
                        refreshKnownDevices()
                    }
                is ConfirmAction.ResetLifetimeStats ->
                    runCommand(Opcode.RESET_LIFETIME_STATS, parseAddress(action.address)) {
                        dismissConfirmation()
                        refreshKnownDevices()
                    }
                ConfirmAction.Restart -> runCommand(Opcode.RESTART) { dismissConfirmation() }
                null -> Unit
            }
        }
    }
