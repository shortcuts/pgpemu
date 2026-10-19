package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.Opcode
import com.pgpemu.companion.ble.ResponseFrame
import com.pgpemu.companion.ble.ScannedDevice
import com.pgpemu.companion.ble.StatusCode
import com.pgpemu.companion.ble.otaImageBytes
import com.pgpemu.companion.core.testing.FakeBleControlRepository
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
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
    autocatch: Boolean = false,
    hasStats: Boolean = false,
    handshakeDone: Boolean = connId != 0xFFFF,
    caught: Int = 0,
    fled: Int = 0,
    spin: Int = 0,
): ByteArray {
    val flags =
        (if (hasSettings) 0x01 else 0) or (if (hasStats) 0x02 else 0) or
            (if (handshakeDone) 0x04 else 0)
    return byteArrayOf(
        (connId and 0xFF).toByte(),
        ((connId shr 8) and 0xFF).toByte(),
        flags.toByte(),
        (if (autospin) 1 else 0).toByte(),
        (if (autocatch) 1 else 0).toByte(),
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
            val viewModel = DeviceViewModel(repository)
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
            val viewModel = DeviceViewModel(repository)

            viewModel.startScan()
            dispatcher.scheduler.runCurrent()

            assertTrue(viewModel.uiState.value.hasScanned)
            assertEquals(1, repository.startScanCalls)
        }

    @Test
    fun `connectTo stops scan then connects to the tapped address`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository)

            viewModel.connectTo("AA:BB:CC:DD:EE:02")
            dispatcher.scheduler.runCurrent()

            assertEquals(1, repository.stopScanCalls)
            assertEquals(listOf("AA:BB:CC:DD:EE:02"), repository.connectedAddresses)
        }

    @Test
    fun `uiState reflects repository connection state`() =
        runTest {
            val repository = FakeBleControlRepository()
            val viewModel = DeviceViewModel(repository)

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
            DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)
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
            val viewModel = DeviceViewModel(repository)
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
            val viewModel = DeviceViewModel(repository)
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
                            clientSummarySlotBytes(connId = 1, hasSettings = true, autospin = true, autocatch = false) +
                                emptyClientSummarySlot() +
                                clientSummarySlotBytes(connId = 5, hasSettings = true, autospin = false, autocatch = true) +
                                emptyClientSummarySlot()
                        ),
                    ),
                ),
            )
            val viewModel = DeviceViewModel(repository)

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
            assertEquals(false, state.profiles[0].autocatch)
            assertNull(state.profiles[1].autospin)
            assertEquals(false, state.profiles[2].autospin)
            assertEquals(true, state.profiles[2].autocatch)
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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

            viewModel.toggleAdvertising()
            dispatcher.scheduler.advanceUntilIdle()

            assertEquals(Opcode.ADVERTISE_START, repository.sentCommands.single().first)
            assertEquals(true, viewModel.uiState.value.status.advertisingEnabled)
        }

    @Test
    fun `toggleAutospin sends profile index as payload and updates that profile`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(
                Opcode.SET_AUTOSPIN,
                Result.success(ResponseFrame(StatusCode.OK, Opcode.SET_AUTOSPIN.toByte(), byteArrayOf(1))),
            )
            val viewModel = DeviceViewModel(repository)

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

    @Test
    fun `toggleAutospin repeated without a reply sends the same target`() =
        runTest {
            val repository = FakeBleControlRepository()
            repository.stubResponse(Opcode.SET_AUTOSPIN, Result.failure(IOException("lost")))
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)

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
            val viewModel = DeviceViewModel(repository)
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
            val viewModel = DeviceViewModel(repository)
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
            val oldVm = DeviceViewModel(old)
            oldVm.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            assertFalse(oldVm.uiState.value.otaSupported)
            oldVm.requestFirmwareUpdate(otaImageBytes())
            assertNull(oldVm.uiState.value.pendingConfirmation)

            val vm = DeviceViewModel(otaDevice(5))
            vm.refreshStatus()
            dispatcher.scheduler.advanceUntilIdle()
            vm.requestFirmwareUpdate(otaImageBytes(project = "other"))
            assertTrue(vm.uiState.value.message is UiMessage.InvalidFirmwareFile)
            assertNull(vm.uiState.value.pendingConfirmation)
        }
}
