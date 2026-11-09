// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: matching logic moved behind PairingNode and PairingRoot so JVM tests drive it.
package com.pgpemu.companion.pairing

import android.bluetooth.BluetoothDevice

const val PAIRING_DEVICE_NAME = "Pokemon GO Plus"

private const val PAIRING_DEVICE_NAME_PLUS = "Pokemon GO Plus +"
private val SETTINGS_PACKAGES =
    setOf(
        "com.android.settings",
        // Realme and other ColorOS phones.
        "com.coloros.wirelesssettings",
        // Oppo and OnePlus.
        "com.oplus.wirelesssettings",
    )
private val SETTINGS_CHANNELS = setOf("bluetooth_notification_channel", "Wireless_bt_channel")
private val DEVICE_NAMES = setOf(PAIRING_DEVICE_NAME, PAIRING_DEVICE_NAME_PLUS, "Pokemon PBP", "EbisuEbisu test")
private val DEVICE_OUIS = listOf("7C:BB:8A:", "98:B6:E9:", "B8:78:26:")

interface PairingNode {
    val text: CharSequence?

    fun click()
}

interface PairingRoot {
    val packageName: String?

    fun findById(id: String): List<PairingNode>

    fun findByText(text: String): List<PairingNode>
}

/** Strings from the Settings package; request and message2 are already formatted with the device name. */
data class SettingsStrings(
    val accept: String?,
    val request: String?,
    val message2: String?,
)

fun isPairingNotification(
    packageName: CharSequence?,
    channelId: String?,
    text: CharSequence?,
) = packageName in SETTINGS_PACKAGES && channelId in SETTINGS_CHANNELS && text?.contains(PAIRING_DEVICE_NAME) == true

fun isPokemonGoPlusDevice(
    type: Int,
    name: String?,
    address: String,
): Boolean {
    val bleOrUnknown = type == BluetoothDevice.DEVICE_TYPE_UNKNOWN || type == BluetoothDevice.DEVICE_TYPE_LE
    val known = if (name == null) DEVICE_OUIS.any { address.startsWith(it, ignoreCase = true) } else name in DEVICE_NAMES
    return bleOrUnknown && known
}

fun locateConfirm(
    root: PairingRoot,
    strings: SettingsStrings,
): PairingNode? = locateById(root) ?: locateByText(root, strings)

private fun PairingNode.namesDevice() = text?.contains(PAIRING_DEVICE_NAME) == true

private fun locateById(root: PairingRoot): PairingNode? {
    val pkg = root.packageName
    val confirm =
        root.findById("android:id/button1").ifEmpty {
            // Samsung uses an AppCompat dialog.
            root.findById("$pkg:id/button1").ifEmpty {
                // The Android 16 Pixel dialog uses an expressive button container.
                root.findById("$pkg:id/positive_button_container")
            }
        }
    if (confirm.size != 1) return null
    val ownTitle = root.findById("$pkg:id/alertTitle")
    val title = ownTitle.ifEmpty { root.findById("android:id/alertTitle") }
    if (title.size == 1 && title[0].namesDevice()) return confirm[0]
    // Samsung puts the name in the message, Oplus in a name field.
    val named = listOf("message", "name").any { root.findById("$pkg:id/$it").singleOrNull()?.namesDevice() == true }
    return if (named) confirm[0] else null
}

private fun locateByText(
    root: PairingRoot,
    strings: SettingsStrings,
): PairingNode? {
    val accept = strings.accept ?: return null
    val confirm = root.findByText(accept).filter { accept.equals(it.text?.toString(), ignoreCase = true) }
    if (confirm.size != 1) return null
    val prompts = listOfNotNull(strings.request, strings.message2)
    if (prompts.any { prompt -> root.findByText(prompt).any { it.text == prompt } }) return confirm[0]
    // ColorOS shows the bare device name in its own text view.
    val names = root.findByText(PAIRING_DEVICE_NAME)
    return if (names.isNotEmpty() && names.all { it.text == PAIRING_DEVICE_NAME }) confirm[0] else null
}
