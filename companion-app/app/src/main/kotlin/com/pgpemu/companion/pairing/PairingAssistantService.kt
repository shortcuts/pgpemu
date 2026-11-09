// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: Timber and coroutines removed, matching delegated to PairingMatcher.
package com.pgpemu.companion.pairing

import android.accessibilityservice.AccessibilityService
import android.annotation.SuppressLint
import android.app.Notification
import android.app.PendingIntent
import android.content.Intent
import android.content.res.Resources
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo

class PairingAssistantService : AccessibilityService() {
    private val handler = Handler(Looper.getMainLooper())
    private val rescan = Runnable { tryConfirm() }

    override fun onServiceConnected() {
        super.onServiceConnected()
        tryConfirm()
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent) {
        when (event.eventType) {
            AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED ->
                onNotification(event.parcelableData as? Notification ?: return, event.packageName)
            AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED -> {
                val source = if (Build.VERSION.SDK_INT >= 33) event.getSource(0) else event.source
                tryConfirm(source ?: return)
            }
        }
    }

    private fun onNotification(
        notification: Notification,
        packageName: CharSequence,
    ) {
        if (!isPairingNotification(
                packageName,
                notification.channelId,
                notification.extras.getCharSequence(Notification.EXTRA_TEXT),
            )
        ) {
            return
        }
        val intent = notification.actions?.firstOrNull()?.actionIntent ?: notification.contentIntent ?: return
        try {
            intent.send()
        } catch (_: PendingIntent.CanceledException) {
            return
        }
        performGlobalAction(GLOBAL_ACTION_DISMISS_NOTIFICATION_SHADE)
        // Some devices fire no window event for the dialog.
        handler.removeCallbacks(rescan)
        handler.postDelayed(rescan, RESCAN_DELAY_MS)
    }

    private fun tryConfirm() {
        for (window in windows) {
            val root = if (Build.VERSION.SDK_INT >= 33) window.getRoot(0) else window.root
            if (root != null && tryConfirm(root)) return
        }
    }

    private fun tryConfirm(root: AccessibilityNodeInfo): Boolean {
        val node = locateConfirm(NodeRoot(root), settingsStrings(root.packageName?.toString())) ?: return false
        node.click()
        handler.removeCallbacks(rescan)
        return true
    }

    private fun settingsStrings(packageName: String?): SettingsStrings {
        val resources =
            try {
                packageManager.getResourcesForApplication(packageName ?: return SettingsStrings(null, null, null))
            } catch (_: android.content.pm.PackageManager.NameNotFoundException) {
                return SettingsStrings(null, null, null)
            }
        return SettingsStrings(
            resources.findString("bluetooth_pairing_accept", packageName),
            resources.findString("bluetooth_pairing_request", packageName, PAIRING_DEVICE_NAME),
            resources.findString("bluetooth_pair_message_2", packageName, PAIRING_DEVICE_NAME),
        )
    }

    // Android resolves the Settings package's own strings by name only.
    @SuppressLint("DiscouragedApi")
    private fun Resources.findString(
        name: String,
        packageName: String,
        vararg formatArgs: Any,
    ) = try {
        getString(getIdentifier(name, "string", packageName), *formatArgs)
    } catch (_: Resources.NotFoundException) {
        null
    }

    override fun onInterrupt() = Unit

    override fun onUnbind(intent: Intent?): Boolean {
        handler.removeCallbacks(rescan)
        return super.onUnbind(intent)
    }

    private class NodeRoot(
        private val root: AccessibilityNodeInfo,
    ) : PairingRoot {
        override val packageName = root.packageName?.toString()

        override fun findById(id: String) = root.findAccessibilityNodeInfosByViewId(id).map(::Node)

        override fun findByText(text: String) = root.findAccessibilityNodeInfosByText(text).map(::Node)
    }

    private class Node(
        private val info: AccessibilityNodeInfo,
    ) : PairingNode {
        override val text: CharSequence? get() = info.text

        override fun click() {
            info.performAction(AccessibilityNodeInfo.ACTION_CLICK)
        }
    }

    private companion object {
        const val RESCAN_DELAY_MS = 1000L
    }
}
