package com.pgpemu.companion.monitor

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import androidx.annotation.StringRes
import com.pgpemu.companion.R
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class AndroidGameAlerts
    @Inject
    constructor(
        @ApplicationContext private val context: Context,
    ) : GameAlerts {
        private val manager = context.getSystemService(NotificationManager::class.java)

        override fun post(
            alert: GameAlert,
            text: String?,
            gamePackage: String,
        ) {
            manager.createNotificationChannel(
                NotificationChannel(alert.channelId, context.getText(alert.channelName), NotificationManager.IMPORTANCE_HIGH)
                    .apply { lockscreenVisibility = Notification.VISIBILITY_PUBLIC },
            )
            val launch = context.packageManager.getLaunchIntentForPackage(gamePackage)
            manager.notify(
                alert.ordinal + 1,
                Notification
                    .Builder(context, alert.channelId)
                    .setCategory(Notification.CATEGORY_STATUS)
                    .setContentTitle(text ?: context.getText(R.string.monitor_alert_disconnected))
                    .setSmallIcon(R.drawable.ic_monitor_alert)
                    .setOnlyAlertOnce(true)
                    .setAutoCancel(true)
                    .setVisibility(Notification.VISIBILITY_PUBLIC)
                    .apply {
                        launch?.let {
                            setContentIntent(
                                PendingIntent.getActivity(
                                    context,
                                    0,
                                    it,
                                    PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
                                ),
                            )
                        }
                    }.build(),
            )
        }

        override fun cancel(alert: GameAlert) = manager.cancel(alert.ordinal + 1)

        private val GameAlert.channelId
            get() =
                when (this) {
                    GameAlert.Disconnected -> "game_disconnected"
                    GameAlert.BagFull -> "game_bag_full"
                    GameAlert.BoxFull -> "game_box_full"
                    GameAlert.BallsEmpty -> "game_balls_empty"
                    GameAlert.SpinFailed -> "game_spin_failed"
                }

        @get:StringRes
        private val GameAlert.channelName
            get() =
                when (this) {
                    GameAlert.Disconnected -> R.string.monitor_channel_disconnected
                    GameAlert.BagFull -> R.string.monitor_channel_bag_full
                    GameAlert.BoxFull -> R.string.monitor_channel_box_full
                    GameAlert.BallsEmpty -> R.string.monitor_channel_balls_empty
                    GameAlert.SpinFailed -> R.string.monitor_channel_spin_failed
                }
    }
