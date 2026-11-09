package com.pgpemu.companion.pairing

import android.bluetooth.BluetoothDevice
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class PairingMatcherTest {
    private class FakeNode(
        override val text: CharSequence? = null,
    ) : PairingNode {
        var clicks = 0

        override fun click() {
            clicks++
        }
    }

    private class FakeRoot(
        override val packageName: String? = "com.android.settings",
        val byId: Map<String, List<PairingNode>> = emptyMap(),
        val byText: Map<String, List<PairingNode>> = emptyMap(),
    ) : PairingRoot {
        override fun findById(id: String) = byId[id].orEmpty()

        override fun findByText(text: String) = byText[text].orEmpty()
    }

    private val none = SettingsStrings(null, null, null)
    private val confirm = FakeNode("Pair")
    private val pgpTitle = FakeNode("Pair with Pokemon GO Plus?")

    @Test
    fun `android button1 with the name in the title`() {
        val root =
            FakeRoot(byId = mapOf("android:id/button1" to listOf(confirm), "com.android.settings:id/alertTitle" to listOf(pgpTitle)))
        assertSame(confirm, locateConfirm(root, none))
    }

    @Test
    fun `package button1 with the name in the message`() {
        val root =
            FakeRoot(
                byId =
                    mapOf(
                        "com.android.settings:id/button1" to listOf(confirm),
                        "com.android.settings:id/message" to listOf(pgpTitle),
                    ),
            )
        assertSame(confirm, locateConfirm(root, none))
    }

    @Test
    fun `oplus name field`() {
        val root =
            FakeRoot(
                packageName = "com.oplus.wirelesssettings",
                byId =
                    mapOf(
                        "android:id/button1" to listOf(confirm),
                        "com.oplus.wirelesssettings:id/name" to listOf(pgpTitle),
                    ),
            )
        assertSame(confirm, locateConfirm(root, none))
    }

    @Test
    fun `android 16 pixel button container with the platform title`() {
        val root =
            FakeRoot(
                byId =
                    mapOf(
                        "com.android.settings:id/positive_button_container" to listOf(confirm),
                        "android:id/alertTitle" to listOf(pgpTitle),
                    ),
            )
        assertSame(confirm, locateConfirm(root, none))
    }

    @Test
    fun `two button matches, another device name and a missing button return null`() {
        val other = FakeNode("Pair with Headphones?")
        val title = mapOf("com.android.settings:id/alertTitle" to listOf(pgpTitle))
        assertNull(locateConfirm(FakeRoot(byId = title + ("android:id/button1" to listOf(confirm, FakeNode("Pair")))), none))
        assertNull(
            locateConfirm(
                FakeRoot(
                    byId =
                        mapOf(
                            "android:id/button1" to listOf(confirm),
                            "com.android.settings:id/alertTitle" to listOf(other),
                        ),
                ),
                none,
            ),
        )
        assertNull(locateConfirm(FakeRoot(byId = title), none))
    }

    @Test
    fun `settings strings find the button when no view id matches`() {
        val strings = SettingsStrings("Pair", "Pair with Pokemon GO Plus?", null)
        val prompt = FakeNode("Pair with Pokemon GO Plus?")
        val root = FakeRoot(byText = mapOf("Pair" to listOf(confirm), "Pair with Pokemon GO Plus?" to listOf(prompt)))
        assertSame(confirm, locateConfirm(root, strings))
    }

    @Test
    fun `bare device name text confirms and another name does not`() {
        val strings = SettingsStrings("Pair", "x", null)
        val bare = FakeNode("Pokemon GO Plus")
        assertSame(confirm, locateConfirm(FakeRoot(byText = mapOf("Pair" to listOf(confirm), "Pokemon GO Plus" to listOf(bare))), strings))
        val plus = FakeNode("Pokemon GO Plus +")
        assertNull(locateConfirm(FakeRoot(byText = mapOf("Pair" to listOf(confirm), "Pokemon GO Plus" to listOf(plus))), strings))
    }

    @Test
    fun `notification needs a settings package, a bluetooth channel and the name`() {
        val text = "Pair with Pokemon GO Plus"
        assertTrue(isPairingNotification("com.android.settings", "bluetooth_notification_channel", text))
        assertTrue(isPairingNotification("com.coloros.wirelesssettings", "Wireless_bt_channel", text))
        assertTrue(isPairingNotification("com.oplus.wirelesssettings", "Wireless_bt_channel", text))
        assertFalse(isPairingNotification("com.example", "bluetooth_notification_channel", text))
        assertFalse(isPairingNotification("com.android.settings", "other", text))
        assertFalse(isPairingNotification("com.android.settings", "bluetooth_notification_channel", "Pair with Headphones"))
        assertFalse(isPairingNotification("com.android.settings", null, null))
    }

    @Test
    fun `device filter accepts the name list and our OUI and rejects the rest`() {
        val le = BluetoothDevice.DEVICE_TYPE_LE
        assertTrue(isPokemonGoPlusDevice(le, "Pokemon GO Plus", "AA:BB:CC:DD:EE:FF"))
        assertTrue(isPokemonGoPlusDevice(BluetoothDevice.DEVICE_TYPE_UNKNOWN, "Pokemon GO Plus +", "AA:BB:CC:DD:EE:FF"))
        assertTrue(isPokemonGoPlusDevice(le, "Pokemon PBP", "AA:BB:CC:DD:EE:FF"))
        assertTrue(isPokemonGoPlusDevice(le, null, "7c:bb:8a:00:00:01"))
        assertFalse(isPokemonGoPlusDevice(le, null, "AA:BB:CC:DD:EE:FF"))
        assertFalse(isPokemonGoPlusDevice(le, "Headphones", "7C:BB:8A:00:00:01"))
        assertFalse(isPokemonGoPlusDevice(BluetoothDevice.DEVICE_TYPE_CLASSIC, "Pokemon GO Plus", "7C:BB:8A:00:00:01"))
    }
}
