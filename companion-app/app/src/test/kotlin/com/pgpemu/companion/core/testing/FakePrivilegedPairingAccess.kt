package com.pgpemu.companion.core.testing

import com.pgpemu.companion.pairing.PrivilegedPairingAccess

class FakePrivilegedPairingAccess(
    var grantOnRequest: Boolean = false,
) : PrivilegedPairingAccess {
    var receiverOn = false
    var requests = 0

    override fun requestPermission(onResult: (Boolean) -> Unit) {
        requests++
        onResult(grantOnRequest)
    }

    override fun setReceiverEnabled(enabled: Boolean) {
        receiverOn = enabled
    }
}
