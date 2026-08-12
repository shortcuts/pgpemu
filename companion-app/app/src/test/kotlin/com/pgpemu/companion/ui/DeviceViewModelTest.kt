package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.Opcode
import com.pgpemu.companion.ble.ResponseFrame
import com.pgpemu.companion.ble.StatusCode
import com.pgpemu.companion.core.testing.FakeBleControlRepository
import java.io.IOException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

private fun clientSummarySlotBytes(
    connId: Int,
    hasSettings: Boolean = false,
    autospin: Boolean = false,
    autocatch: Boolean = false,
    hasStats: Boolean = false,
    caught: Int = 0,
    fled: Int = 0,
    spin: Int = 0,
): ByteArray {
    val flags = (if (hasSettings) 0x01 else 0) or (if (hasStats) 0x02 else 0)
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
    fun `uiState reflects repository connection state`() = runTest {
        val repository = FakeBleControlRepository()
        val viewModel = DeviceViewModel(repository)

        repository.setConnectionState(ConnectionState.Ready)
        dispatcher.scheduler.advanceUntilIdle()

        assertEquals(ConnectionState.Ready, viewModel.uiState.value.connectionState)
    }

    @Test
    fun `refreshStatus populates status and settings from GET responses`() = runTest {
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
        assertEquals(3, state.status.activeConnections)
        assertEquals(4, state.settings.maxConnections)
        assertEquals(true, state.status.ledOn)
        assertEquals(true, state.profiles[0].autospin)
        assertEquals(false, state.profiles[0].autocatch)
        assertNull(state.profiles[1].autospin)
        assertEquals(false, state.profiles[2].autospin)
        assertEquals(true, state.profiles[2].autocatch)
    }

    @Test
    fun `toggleAdvertising sends ADVERTISE_START and updates state when turning on`() = runTest {
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
    fun `toggleAutospin sends profile index as payload and updates that profile`() = runTest {
        val repository = FakeBleControlRepository()
        repository.stubResponse(
            Opcode.TOGGLE_AUTOSPIN,
            Result.success(ResponseFrame(StatusCode.OK, Opcode.TOGGLE_AUTOSPIN.toByte(), byteArrayOf(1))),
        )
        val viewModel = DeviceViewModel(repository)

        viewModel.toggleAutospin(2)
        dispatcher.scheduler.advanceUntilIdle()

        val (opcode, payload) = repository.sentCommands.single()
        assertEquals(Opcode.TOGGLE_AUTOSPIN, opcode)
        assertEquals(2, payload[0].toInt())
        assertEquals(true, viewModel.uiState.value.profiles[2].autospin)
    }

    @Test
    fun `repository failure surfaces error message in uiState`() = runTest {
        val repository = FakeBleControlRepository()
        repository.stubResponse(Opcode.CYCLE_LOG_LEVEL, Result.failure(IOException("disconnected")))
        val viewModel = DeviceViewModel(repository)

        viewModel.cycleLogLevel()
        dispatcher.scheduler.advanceUntilIdle()

        assertEquals("disconnected", viewModel.uiState.value.errorMessage)
    }

    @Test
    fun `non-OK device status surfaces error message and leaves state unchanged`() = runTest {
        val repository = FakeBleControlRepository()
        repository.stubResponse(
            Opcode.CYCLE_LOG_LEVEL,
            Result.success(ResponseFrame(StatusCode.ERR_BUSY, Opcode.CYCLE_LOG_LEVEL.toByte(), ByteArray(0))),
        )
        val viewModel = DeviceViewModel(repository)

        viewModel.cycleLogLevel()
        dispatcher.scheduler.advanceUntilIdle()

        assertEquals("device returned status ${StatusCode.ERR_BUSY}", viewModel.uiState.value.errorMessage)
        assertNull(viewModel.uiState.value.status.logLevel)
    }

    @Test
    fun `refreshRuntimeStats attributes caught fled spin to the right profile slot`() = runTest {
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
}
