package com.pgpemu.companion.ble

/** A PGP device seen during a scan. Plain data so UI and tests never touch BluetoothDevice. */
data class ScannedDevice(
    val address: String,
    val name: String,
    val rssi: Int,
)
