package com.pgpemu.companion.core.testing

import com.pgpemu.companion.pairing.PairingServiceAccess

class FakePairingServiceAccess(
    var enabled: Boolean = false,
) : PairingServiceAccess {
    override fun isEnabled() = enabled
}
