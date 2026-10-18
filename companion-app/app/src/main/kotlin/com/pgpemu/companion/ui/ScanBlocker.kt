package com.pgpemu.companion.ui

enum class ScanBlocker { PermissionDenied, PermissionPermanentlyDenied, BluetoothOff }

// Permission outranks adapter state: the adapter cannot be enabled without BLUETOOTH_CONNECT on API 31+.
fun scanBlocker(
    granted: Boolean,
    canAskAgain: Boolean,
    adapterOn: Boolean,
): ScanBlocker? =
    when {
        !granted -> if (canAskAgain) ScanBlocker.PermissionDenied else ScanBlocker.PermissionPermanentlyDenied
        !adapterOn -> ScanBlocker.BluetoothOff
        else -> null
    }
