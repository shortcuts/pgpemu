package com.pgpemu.companion.core.testing

import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.ResponseFrame
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class FakeBleControlRepository : BleControlRepository {
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Idle)
    override val connectionState: StateFlow<ConnectionState> = _connectionState

    fun setConnectionState(state: ConnectionState) {
        _connectionState.value = state
    }

    override suspend fun connect() = Unit

    override suspend fun disconnect() = Unit

    override suspend fun sendCommand(opcode: Int, payload: ByteArray): Result<ResponseFrame> =
        Result.failure(UnsupportedOperationException("not stubbed"))
}
