package com.pgpemu.companion.pairing

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import com.pgpemu.companion.MainActivity
import com.pgpemu.companion.R
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton

interface PairingAlerts {
    fun failed()

    fun cleared()
}

@Singleton
class AndroidPairingAlerts
    @Inject
    constructor(
        @ApplicationContext private val context: Context,
    ) : PairingAlerts {
        private val manager = context.getSystemService(NotificationManager::class.java)

        override fun failed() {
            manager.createNotificationChannel(
                NotificationChannel(CHANNEL_ID, context.getText(R.string.pairing_channel_name), NotificationManager.IMPORTANCE_DEFAULT),
            )
            manager.notify(
                NOTIFICATION_ID,
                Notification
                    .Builder(context, CHANNEL_ID)
                    .setCategory(Notification.CATEGORY_ERROR)
                    .setContentTitle(context.getText(R.string.pairing_failed_title))
                    .setContentText(context.getText(R.string.pairing_failed_text))
                    .setSmallIcon(R.drawable.ic_monitor_alert)
                    .setOnlyAlertOnce(true)
                    .setAutoCancel(true)
                    .setContentIntent(
                        PendingIntent.getActivity(
                            context,
                            0,
                            Intent(context, MainActivity::class.java),
                            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
                        ),
                    ).build(),
            )
        }

        override fun cleared() = manager.cancel(NOTIFICATION_ID)

        private companion object {
            const val CHANNEL_ID = "pairing_assistant"
            const val NOTIFICATION_ID = 1000
        }
    }
