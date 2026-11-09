package com.pgpemu.companion.pairing

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.os.Handler
import android.os.Looper
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import kotlin.concurrent.thread

interface PrivilegedPairingAccess {
    fun requestPermission(onResult: (Boolean) -> Unit)

    fun setReceiverEnabled(enabled: Boolean)
}

class AndroidPrivilegedPairingAccess
    @Inject
    constructor(
        @ApplicationContext private val context: Context,
    ) : PrivilegedPairingAccess {
        override fun requestPermission(onResult: (Boolean) -> Unit) {
            thread {
                val granted = RootBluetooth.isRootGranted()
                Handler(Looper.getMainLooper()).post { onResult(granted) }
            }
        }

        override fun setReceiverEnabled(enabled: Boolean) {
            context.packageManager.setComponentEnabledSetting(
                ComponentName(context, PrivilegedPairingReceiver::class.java),
                if (enabled) PackageManager.COMPONENT_ENABLED_STATE_ENABLED else PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP,
            )
        }
    }
