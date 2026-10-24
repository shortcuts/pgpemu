package com.pgpemu.companion.core.testing

import com.pgpemu.companion.ble.LastDeviceStore

class FakeLastDeviceStore(
    override var address: String? = null,
) : LastDeviceStore
