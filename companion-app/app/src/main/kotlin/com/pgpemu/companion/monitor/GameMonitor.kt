// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: classification split out of GameNotificationService, stats and Sfida bookkeeping removed.
package com.pgpemu.companion.monitor

import android.service.notification.NotificationListenerService

enum class GameAlert { Disconnected, BagFull, BoxFull, BallsEmpty, SpinFailed }

interface GameAlerts {
    fun post(
        alert: GameAlert,
        text: String?,
        gamePackage: String,
    )

    fun cancel(alert: GameAlert)
}

internal class GameMonitor(
    private val alerts: GameAlerts,
) {
    fun onText(
        text: String?,
        strings: GameStrings,
        gamePackage: String,
    ) {
        when {
            text.isNullOrEmpty() -> alerts.cancel(GameAlert.Disconnected)
            text in strings.disconnecting -> alerts.post(GameAlert.Disconnected, null, gamePackage)
            text in strings.itemInventoryFull -> alerts.post(GameAlert.BagFull, text, gamePackage)
            text in strings.pokemonInventoryFull -> alerts.post(GameAlert.BoxFull, text, gamePackage)
            text in strings.outOfPokeballs -> alerts.post(GameAlert.BallsEmpty, text, gamePackage)
            text in strings.capturedPokemon || text in strings.escapedPokemon -> {
                alerts.cancel(GameAlert.BoxFull)
                alerts.cancel(GameAlert.BallsEmpty)
            }
            text in strings.retrievedAnItem -> {
                alerts.cancel(GameAlert.BagFull)
                alerts.cancel(GameAlert.SpinFailed)
            }
            text in strings.ignoredPokestop -> Unit
            else -> onItemCount(text, strings, gamePackage)
        }
    }

    private fun onItemCount(
        text: String,
        strings: GameStrings,
        gamePackage: String,
    ) {
        val count =
            strings.retrievedItems
                .firstNotNullOfOrNull { template ->
                    val (prefix, suffix) = template.split("%s", limit = 2).takeIf { it.size == 2 } ?: return@firstNotNullOfOrNull null
                    if (text.startsWith(prefix) && text.endsWith(suffix) && text.length >= prefix.length + suffix.length) {
                        text.substring(prefix.length, text.length - suffix.length)
                    } else {
                        null
                    }
                }?.toLongOrNull() ?: return
        alerts.cancel(GameAlert.BagFull)
        if (count == 0L) alerts.post(GameAlert.SpinFailed, text, gamePackage) else alerts.cancel(GameAlert.SpinFailed)
    }

    fun onRemoved(reason: Int) {
        if (reason in GONE_REASONS) alerts.post(GameAlert.Disconnected, null, "")
    }

    private companion object {
        val GONE_REASONS =
            setOf(
                NotificationListenerService.REASON_PACKAGE_CHANGED,
                NotificationListenerService.REASON_USER_STOPPED,
                NotificationListenerService.REASON_APP_CANCEL,
                NotificationListenerService.REASON_PACKAGE_SUSPENDED,
                NotificationListenerService.REASON_PROFILE_TURNED_OFF,
                NotificationListenerService.REASON_CLEAR_DATA,
            )
    }
}
