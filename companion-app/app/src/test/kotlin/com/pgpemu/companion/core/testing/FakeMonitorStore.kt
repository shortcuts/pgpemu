package com.pgpemu.companion.core.testing

import com.pgpemu.companion.monitor.MonitorStore

class FakeMonitorStore(
    override var enabled: Boolean = false,
) : MonitorStore
