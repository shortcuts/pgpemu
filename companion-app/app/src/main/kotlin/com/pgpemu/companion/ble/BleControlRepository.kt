package com.pgpemu.companion.ble

import kotlinx.coroutines.flow.StateFlow

interface BleControlRepository {
    val connectionState: StateFlow<ConnectionState>

    suspend fun connect()

    suspend fun disconnect()

    suspend fun sendCommand(opcode: Int, payload: ByteArray = ByteArray(0)): Result<ResponseFrame>
}
