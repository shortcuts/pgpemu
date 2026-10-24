// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: Sfida, Shizuku and Timber code removed, classification delegated to GameMonitor.
package com.pgpemu.companion.monitor

import android.app.Notification
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.os.Build
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject

@AndroidEntryPoint
class GameNotificationService : NotificationListenerService() {
    @Inject lateinit var store: MonitorStore

    @Inject lateinit var alerts: GameAlerts

    private val monitor by lazy { GameMonitor(alerts) }

    private val gameStringsCache =
        GameStringsCache { packageName ->
            try {
                createPackageContext(packageName, 0).loadGameStrings()
            } catch (_: PackageManager.NameNotFoundException) {
                null
            }
        }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        gameStringsCache.clear()
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        if (!store.enabled || !isInterested(sbn)) return
        // Before Android 12 the game reposts its notification; only the foreground-service original counts.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S &&
            sbn.notification.flags and Notification.FLAG_FOREGROUND_SERVICE != Notification.FLAG_FOREGROUND_SERVICE
        ) {
            return
        }
        @Suppress("DEPRECATION")
        val packageInfo =
            try {
                packageManager.getPackageInfo(sbn.packageName, 0)
            } catch (_: PackageManager.NameNotFoundException) {
                gameStringsCache.clear()
                return
            }
        val applicationInfo = packageInfo.applicationInfo ?: return
        val strings =
            gameStringsCache.get(
                GamePackageRevision(
                    packageName = sbn.packageName,
                    longVersionCode = packageInfo.longVersionCode,
                    lastUpdateTime = packageInfo.lastUpdateTime,
                    publicSourceDir = applicationInfo.publicSourceDir,
                    splitPublicSourceDirs = applicationInfo.splitPublicSourceDirs.orEmpty().toList(),
                ),
            ) ?: return
        monitor.onText(sbn.notification.extras.getString(Notification.EXTRA_TEXT), strings, sbn.packageName)
    }

    override fun onNotificationRemoved(
        sbn: StatusBarNotification?,
        rankingMap: RankingMap?,
        reason: Int,
    ) {
        if (store.enabled && sbn != null && isInterested(sbn)) monitor.onRemoved(reason)
    }

    private fun isInterested(sbn: StatusBarNotification) =
        sbn.notification.channelId == sbn.packageName && sbn.packageName in POKEMON_GO_PACKAGES
}
