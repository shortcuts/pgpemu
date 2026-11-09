package com.pgpemu.companion.core.testing

import com.pgpemu.companion.pairing.PairingStore

class FakePairingStore(
    override var privilegedEnabled: Boolean = false,
) : PairingStore
