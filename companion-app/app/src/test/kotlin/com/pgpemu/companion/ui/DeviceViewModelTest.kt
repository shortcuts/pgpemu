package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.CrashReport
import com.pgpemu.companion.ble.DeviceEvent
import com.pgpemu.companion.ble.EventType
import com.pgpemu.companion.ble.KnownDevice
import com.pgpemu.companion.ble.LifetimeStats
import com.pgpemu.companion.ble.Opcode
import com.pgpemu.companion.ble.ResponseFrame
import com.pgpemu.companion.ble.ScannedDevice
import com.pgpemu.companion.ble.StatusCode
import com.pgpemu.companion.ble.otaImageBytes
import com.pgpemu.companion.core.testing.FakeBleControlRepository
import com.pgpemu.companion.core.testing.FakeLastDeviceStore
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.io.IOException

private fun clientSummarySlotBytes(
    connId: Int,
    hasSettings: Boolean = false,
    autospin: Boolean = false,
    catchMode: Int = 0,
    hasStats: Boolean = false,
    handshakeDone: Boolean = connId != 0xFFFF,
    caught: Int = 0,
    fled: Int = 0,
    spin: Int = 0,
    warning: Int = 0,
): ByteArray {
    val flags =
        (if (hasSettings) 0x01 else 0) or (if (hasStats) 0x02 else 0) or
            (if (handshakeDone) 0x04 else 0) or (warning shl 3)
    return byteArrayOf(
        (connId and 0xFF).toByte(),
        ((connId shr 8) and 0xFF).toByte(),
        flags.toByte(),
        (if (autospin) 1 else 0).toByte(),
        catchMode.toByte(),
        (caught and 0xFF).toByte(),
        ((caught shr 8) and 0xFF).toByte(),
        (fled and 0xFF).toByte(),
        ((fled shr 8) and 0xFF).toByte(),
        (spin and 0xFF).toByte(),
        ((spin shr 8) and 0xFF).toByte(),
    )
}

private fun validSecretsCsv() =
    "key,type,encoding,value\npgpsecret,namespace,,\nname,data,string,\"PKLMGOPLUS\"\n" +
        "mac,data,hex2bin,e4cb0bc35963\ndkey,data,hex2bin,${"ab".repeat(16)}\nblob,data,hex2bin,${"cd".repeat(256)}\n"

private fun emptyClientSummarySlot(): ByteArray = clientSummarySlotBytes(connId = 0xFFFF)

@OptIn(ExperimentalCoroutinesApi::class)
class DeviceViewModelTest {
    private val dispatcher = StandardTestDispatcher()

    @Before
    fun setUp() {
        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun `uiState reflects discovered devices and hasScanned is false before any scan`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            val devices = listOf(ScannedDevice("AA:BB:CC:DD:EE:01", "Pokemon GO Plus", -50))

            repository.setDiscoveredDevices(devices)
            dispatcher.scheduler.runCurrent()

            assertEquals(devices, viewModel.uiState.value.discoveredDevices)
            assertFalse(viewModel.uiState.value.hasScanned)
        }

    @Test
    fun `startScan sets hasScanned and starts repository scan once`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.startScan()
            dispatcher.scheduler.runCurrent()

