package com.pgpemu.companion.ble

import android.content.IntentSender
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow

interface BleControlRepository {
    val connectionState: StateFlow<ConnectionState>

    /** System pairing dialogs the UI must launch (CompanionDeviceManager association). */
    val associationRequests: SharedFlow<IntentSender>

    /** Devices found by the current/last scan, strongest signal first. */
    val discoveredDevices: StateFlow<List<ScannedDevice>>

    /** Clears the list and scans until the scan timeout elapses or [stopScan] is called. */
    suspend fun startScan()

    fun stopScan()

    suspend fun connect(address: String)

    suspend fun disconnect()

    suspend fun sendCommand(
        opcode: Int,
        payload: ByteArray = ByteArray(0),
    ): Result<ResponseFrame>
}
