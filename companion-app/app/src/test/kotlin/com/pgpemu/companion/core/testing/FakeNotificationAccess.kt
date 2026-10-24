package com.pgpemu.companion.core.testing

import com.pgpemu.companion.monitor.NotificationAccess

class FakeNotificationAccess(
    var granted: Boolean = false,
) : NotificationAccess {
    override fun isGranted() = granted
}
