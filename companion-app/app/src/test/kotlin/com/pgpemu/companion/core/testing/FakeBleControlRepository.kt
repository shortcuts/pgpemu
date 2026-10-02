package com.pgpemu.companion.core.testing

import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.ResponseFrame
import com.pgpemu.companion.ble.ScannedDevice
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class FakeBleControlRepository : BleControlRepository {
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Idle)
    override val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _discoveredDevices = MutableStateFlow<List<ScannedDevice>>(emptyList())
    override val discoveredDevices: StateFlow<List<ScannedDevice>> = _discoveredDevices

    var startScanCalls = 0
        private set
    var stopScanCalls = 0
        private set
    val connectedAddresses = mutableListOf<String>()

    private val responses = mutableMapOf<Int, Result<ResponseFrame>>()
    val sentCommands = mutableListOf<Pair<Int, ByteArray>>()

    fun setConnectionState(state: ConnectionState) {
        _connectionState.value = state
    }

    fun setDiscoveredDevices(devices: List<ScannedDevice>) {
        _discoveredDevices.value = devices
    }

    fun stubResponse(opcode: Int, result: Result<ResponseFrame>) {
        responses[opcode] = result
    }

    override suspend fun startScan() {
        startScanCalls++
    }

    override fun stopScan() {
        stopScanCalls++
    }

    override suspend fun connect(address: String) {
        connectedAddresses.add(address)
    }

    override suspend fun disconnect() = Unit

    override suspend fun sendCommand(opcode: Int, payload: ByteArray): Result<ResponseFrame> {
        sentCommands.add(opcode to payload)
        return responses[opcode] ?: Result.failure(UnsupportedOperationException("not stubbed: opcode=$opcode"))
    }
}
