package com.pgpemu.companion.ble

sealed interface ConnectionState {
    data object Idle : ConnectionState

    data object Scanning : ConnectionState

    data object Connecting : ConnectionState

    data object Bonding : ConnectionState

    data object DiscoveringServices : ConnectionState

    data object Ready : ConnectionState

    /** [reason] is the GATT status of an unexpected drop; null when the user asked for it. */
    data class Disconnected(
        val reason: Int? = null,
    ) : ConnectionState

    data class Error(
        val reason: String,
    ) : ConnectionState
}
