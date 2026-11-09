package com.pgpemu.companion.pairing

import org.junit.Assert.assertEquals
import org.junit.Test

class PrivilegedPairingTest {
    private var viaRootCalls = 0

    private fun run(
        variant: Int = PAIRING_VARIANT_CONSENT,
        isPgp: Boolean = true,
        direct: () -> Boolean = { throw SecurityException() },
        viaRoot: () -> Boolean = { true },
    ) = confirmPrivileged(variant, isPgp, direct, { viaRootCalls++.let { viaRoot() } })

    @Test
    fun `other variants and other devices are ignored`() {
        assertEquals(PrivilegedOutcome.Ignored, run(variant = 2))
        assertEquals(PrivilegedOutcome.Ignored, run(isPgp = false))
    }

    @Test
    fun `direct confirmation never touches root`() {
        assertEquals(PrivilegedOutcome.Confirmed, run(direct = { true }))
        assertEquals(0, viaRootCalls)
    }

    @Test
    fun `security exception falls back to root`() {
        assertEquals(PrivilegedOutcome.Confirmed, run())
        assertEquals(1, viaRootCalls)
    }

    @Test
    fun `root false or exception fails`() {
        assertEquals(PrivilegedOutcome.Failed, run(viaRoot = { false }))
        assertEquals(PrivilegedOutcome.Failed, run(viaRoot = { error("boom") }))
    }
}
