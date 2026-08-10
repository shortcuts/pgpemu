package com.pgpemu.companion.ble

sealed interface ConnectionState {
    data object Idle : ConnectionState
    data object Scanning : ConnectionState
    data object Connecting : ConnectionState
    data object Bonding : ConnectionState
    data object DiscoveringServices : ConnectionState
    data object Ready : ConnectionState
    data class Disconnected(val reason: String) : ConnectionState
    data class Error(val reason: String) : ConnectionState
}
