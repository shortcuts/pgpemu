package com.pgpemu.companion.monitor

import android.content.Context
import androidx.core.app.NotificationManagerCompat
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

interface NotificationAccess {
    fun isGranted(): Boolean
}

class AndroidNotificationAccess
    @Inject
    constructor(
        @ApplicationContext private val context: Context,
    ) : NotificationAccess {
        override fun isGranted() = context.packageName in NotificationManagerCompat.getEnabledListenerPackages(context)
    }
