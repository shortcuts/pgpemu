package com.pgpemu.companion.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ScanBlockerTest {
    @Test
    fun `granted with adapter on does not block`() {
        assertNull(scanBlocker(granted = true, canAskAgain = true, adapterOn = true))
    }

    @Test
    fun `granted with adapter off asks to turn bluetooth on`() {
        assertEquals(ScanBlocker.BluetoothOff, scanBlocker(granted = true, canAskAgain = true, adapterOn = false))
    }

    @Test
    fun `denied but askable shows the plain denial`() {
        assertEquals(ScanBlocker.PermissionDenied, scanBlocker(granted = false, canAskAgain = true, adapterOn = true))
    }

    @Test
    fun `permanently denied points to app settings even with adapter off`() {
        assertEquals(ScanBlocker.PermissionPermanentlyDenied, scanBlocker(granted = false, canAskAgain = false, adapterOn = false))
    }
}