            assertTrue(viewModel.uiState.value.hasScanned)
            assertEquals(1, repository.startScanCalls)
        }

    @Test
    fun `connectTo stops scan then connects to the tapped address`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.connectTo("AA:BB:CC:DD:EE:02")
            dispatcher.scheduler.runCurrent()

            assertEquals(1, repository.stopScanCalls)
            assertEquals(listOf("AA:BB:CC:DD:EE:02"), repository.connectedAddresses)
        }

    @Test
    fun `uiState reflects repository connection state`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            repository.setConnectionState(ConnectionState.Ready)
            // Not advanceUntilIdle(): once Ready, the status-poll loop reschedules itself
            // forever via delay(), so advanceUntilIdle() would never find an idle point.
            dispatcher.scheduler.runCurrent()

            assertEquals(ConnectionState.Ready, viewModel.uiState.value.connectionState)

            // runTest drains the scheduler on exit; stop the poll loop so it can finish.
            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `polls status on an interval while connected and stops when disconnected`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_GLOBAL_SETTINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(0, 0, 1, 4))),
            )
            repository.stubResponse(
                Opcode.GET_LED_STATE,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf(0))),
            )
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot(),
                    ),
                ),
            )
            DeviceViewModel(repository, FakeLastDeviceStore())

            repository.setConnectionState(ConnectionState.Ready)
            dispatcher.scheduler.runCurrent() // initial refreshStatus() on connect

            val countAfterConnect = repository.sentCommands.count { it.first == Opcode.GET_GLOBAL_SETTINGS }
            dispatcher.scheduler.advanceTimeBy(STATUS_POLL_INTERVAL_MS + 1)
            dispatcher.scheduler.runCurrent()

            assertEquals(
                countAfterConnect + 1,
                repository.sentCommands.count { it.first == Opcode.GET_GLOBAL_SETTINGS },
            )

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
            val countAfterDisconnect = repository.sentCommands.count { it.first == Opcode.GET_GLOBAL_SETTINGS }

            dispatcher.scheduler.advanceTimeBy(STATUS_POLL_INTERVAL_MS * 3)
            dispatcher.scheduler.runCurrent()

            assertEquals(
                countAfterDisconnect,
                repository.sentCommands.count { it.first == Opcode.GET_GLOBAL_SETTINGS },
            )
        }

    @Test
    fun `polling stops in the background and refreshes once on return to foreground`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_GLOBAL_SETTINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(0, 0, 1, 4))),
            )
            repository.stubResponse(
                Opcode.GET_LED_STATE,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf(0))),
            )
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot(),
                    ),
                ),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            repository.setConnectionState(ConnectionState.Ready)
            dispatcher.scheduler.runCurrent()

            fun polls() = repository.sentCommands.count { it.first == Opcode.GET_GLOBAL_SETTINGS }

            viewModel.setForeground(false)
            dispatcher.scheduler.runCurrent()
            val whileBackground = polls()
            dispatcher.scheduler.advanceTimeBy(STATUS_POLL_INTERVAL_MS * 3)
            dispatcher.scheduler.runCurrent()
            assertEquals(whileBackground, polls())

            viewModel.setForeground(true)
            dispatcher.scheduler.runCurrent()
            assertEquals(whileBackground + 1, polls())

            dispatcher.scheduler.advanceTimeBy(STATUS_POLL_INTERVAL_MS + 1)
            dispatcher.scheduler.runCurrent()
            assertEquals(whileBackground + 2, polls())

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `user tap during a status refresh runs after it instead of being dropped`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_GLOBAL_SETTINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(0, 0, 1, 4))),
            )
            repository.stubResponse(
                Opcode.GET_LED_STATE,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf(0))),
            )
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot(),
                    ),
                ),
            )
            repository.stubResponse(
                Opcode.SET_LOG_LEVEL,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_LOG_LEVEL.toByte(), byteArrayOf(3))),
            )
            val gate = CompletableDeferred<Unit>()
            repository.gates[Opcode.GET_GLOBAL_SETTINGS] = gate
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            repository.setConnectionState(ConnectionState.Ready)
            dispatcher.scheduler.runCurrent() // refreshStatus() parks on the gate

            viewModel.cycleLogLevel()
            dispatcher.scheduler.runCurrent()
            assertTrue(repository.sentCommands.none { it.first == Opcode.SET_LOG_LEVEL })

            gate.complete(Unit)
            dispatcher.scheduler.runCurrent()

            assertEquals(1, repository.sentCommands.count { it.first == Opcode.SET_LOG_LEVEL })
            assertEquals(3, viewModel.uiState.value.status.logLevel)

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `background poll does not clear an existing error message`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(Opcode.SET_LOG_LEVEL, Result.failure(IOException("boom")))
            repository.stubResponse(
                Opcode.GET_GLOBAL_SETTINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(0, 0, 1, 4))),
            )
            repository.stubResponse(
                Opcode.GET_LED_STATE,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf(0))),
            )
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot() + emptyClientSummarySlot(),
                    ),
                ),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            repository.setConnectionState(ConnectionState.Ready)
            dispatcher.scheduler.runCurrent()

            viewModel.cycleLogLevel()
            dispatcher.scheduler.runCurrent()
            assertEquals(UiMessage.CommandFailed("boom"), viewModel.uiState.value.message)

            dispatcher.scheduler.advanceTimeBy(STATUS_POLL_INTERVAL_MS + 1)
            dispatcher.scheduler.runCurrent()

            assertEquals(UiMessage.CommandFailed("boom"), viewModel.uiState.value.message)

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `refreshStatus populates status and settings from GET responses`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_GLOBAL_SETTINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(2, 1, 3, 4))),
            )
            repository.stubResponse(
                Opcode.GET_LED_STATE,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf(1))),
            )
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        (
                            clientSummarySlotBytes(connId = 1, hasSettings = true, autospin = true, catchMode = 0) +
                                emptyClientSummarySlot() +
                                clientSummarySlotBytes(connId = 5, hasSettings = true, autospin = false, catchMode = 2) +
                                emptyClientSummarySlot()
                        ),
                    ),
                ),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            val state = viewModel.uiState.value
            assertEquals(2, state.status.logLevel)
            assertEquals(true, state.status.advertisingEnabled)
            assertNull(state.status.advertisingActive)
            assertEquals(3, state.status.activeConnections)
            assertEquals(4, state.settings.maxConnections)
            assertEquals(true, state.status.ledOn)
            assertEquals(true, state.profiles[0].autospin)
            assertEquals(0, state.profiles[0].catchMode)
            assertNull(state.profiles[1].autospin)
            assertEquals(false, state.profiles[2].autospin)
            assertEquals(2, state.profiles[2].catchMode)
        }

    private fun stubStatusReplies(repository: FakeBleControlRepository) {
        repository.stubResponse(
            Opcode.GET_GLOBAL_SETTINGS,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(0, 0, 1, 2))),
        )
        repository.stubResponse(
            Opcode.GET_LED_STATE,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf(0))),
        )
        repository.stubResponse(
            Opcode.GET_CLIENT_SUMMARY,
            Result.success(
                ResponseFrame(
                    StatusCode.OK,
                    Opcode.GET_CLIENT_SUMMARY.toByte(),
                    emptyClientSummarySlot() + emptyClientSummarySlot(),
                ),
            ),
        )
    }

    private fun deviceInfoBytes(
        protocol: Int,
        slots: Int,
        flags: Int = 0,
    ) = byteArrayOf(protocol.toByte(), slots.toByte(), 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, flags.toByte()) + "1.0.1".toByteArray()

    @Test
    fun `device info sizes profiles and the max connections cap, and is read once`() =
        runTest {
            val repository = FakeBleControlRepository()
            stubStatusReplies(repository)
            repository.stubResponse(
                Opcode.GET_DEVICE_INFO,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(4, 2))),
            )
            repository.stubResponse(
                Opcode.SET_MAX_CONNECTIONS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_MAX_CONNECTIONS.toByte(), byteArrayOf())),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            viewModel.setMaxConnections(9)
            dispatcher.scheduler.advanceUntilIdle()

            val state = viewModel.uiState.value
            assertEquals("1.0.1", state.deviceInfo?.firmwareVersion)
            assertEquals(2, state.profiles.size)
            assertFalse(state.firmwareOutdated)
            assertEquals(1, repository.sentCommands.count { it.first == Opcode.GET_DEVICE_INFO })
            assertEquals(
                2,
                repository.sentCommands
                    .last { it.first == Opcode.SET_MAX_CONNECTIONS }
                    .second[0]
                    .toInt(),
            )
        }

    @Test
    fun `unknown opcode and protocol 2 flag outdated firmware and keep default profiles, disconnect clears`() =
        runTest {
            val repository = FakeBleControlRepository()
            stubStatusReplies(repository)
            repository.stubResponse(
                Opcode.GET_DEVICE_INFO,
                Result.success(ResponseFrame(StatusCode.ERR_UNKNOWN_OPCODE, Opcode.GET_DEVICE_INFO.toByte(), byteArrayOf())),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertTrue(viewModel.uiState.value.firmwareOutdated)
            assertEquals(UiMessage.FirmwareOutdated, viewModel.uiState.value.message)
            assertEquals(DEFAULT_PROFILE_COUNT, viewModel.uiState.value.profiles.size)

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.advanceUntilIdle()
            assertFalse(viewModel.uiState.value.firmwareOutdated)

            repository.stubResponse(
                Opcode.GET_DEVICE_INFO,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(3, 2))),
            )
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertTrue(viewModel.uiState.value.firmwareOutdated)
            assertNull(viewModel.uiState.value.deviceInfo)
        }

    @Test
    fun `provisioning device skips status reads and import sends the secrets then restarts`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_DEVICE_INFO,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(4, 4, flags = 1))),
            )
            for (op in listOf(Opcode.PROVISION_SECRETS, Opcode.RESTART)) {
                repository.stubResponse(op, Result.success(ResponseFrame(StatusCode.OK, op.toByte(), byteArrayOf())))
            }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertTrue(
                viewModel.uiState.value.deviceInfo
                    ?.provisioning == true,
            )
            assertEquals(listOf(Opcode.GET_DEVICE_INFO), repository.sentCommands.map { it.first })

            viewModel.importSecrets("name,data,string,\"\"\n")
            dispatcher.scheduler.advanceUntilIdle()
            assertTrue(viewModel.uiState.value.message is UiMessage.InvalidSecretsFile)
            assertEquals(1, repository.sentCommands.size)

            viewModel.importSecrets(validSecretsCsv())
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(
                listOf(Opcode.GET_DEVICE_INFO, Opcode.PROVISION_SECRETS, Opcode.RESTART),
                repository.sentCommands.map { it.first },
            )
            assertEquals(294, repository.sentCommands[1].second.size)
            assertEquals(UiMessage.SecretsProvisioned, viewModel.uiState.value.message)
        }

    @Test
    fun `forget pairings sends one command only after confirmation`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.FORGET_PAIRINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.FORGET_PAIRINGS.toByte(), byteArrayOf())),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.requestForgetPairings()
            assertEquals(ConfirmAction.ForgetPairings, viewModel.uiState.value.pendingConfirmation)
            assertEquals(emptyList<Int>(), repository.sentCommands.map { it.first })

            viewModel.confirmPendingAction()
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(listOf(Opcode.FORGET_PAIRINGS), repository.sentCommands.map { it.first })
            assertNull(viewModel.uiState.value.pendingConfirmation)
        }

    @Test
    fun `failed provisioning does not restart the device`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.PROVISION_SECRETS,
                Result.success(ResponseFrame(StatusCode.ERR_INTERNAL, Opcode.PROVISION_SECRETS.toByte(), byteArrayOf())),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.importSecrets(validSecretsCsv())
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(listOf(Opcode.PROVISION_SECRETS), repository.sentCommands.map { it.first })
            assertEquals(UiMessage.DeviceStatus(StatusCode.ERR_INTERNAL.toInt()), viewModel.uiState.value.message)
        }

    @Test
    fun `refreshStatus hides a slot whose handshake is not complete`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        clientSummarySlotBytes(connId = 1, hasSettings = true, handshakeDone = false) +
                            clientSummarySlotBytes(connId = 2, hasSettings = true) +
                            emptyClientSummarySlot() +
                            emptyClientSummarySlot(),
                    ),
                ),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(
                false,
                viewModel.uiState.value.profiles[0]
                    .connected,
            )
            assertEquals(
                true,
                viewModel.uiState.value.profiles[1]
                    .connected,
            )
        }

    @Test
    fun `refreshStatus reads the radio state from byte 4 of GET_GLOBAL_SETTINGS`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_GLOBAL_SETTINGS,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_GLOBAL_SETTINGS.toByte(), byteArrayOf(2, 0, 1, 4, 0))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(false, viewModel.uiState.value.status.advertisingEnabled)
            assertEquals(false, viewModel.uiState.value.status.advertisingActive)
        }

    @Test
    fun `toggleAdvertising sends ADVERTISE_START and updates state when turning on`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.ADVERTISE_START,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.ADVERTISE_START.toByte(), ByteArray(0))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.toggleAdvertising()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(Opcode.ADVERTISE_START, repository.sentCommands.single().first)
            assertEquals(true, viewModel.uiState.value.status.advertisingEnabled)
        }

    @Test
    fun `toggleStatusLed sends the flipped value and updates state`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.SET_LED_ENABLED,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_LED_ENABLED.toByte(), byteArrayOf(1))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.toggleStatusLed()
            dispatcher.scheduler.advanceUntilIdle()

            val (opcode, payload) = repository.sentCommands.single()
            assertEquals(Opcode.SET_LED_ENABLED, opcode)
            assertEquals(listOf<Byte>(1), payload.toList())
            assertEquals(true, viewModel.uiState.value.status.ledEnabled)
        }

    @Test
    fun `toggleStatusLed failure leaves the state unchanged`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(Opcode.SET_LED_ENABLED, Result.failure(IOException("boom")))
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.toggleStatusLed()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(null, viewModel.uiState.value.status.ledEnabled)
            assertEquals(UiMessage.CommandFailed("boom"), viewModel.uiState.value.message)
        }

    @Test
    fun `toggleAutospin sends profile index as payload and updates that profile`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.SET_AUTOSPIN,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_AUTOSPIN.toByte(), byteArrayOf(1))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.toggleAutospin(2)
            dispatcher.scheduler.advanceUntilIdle()

            val (opcode, payload) = repository.sentCommands.single()
            assertEquals(Opcode.SET_AUTOSPIN, opcode)
            assertEquals(listOf<Byte>(2, 1), payload.toList())
            assertEquals(
                true,
                viewModel.uiState.value.profiles[2]
                    .autospin,
            )
        }

    private fun catchModeDevice(protocol: Int): FakeBleControlRepository {
        val repository = FakeBleControlRepository()
        stubStatusReplies(repository)
        repository.stubResponse(
            Opcode.GET_DEVICE_INFO,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(protocol, 4))),
        )
        repository.responder = { opcode, payload ->
            when (opcode) {
                Opcode.SET_CATCH_MODE, Opcode.SET_AUTOCATCH ->
                    Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), byteArrayOf(payload[1])))
                else -> null
            }
        }
        return repository
    }

    @Test
    fun `cycleCatchMode sends the target mode and walks off, new only, all`() =
        runTest {
            val repository = catchModeDevice(10)
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            val sent = mutableListOf<List<Byte>>()
            repeat(3) {
                viewModel.cycleCatchMode(1)
                dispatcher.scheduler.advanceUntilIdle()
            }
            repository.sentCommands.filter { it.first == Opcode.SET_CATCH_MODE }.forEach { sent += it.second.toList() }

            assertEquals(listOf(listOf<Byte>(1, 2), listOf<Byte>(1, 1), listOf<Byte>(1, 0)), sent)
            assertEquals(
                0,
                viewModel.uiState.value.profiles[1]
                    .catchMode,
            )
        }

    @Test
    fun `cycleCatchMode on protocol 9 flips with SET_AUTOCATCH`() =
        runTest {
            val repository = catchModeDevice(9)
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            viewModel.cycleCatchMode(1)
            dispatcher.scheduler.advanceUntilIdle()

            val (opcode, payload) = repository.sentCommands.single { it.first == Opcode.SET_AUTOCATCH }
            assertEquals(Opcode.SET_AUTOCATCH, opcode)
            assertEquals(listOf<Byte>(1, 1), payload.toList())
            assertEquals(
                1,
                viewModel.uiState.value.profiles[1]
                    .catchMode,
            )
            assertFalse(repository.sentCommands.any { it.first == Opcode.SET_CATCH_MODE })
        }

    @Test
    fun `toggleAutospin repeated without a reply sends the same target`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(Opcode.SET_AUTOSPIN, Result.failure(IOException("lost")))
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.toggleAutospin(0)
            dispatcher.scheduler.advanceUntilIdle()
            viewModel.toggleAutospin(0)
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(listOf(listOf<Byte>(0, 1), listOf<Byte>(0, 1)), repository.sentCommands.map { it.second.toList() })
        }

    @Test
    fun `cycleLogLevel from level 3 sends level 1`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.SET_LOG_LEVEL,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_LOG_LEVEL.toByte(), byteArrayOf(3))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.cycleLogLevel()
            dispatcher.scheduler.advanceUntilIdle()
            viewModel.cycleLogLevel()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(listOf<Byte>(1), repository.sentCommands[0].second.toList())
            assertEquals(listOf<Byte>(1), repository.sentCommands[1].second.toList())
        }

    @Test
    fun `repository failure surfaces error message in uiState`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(Opcode.SET_LOG_LEVEL, Result.failure(IOException("disconnected")))
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.cycleLogLevel()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(UiMessage.CommandFailed("disconnected"), viewModel.uiState.value.message)
        }

    @Test
    fun `empty OK payload shows unexpected reply instead of crashing`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.SET_LOG_LEVEL,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_LOG_LEVEL.toByte(), ByteArray(0))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.cycleLogLevel()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(UiMessage.UnexpectedReply, viewModel.uiState.value.message)
            assertNull(viewModel.uiState.value.status.logLevel)
        }

    @Test
    fun `non-OK device status surfaces error message and leaves state unchanged`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.SET_LOG_LEVEL,
                Result.success(ResponseFrame(StatusCode.ERR_BUSY, Opcode.SET_LOG_LEVEL.toByte(), ByteArray(0))),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.cycleLogLevel()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(UiMessage.DeviceStatus(StatusCode.ERR_BUSY.toInt()), viewModel.uiState.value.message)
            assertNull(viewModel.uiState.value.status.logLevel)
        }

    @Test
    fun `refreshRuntimeStats attributes caught fled spin to the right profile slot`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.GET_RUNTIME_STATS,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_RUNTIME_STATS.toByte(),
                        "---STATS---\nConnection 3:\n- Caught: 5\n- Fled: 2\n- Spin: 7\n".toByteArray(),
                    ),
                ),
            )
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        (
                            clientSummarySlotBytes(connId = 3, hasStats = true, caught = 5, fled = 2, spin = 7) +
                                emptyClientSummarySlot() +
                                emptyClientSummarySlot() +
                                emptyClientSummarySlot()
                        ),
                    ),
                ),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshRuntimeStats()
            dispatcher.scheduler.advanceUntilIdle()

            val profiles = viewModel.uiState.value.profiles
            assertEquals(5, profiles[0].caught)
            assertEquals(2, profiles[0].fled)
            assertEquals(7, profiles[0].spin)
            assertNull(profiles[1].caught)
            assertEquals(
                listOf(Opcode.GET_RUNTIME_STATS, Opcode.GET_CLIENT_SUMMARY),
                repository.sentCommands.map { it.first },
            )
        }

    private fun otaDevice(protocol: Int): FakeBleControlRepository {
        val repository = FakeBleControlRepository()
        stubStatusReplies(repository)
        repository.stubResponse(
            Opcode.GET_DEVICE_INFO,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(protocol, 4))),
        )
        repository.responder = { opcode, payload ->
            val reply =
                when (opcode) {
                    Opcode.OTA_BEGIN -> byteArrayOf(0xE0.toByte(), 0x01)
                    Opcode.OTA_DATA -> {
                        val next = (0 until 4).sumOf { (payload[it].toInt() and 0xFF) shl (8 * it) } + payload.size - 4
                        ByteArray(4) { (next shr (8 * it)).toByte() }
                    }
                    else -> byteArrayOf()
                }
            if (opcode in Opcode.OTA_BEGIN..Opcode.OTA_ABORT || opcode == Opcode.RESTART) {
                Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), reply))
            } else {
                null
            }
        }
        return repository
    }

    @Test
    fun `firmware update sends the image after confirmation, then restarts`() =
        runTest {
            val repository = otaDevice(5)
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertTrue(viewModel.uiState.value.otaSupported)

            viewModel.requestFirmwareUpdate(otaImageBytes(size = 1000))
            assertEquals(ConfirmAction.UpdateFirmware, viewModel.uiState.value.pendingConfirmation)
            assertFalse(repository.sentCommands.any { it.first == Opcode.OTA_BEGIN })

            viewModel.confirmPendingAction()
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(
                listOf(Opcode.OTA_BEGIN, Opcode.OTA_DATA, Opcode.OTA_DATA, Opcode.OTA_DATA, Opcode.OTA_END, Opcode.RESTART),
                repository.sentCommands.map { it.first }.dropWhile { it !in Opcode.OTA_BEGIN..Opcode.OTA_ABORT },
            )
            assertEquals(UiMessage.FirmwareInstalled, viewModel.uiState.value.message)
            assertNull(viewModel.uiState.value.otaProgress)
            assertNull(viewModel.uiState.value.pendingConfirmation)
        }

    @Test
    fun `firmware update reports a device that is not armed`() =
        runTest {
            val repository = otaDevice(5)
            repository.responder = { opcode, _ ->
                if (opcode == Opcode.OTA_BEGIN) {
                    Result.success(ResponseFrame(StatusCode.ERR_OTA_NOT_ARMED, opcode.toByte(), byteArrayOf()))
                } else {
                    null
                }
            }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            viewModel.requestFirmwareUpdate(otaImageBytes(size = 1000))
            viewModel.confirmPendingAction()
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(UiMessage.DeviceStatus(StatusCode.ERR_OTA_NOT_ARMED.toInt()), viewModel.uiState.value.message)
            assertFalse(repository.sentCommands.any { it.first == Opcode.RESTART })
            assertFalse(viewModel.uiState.value.isBusy)
        }

    @Test
    fun `firmware update is refused on protocol 4 and for a bad image`() =
        runTest {
            val old = otaDevice(4)
            val oldVm = DeviceViewModel(old, FakeLastDeviceStore())
            oldVm.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertFalse(oldVm.uiState.value.otaSupported)
            oldVm.requestFirmwareUpdate(otaImageBytes())
            assertNull(oldVm.uiState.value.pendingConfirmation)

            val vm = DeviceViewModel(otaDevice(5), FakeLastDeviceStore())
            vm.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            vm.requestFirmwareUpdate(otaImageBytes(project = "other"))
            assertTrue(vm.uiState.value.message is UiMessage.InvalidFirmwareFile)
            assertNull(vm.uiState.value.pendingConfirmation)
        }

    private fun knownDevicesList(
        vararg named: Pair<Int, String>,
        flags: Int = 0,
    ): ByteArray =
        byteArrayOf(named.size.toByte()) +
            named
                .map { (last, name) ->
                    byteArrayOf(0xAA.toByte(), 0xBB.toByte(), 0xCC.toByte(), 0xDD.toByte(), 0xEE.toByte(), last.toByte()) +
                        byteArrayOf(last.toByte(), 0, 0, 0, flags.toByte()) + name.toByteArray().copyOf(16)
                }.reduce(ByteArray::plus)

    private fun knownDevicesDevice(
        protocol: Int,
        list: () -> ByteArray,
    ): FakeBleControlRepository {
        val repository = FakeBleControlRepository()
        stubStatusReplies(repository)
        repository.stubResponse(
            Opcode.GET_DEVICE_INFO,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(protocol, 4))),
        )
        repository.responder = { opcode, _ ->
            when (opcode) {
                Opcode.LIST_KNOWN_DEVICES -> Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), list()))
                Opcode.RENAME_KNOWN_DEVICE, Opcode.FORGET_KNOWN_DEVICE ->
                    Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), byteArrayOf()))
                else -> null
            }
        }
        return repository
    }

    @Test
    fun `known devices are fetched on refresh when the firmware supports them`() =
        runTest {
            val repository = knownDevicesDevice(6) { knownDevicesList(2 to "", 1 to "Pixel") }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertTrue(viewModel.uiState.value.knownDevicesSupported)
            assertEquals(
                listOf(KnownDevice("AA:BB:CC:DD:EE:02", 2L, false, ""), KnownDevice("AA:BB:CC:DD:EE:01", 1L, false, "Pixel")),
                viewModel.uiState.value.knownDevices,
            )

            viewModel.refreshStatus(clearError = false)
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(1, repository.sentCommands.count { it.first == Opcode.LIST_KNOWN_DEVICES })
        }

    @Test
    fun `a phone on profile 1 is matched to its card from protocol 11`() =
        runTest {
            val repository = knownDevicesDevice(11) { knownDevicesList(1 to "Pixel", flags = 0x03) }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertTrue(viewModel.uiState.value.profilePhoneSupported)
            assertEquals(
                1,
                viewModel.uiState.value.knownDevices
                    .single()
                    .profile,
            )
        }

    @Test
    fun `protocol 10 does not offer profile phones`() =
        runTest {
            val repository = knownDevicesDevice(10) { knownDevicesList(1 to "Pixel", flags = 0x01) }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertFalse(viewModel.uiState.value.profilePhoneSupported)
        }

    @Test
    fun `a profile that connects mid-session refetches the known devices`() =
        runTest {
            val repository = knownDevicesDevice(11) { knownDevicesList(1 to "Pixel", flags = 0x03) }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(1, repository.sentCommands.count { it.first == Opcode.LIST_KNOWN_DEVICES })

            viewModel.refreshStatus(clearError = false)
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(1, repository.sentCommands.count { it.first == Opcode.LIST_KNOWN_DEVICES })

            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        emptyClientSummarySlot() + clientSummarySlotBytes(connId = 1),
                    ),
                ),
            )
            viewModel.refreshStatus(clearError = false)
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(2, repository.sentCommands.count { it.first == Opcode.LIST_KNOWN_DEVICES })
        }

    @Test
    fun `known devices are neither offered nor fetched on protocol 5`() =
        runTest {
            val repository = knownDevicesDevice(5) { knownDevicesList(1 to "") }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertFalse(viewModel.uiState.value.knownDevicesSupported)
            assertFalse(repository.sentCommands.any { it.first == Opcode.LIST_KNOWN_DEVICES })
        }

    @Test
    fun `rename sends the address and name, then reloads the list`() =
        runTest {
            var list = knownDevicesList(1 to "")
            val repository = knownDevicesDevice(6) { list }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            list = knownDevicesList(1 to "Pixel")
            viewModel.renameKnownDevice("AA:BB:CC:DD:EE:01", "Pixel")
            dispatcher.scheduler.advanceUntilIdle()

            val sent = repository.sentCommands.last { it.first == Opcode.RENAME_KNOWN_DEVICE }.second
            assertEquals(
                "AABBCCDDEE01" + "Pixel".toByteArray().joinToString("") { "%02X".format(it) },
                sent.joinToString("") { "%02X".format(it) },
            )
            assertEquals(
                "Pixel",
                viewModel.uiState.value.knownDevices
                    .single()
                    .name,
            )
        }

    @Test
    fun `forgetting a known device sends one command only after confirmation, then reloads the list`() =
        runTest {
            var list = knownDevicesList(1 to "")
            val repository = knownDevicesDevice(6) { list }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            viewModel.requestForgetKnownDevice("AA:BB:CC:DD:EE:01")
            assertEquals(ConfirmAction.ForgetKnownDevice("AA:BB:CC:DD:EE:01"), viewModel.uiState.value.pendingConfirmation)
            assertFalse(repository.sentCommands.any { it.first == Opcode.FORGET_KNOWN_DEVICE })

            list = byteArrayOf(0)
            viewModel.confirmPendingAction()
            dispatcher.scheduler.advanceUntilIdle()

            val forgets = repository.sentCommands.filter { it.first == Opcode.FORGET_KNOWN_DEVICE }
            assertEquals(1, forgets.size)
            assertEquals("AABBCCDDEE01", forgets.single().second.joinToString("") { "%02X".format(it) })
            assertNull(viewModel.uiState.value.pendingConfirmation)
            assertEquals(emptyList<KnownDevice>(), viewModel.uiState.value.knownDevices)
        }

    private fun crashDevice(
        protocol: Int,
        clearStatus: Byte = StatusCode.OK,
    ): FakeBleControlRepository {
        val repository = FakeBleControlRepository()
        stubStatusReplies(repository)
        repository.stubResponse(
            Opcode.GET_DEVICE_INFO,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(protocol, 4))),
        )
        repository.responder = { opcode, _ ->
            when (opcode) {
                Opcode.GET_CRASH_REPORT ->
                    Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), ByteArray(47).also { it[0] = 1 }))
                Opcode.CLEAR_CRASH_REPORT -> Result.success(ResponseFrame(clearStatus, opcode.toByte(), byteArrayOf()))
                else -> null
            }
        }
        return repository
    }

    @Test
    fun `crash report is fetched once per connect and dismissing clears it`() =
        runTest {
            val repository = crashDevice(9)
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            viewModel.refreshStatus(clearError = false)
            dispatcher.scheduler.advanceUntilIdle()

            assertTrue(viewModel.uiState.value.crashSupported)
            assertEquals(CrashReport("", 0L, 0L, 0L, 0L, 0L, "", emptyList()), viewModel.uiState.value.crashReport)
            assertEquals(1, repository.sentCommands.count { it.first == Opcode.GET_CRASH_REPORT })

            viewModel.dismissCrashReport()
            dispatcher.scheduler.advanceUntilIdle()
            assertEquals(1, repository.sentCommands.count { it.first == Opcode.CLEAR_CRASH_REPORT })
            assertNull(viewModel.uiState.value.crashReport)
        }

    @Test
    fun `a busy device keeps the crash report and reports the status`() =
        runTest {
            val viewModel = DeviceViewModel(crashDevice(9, StatusCode.ERR_BUSY), FakeLastDeviceStore())
            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            viewModel.dismissCrashReport()
            dispatcher.scheduler.advanceUntilIdle()

            assertTrue(viewModel.uiState.value.crashReport != null)
            assertEquals(UiMessage.DeviceStatus(StatusCode.ERR_BUSY.toInt()), viewModel.uiState.value.message)
        }

    @Test
    fun `crash report is neither offered nor fetched on protocol 8`() =
        runTest {
            val repository = crashDevice(8)
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertFalse(viewModel.uiState.value.crashSupported)
            assertFalse(repository.sentCommands.any { it.first == Opcode.GET_CRASH_REPORT })
        }

    private fun lifetimeFrame(caught: Int) =
        ByteArray(37).also {
            it[0] = 1
            it[1] = caught.toByte()
            it[25] = 2
        }

    @Test
    fun `lifetime stats are fetched per phone and zeroed after a confirmed reset`() =
        runTest {
            var caught = 5
            val repository = knownDevicesDevice(8) { knownDevicesList(2 to "", 1 to "Pixel") }
            val listResponder = repository.responder!!
            repository.responder = { opcode, payload ->
                when (opcode) {
                    Opcode.GET_LIFETIME_STATS -> Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), lifetimeFrame(caught)))
                    Opcode.RESET_LIFETIME_STATS -> {
                        caught = 0
                        Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), byteArrayOf()))
                    }
                    else -> listResponder(opcode, payload)
                }
            }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertTrue(viewModel.uiState.value.lifetimeSupported)
            val gets = repository.sentCommands.filter { it.first == Opcode.GET_LIFETIME_STATS }
            assertEquals(listOf("AABBCCDDEE02", "AABBCCDDEE01"), gets.map { it.second.joinToString("") { b -> "%02X".format(b) } })
            assertEquals(setOf("AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"), viewModel.uiState.value.knownLifetime.keys)
            assertEquals(
                5L,
                viewModel.uiState.value.knownLifetime
                    .getValue("AA:BB:CC:DD:EE:01")
                    .lifetime[0],
            )

            viewModel.requestResetLifetimeStats("AA:BB:CC:DD:EE:01")
            assertEquals(ConfirmAction.ResetLifetimeStats("AA:BB:CC:DD:EE:01"), viewModel.uiState.value.pendingConfirmation)
            assertFalse(repository.sentCommands.any { it.first == Opcode.RESET_LIFETIME_STATS })
            viewModel.confirmPendingAction()
            dispatcher.scheduler.advanceUntilIdle()

            val resets = repository.sentCommands.filter { it.first == Opcode.RESET_LIFETIME_STATS }
            assertEquals("AABBCCDDEE01", resets.single().second.joinToString("") { "%02X".format(it) })
            assertNull(viewModel.uiState.value.pendingConfirmation)
            assertEquals(
                0L,
                viewModel.uiState.value.knownLifetime
                    .getValue("AA:BB:CC:DD:EE:01")
                    .lifetime[0],
            )
        }

    @Test
    fun `lifetime stats are not requested on protocol 7`() =
        runTest {
            val repository = knownDevicesDevice(7) { knownDevicesList(1 to "") }
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())

            viewModel.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()

            assertFalse(viewModel.uiState.value.lifetimeSupported)
            assertFalse(repository.sentCommands.any { it.first == Opcode.GET_LIFETIME_STATS })
            assertEquals(emptyMap<String, LifetimeStats>(), viewModel.uiState.value.knownLifetime)
        }

    private fun TestScope.dropAfterReady(
        repository: FakeBleControlRepository,
        store: FakeLastDeviceStore = FakeLastDeviceStore(),
    ): DeviceViewModel {
        val viewModel = DeviceViewModel(repository, store)
        viewModel.connectTo("AA:BB:CC:DD:EE:02")
        dispatcher.scheduler.runCurrent()
        repository.setConnectionState(ConnectionState.Ready)
        dispatcher.scheduler.runCurrent()
        repository.setConnectionState(ConnectionState.Disconnected(8))
        dispatcher.scheduler.runCurrent()
        return viewModel
    }

    @Test
    fun `Ready after connectTo stores that address`() =
        runTest {
            val repository = FakeBleControlRepository()
            val store = FakeLastDeviceStore()
            dropAfterReady(repository, store)

            assertEquals("AA:BB:CC:DD:EE:02", store.address)
        }

    @Test
    fun `unexpected drop reconnects with growing delays and then gives up`() =
        runTest {
            val repository = FakeBleControlRepository()
            dropAfterReady(repository)
            assertEquals(1, repository.connectedAddresses.size)

            var waited = 0L
            RECONNECT_BACKOFF_MS.forEachIndexed { i, wait ->
                dispatcher.scheduler.advanceTimeBy(wait - 1)
                dispatcher.scheduler.runCurrent()
                assertEquals(1 + i, repository.connectedAddresses.size)
                dispatcher.scheduler.advanceTimeBy(1)
                dispatcher.scheduler.runCurrent()
                assertEquals(2 + i, repository.connectedAddresses.size)
                waited += wait
            }
            dispatcher.scheduler.advanceTimeBy(waited)
            dispatcher.scheduler.runCurrent()
            assertEquals(1 + RECONNECT_BACKOFF_MS.size, repository.connectedAddresses.size)
            assertTrue(repository.connectedAddresses.drop(1).all { it == "AA:BB:CC:DD:EE:02" })
        }

    @Test
    fun `user disconnect does not reconnect`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.connectTo("AA:BB:CC:DD:EE:02")
            repository.setConnectionState(ConnectionState.Ready)
            dispatcher.scheduler.runCurrent()

            viewModel.disconnect()
            repository.setConnectionState(ConnectionState.Disconnected(0))
            dispatcher.scheduler.runCurrent()
            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.advanceTimeBy(60_000)
            dispatcher.scheduler.runCurrent()

            assertEquals(1, repository.connectedAddresses.size)
        }

    @Test
    fun `connect error without a prior Ready does not reconnect`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            viewModel.connectTo("AA:BB:CC:DD:EE:02")
            repository.setConnectionState(ConnectionState.Error("connect failed"))
            dispatcher.scheduler.runCurrent()
            repository.setConnectionState(ConnectionState.Disconnected(133))
            dispatcher.scheduler.advanceTimeBy(60_000)
            dispatcher.scheduler.runCurrent()

            assertEquals(1, repository.connectedAddresses.size)
        }

    @Test
    fun `startScan and connectTo cancel a pending reconnect`() =
        runTest {
            val scanRepo = FakeBleControlRepository()
            val scanVm = dropAfterReady(scanRepo)
            scanVm.startScan()
            dispatcher.scheduler.advanceTimeBy(60_000)
            dispatcher.scheduler.runCurrent()
            assertEquals(1, scanRepo.connectedAddresses.size)

            val connectRepo = FakeBleControlRepository()
            val connectVm = dropAfterReady(connectRepo)
            connectVm.connectTo("AA:BB:CC:DD:EE:03")
            dispatcher.scheduler.runCurrent()
            dispatcher.scheduler.advanceTimeBy(60_000)
            dispatcher.scheduler.runCurrent()
            assertEquals(listOf("AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"), connectRepo.connectedAddresses)
        }

    @Test
    fun `resumeLastDevice connects once to the stored address when idle`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore("AA:BB:CC:DD:EE:09"))

            viewModel.resumeLastDevice()
            viewModel.resumeLastDevice()
            dispatcher.scheduler.runCurrent()

            assertEquals(listOf("AA:BB:CC:DD:EE:09"), repository.connectedAddresses)
        }

    @Test
    fun `resumeLastDevice does nothing without an address or when not idle`() =
        runTest {
            val empty = FakeBleControlRepository()
            DeviceViewModel(empty, FakeLastDeviceStore()).resumeLastDevice()
            val busy = FakeBleControlRepository()
            val busyVm = DeviceViewModel(busy, FakeLastDeviceStore("AA:BB:CC:DD:EE:09"))
            busy.setConnectionState(ConnectionState.Scanning)
            dispatcher.scheduler.runCurrent()
            busyVm.resumeLastDevice()
            dispatcher.scheduler.runCurrent()

            assertTrue(empty.connectedAddresses.isEmpty())
            assertTrue(busy.connectedAddresses.isEmpty())
        }

    private fun connectedWithProtocol(
        repository: FakeBleControlRepository,
        protocol: Int,
    ): DeviceViewModel {
        stubStatusReplies(repository)
        repository.stubResponse(
            Opcode.GET_DEVICE_INFO,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(protocol, 4))),
        )
        val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
        repository.setConnectionState(ConnectionState.Ready)
        dispatcher.scheduler.runCurrent()
        return viewModel
    }

    private fun refreshes(repository: FakeBleControlRepository) = repository.sentCommands.count { it.first == Opcode.GET_GLOBAL_SETTINGS }

    @Test
    fun `events fill the activity feed newest first, capped, and unknown types stay`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = connectedWithProtocol(repository, EVENTS_MIN_PROTOCOL_VERSION)
            assertTrue(viewModel.uiState.value.eventsSupported)

            repository.emitEvent(DeviceEvent(0, EventType.CAUGHT, 1))
            repository.emitEvent(DeviceEvent(1, 99, 2))
            dispatcher.scheduler.runCurrent()
            assertEquals(
                listOf(99 to 2, EventType.CAUGHT to 1),
                viewModel.uiState.value.activity
                    .map { it.type to it.slot },
            )

            for (seq in 2 until ACTIVITY_LIMIT + 10) {
                repository.emitEvent(DeviceEvent(seq and 0xFF, EventType.POKEMON_IN_RANGE, 0))
                dispatcher.scheduler.runCurrent()
            }
            assertEquals(ACTIVITY_LIMIT, viewModel.uiState.value.activity.size)

            viewModel.clearActivity()
            assertTrue(
                viewModel.uiState.value.activity
                    .isEmpty(),
            )
            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `a warning from the summary shows on its profile until a normal pattern event clears it`() =
        runTest {
            val repository = FakeBleControlRepository()
            stubStatusReplies(repository)
            repository.stubResponse(
                Opcode.GET_CLIENT_SUMMARY,
                Result.success(
                    ResponseFrame(
                        StatusCode.OK,
                        Opcode.GET_CLIENT_SUMMARY.toByte(),
                        clientSummarySlotBytes(connId = 7, warning = 2) + emptyClientSummarySlot(),
                    ),
                ),
            )
            repository.stubResponse(
                Opcode.GET_DEVICE_INFO,
                Result.success(
                    ResponseFrame(StatusCode.OK, Opcode.GET_DEVICE_INFO.toByte(), deviceInfoBytes(EVENTS_MIN_PROTOCOL_VERSION, 4)),
                ),
            )
            val viewModel = DeviceViewModel(repository, FakeLastDeviceStore())
            repository.setConnectionState(ConnectionState.Ready)
            dispatcher.scheduler.runCurrent()
            assertEquals(
                EventType.BOX_FULL,
                viewModel.uiState.value.profiles[0]
                    .warning,
            )
            assertEquals(
                NO_WARNING,
                viewModel.uiState.value.profiles[1]
                    .warning,
            )

            repository.emitEvent(DeviceEvent(0, EventType.POKEMON_IN_RANGE, 1))
            dispatcher.scheduler.runCurrent()
            assertEquals(
                EventType.BOX_FULL,
                viewModel.uiState.value.profiles[0]
                    .warning,
            )

            repository.emitEvent(DeviceEvent(1, EventType.POKEMON_IN_RANGE, 0))
            dispatcher.scheduler.runCurrent()
            assertEquals(
                NO_WARNING,
                viewModel.uiState.value.profiles[0]
                    .warning,
            )
            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `state-changing events refresh the status and in-range events do not`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = connectedWithProtocol(repository, EVENTS_MIN_PROTOCOL_VERSION)
            val before = refreshes(repository)

            repository.emitEvent(DeviceEvent(0, EventType.POKEMON_IN_RANGE, 0))
            dispatcher.scheduler.runCurrent()
            assertEquals(before, refreshes(repository))

            repository.emitEvent(DeviceEvent(1, EventType.CAUGHT, 0))
            dispatcher.scheduler.runCurrent()
            assertEquals(before + 1, refreshes(repository))
            assertEquals(2, viewModel.uiState.value.activity.size)

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `a sequence gap refreshes even for an in-range event`() =
        runTest {
            val repository = FakeBleControlRepository()
            connectedWithProtocol(repository, EVENTS_MIN_PROTOCOL_VERSION)
            repository.emitEvent(DeviceEvent(0, EventType.POKEMON_IN_RANGE, 0))
            dispatcher.scheduler.runCurrent()
            val before = refreshes(repository)

            repository.emitEvent(DeviceEvent(1, EventType.POKEMON_IN_RANGE, 0))
            dispatcher.scheduler.runCurrent()
            assertEquals(before, refreshes(repository))

            repository.emitEvent(DeviceEvent(3, EventType.POKEMON_IN_RANGE, 0))
            dispatcher.scheduler.runCurrent()
            assertEquals(before + 1, refreshes(repository))

            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }

    @Test
    fun `event firmware polls every 30 seconds, older firmware every 5`() =
        runTest {
            val repository = FakeBleControlRepository()
            connectedWithProtocol(repository, EVENTS_MIN_PROTOCOL_VERSION)
            val before = refreshes(repository)
            dispatcher.scheduler.advanceTimeBy(EVENT_POLL_INTERVAL_MS - 1000)
            dispatcher.scheduler.runCurrent()
            assertEquals(before, refreshes(repository))
            dispatcher.scheduler.advanceTimeBy(1000 + 1)
            dispatcher.scheduler.runCurrent()
            assertEquals(before + 1, refreshes(repository))
            repository.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()

            val older = FakeBleControlRepository()
            connectedWithProtocol(older, EVENTS_MIN_PROTOCOL_VERSION - 1)
            val olderBefore = refreshes(older)
            dispatcher.scheduler.advanceTimeBy(STATUS_POLL_INTERVAL_MS + 1)
            dispatcher.scheduler.runCurrent()
            assertEquals(olderBefore + 1, refreshes(older))
            older.setConnectionState(ConnectionState.Disconnected())
            dispatcher.scheduler.runCurrent()
        }
}
