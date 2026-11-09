// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: Timber removed, decision moved to confirmPrivileged.
package com.pgpemu.companion.pairing

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import androidx.core.content.IntentCompat
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject
import kotlin.concurrent.thread

@AndroidEntryPoint
class PrivilegedPairingReceiver : BroadcastReceiver() {
    @Inject lateinit var alerts: PairingAlerts

    @SuppressLint("MissingPermission")
    override fun onReceive(
        context: Context,
        intent: Intent,
    ) {
        if (intent.action != BluetoothDevice.ACTION_PAIRING_REQUEST) return
        val variant = intent.getIntExtra(BluetoothDevice.EXTRA_PAIRING_VARIANT, BluetoothDevice.ERROR)
        val device =
            IntentCompat.getParcelableExtra(intent, BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java) ?: return
        val pending = goAsync()
        // su takes up to seconds, so the main thread must not wait for it.
        thread {
            val outcome =
                confirmPrivileged(
                    variant = variant,
                    isPgp = isPokemonGoPlusDevice(device.type, device.name, device.address),
                    direct = { device.setPairingConfirmation(true) },
                    viaRoot = { RootBluetooth.setPairingConfirmation(context, device) },
                )
            when (outcome) {
                PrivilegedOutcome.Ignored -> Unit
                // Abort only after confirming, so every failure keeps Android's normal dialog.
                PrivilegedOutcome.Confirmed -> {
                    pending.abortBroadcast()
                    alerts.cleared()
                }
                PrivilegedOutcome.Failed -> alerts.failed()
            }
            pending.finish()
        }
    }
}
