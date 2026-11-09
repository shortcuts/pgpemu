package com.pgpemu.companion.pairing

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PairingServiceAccessTest {
    private val pkg = "com.pgpemu.companion"
    private val cls = "com.pgpemu.companion.pairing.PairingAssistantService"

    @Test
    fun `flattened form is listed`() = assertTrue(isServiceListed("a/b:$pkg/$cls", pkg, cls))

    @Test
    fun `short form is listed`() = assertTrue(isServiceListed("$pkg/.pairing.PairingAssistantService", pkg, cls))

    @Test
    fun `match ignores case`() = assertTrue(isServiceListed("COM.PGPEMU.COMPANION/$cls", pkg, cls))

    @Test
    fun `null, empty and other services are not listed`() {
        assertFalse(isServiceListed(null, pkg, cls))
        assertFalse(isServiceListed("", pkg, cls))
        assertFalse(isServiceListed("other.app/other.Service", pkg, cls))
    }
}
