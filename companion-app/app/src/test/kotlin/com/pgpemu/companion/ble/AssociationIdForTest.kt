package com.pgpemu.companion.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AssociationIdForTest {
    private val associations = listOf(7 to null, 3 to "AA:BB:CC:DD:EE:FF", 9 to "11:22:33:44:55:66")

    @Test
    fun `exact address returns its association id`() {
        assertEquals(9, associationIdFor(associations, "11:22:33:44:55:66"))
    }

    @Test
    fun `address match ignores letter case`() {
        assertEquals(3, associationIdFor(associations, "aa:bb:cc:dd:ee:ff"))
    }

    @Test
    fun `unknown address returns null`() {
        assertNull(associationIdFor(associations, "00:00:00:00:00:00"))
    }

    @Test
    fun `empty list returns null`() {
        assertNull(associationIdFor(emptyList(), "AA:BB:CC:DD:EE:FF"))
    }
}
