package com.pgpemu.companion.monitor

import android.service.notification.NotificationListenerService
import org.junit.Assert.assertEquals
import org.junit.Test

class GameMonitorTest {
    private class FakeGameAlerts : GameAlerts {
        val calls = mutableListOf<String>()

        override fun post(
            alert: GameAlert,
            text: String?,
            gamePackage: String,
        ) {
            calls += "post ${alert.name} $text $gamePackage"
        }

        override fun cancel(alert: GameAlert) {
            calls += "cancel ${alert.name}"
        }
    }

    private val strings =
        GameStrings(
            disconnecting = setOf("Disconnecting", "切断中"),
            itemInventoryFull = setOf("Bag full"),
            pokemonInventoryFull = setOf("Box full"),
            outOfPokeballs = setOf("No balls"),
            capturedPokemon = setOf("Caught"),
            escapedPokemon = setOf("Escaped"),
            retrievedAnItem = setOf("Got an item"),
            ignoredPokestop = setOf("Cooldown"),
            retrievedItems = setOf("Got %s items!"),
        )
    private val alerts = FakeGameAlerts()
    private val monitor = GameMonitor(alerts)
    private val pkg = "com.nianticlabs.pokemongo"

    private fun text(value: String?) = monitor.onText(value, strings, pkg)

    @Test
    fun `empty text cancels the disconnect alert`() {
        text(null)
        text("")
        assertEquals(listOf("cancel Disconnected", "cancel Disconnected"), alerts.calls)
    }

    @Test
    fun `disconnecting text in any locale posts disconnected`() {
        text("切断中")
        assertEquals(listOf("post Disconnected null $pkg"), alerts.calls)
    }

    @Test
    fun `full and empty texts post their alert with the game text`() {
        text("Bag full")
        text("Box full")
        text("No balls")
        assertEquals(
            listOf("post BagFull Bag full $pkg", "post BoxFull Box full $pkg", "post BallsEmpty No balls $pkg"),
            alerts.calls,
        )
    }

    @Test
    fun `catch and escape cancel box and balls alerts`() {
        text("Caught")
        text("Escaped")
        assertEquals(List(2) { listOf("cancel BoxFull", "cancel BallsEmpty") }.flatten(), alerts.calls)
    }

    @Test
    fun `single item cancels bag and spin alerts`() {
        text("Got an item")
        assertEquals(listOf("cancel BagFull", "cancel SpinFailed"), alerts.calls)
    }

    @Test
    fun `ignored pokestop text does nothing`() {
        text("Cooldown")
        assertEquals(emptyList<String>(), alerts.calls)
    }

    @Test
    fun `zero items posts spin failed and cancels bag`() {
        text("Got 0 items!")
        assertEquals(listOf("cancel BagFull", "post SpinFailed Got 0 items! $pkg"), alerts.calls)
    }

    @Test
    fun `positive items cancels bag and spin alerts`() {
        text("Got 12 items!")
        assertEquals(listOf("cancel BagFull", "cancel SpinFailed"), alerts.calls)
    }

    @Test
    fun `non numeric count and unknown text do nothing`() {
        text("Got many items!")
        text("Something else")
        assertEquals(emptyList<String>(), alerts.calls)
    }

    @Test
    fun `removal by the app or system posts disconnected`() {
        listOf(
            NotificationListenerService.REASON_PACKAGE_CHANGED,
            NotificationListenerService.REASON_USER_STOPPED,
            NotificationListenerService.REASON_APP_CANCEL,
            NotificationListenerService.REASON_PACKAGE_SUSPENDED,
            NotificationListenerService.REASON_PROFILE_TURNED_OFF,
            NotificationListenerService.REASON_CLEAR_DATA,
        ).forEach(monitor::onRemoved)
        assertEquals(List(6) { "post Disconnected null " }, alerts.calls)
    }

    @Test
    fun `removal by the user does nothing`() {
        monitor.onRemoved(NotificationListenerService.REASON_CLICK)
        monitor.onRemoved(NotificationListenerService.REASON_CANCEL)
        assertEquals(emptyList<String>(), alerts.calls)
    }
}
