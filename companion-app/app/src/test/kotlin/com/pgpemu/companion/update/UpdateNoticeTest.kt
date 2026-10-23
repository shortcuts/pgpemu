package com.pgpemu.companion.update

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class UpdateNoticeTest {
    @Test
    fun `isNewerVersion compares three numeric parts`() {
        assertTrue(isNewerVersion("v1.0.2", "1.0.1-alpha.5"))
        assertTrue(isNewerVersion("v1.1.0", "1.0.9"))
        assertTrue(isNewerVersion("v2.0.0", "1.9.9"))
        assertTrue(isNewerVersion("v1.0.10", "1.0.9"))
        assertFalse(isNewerVersion("v1.0.1", "1.0.1"))
        assertFalse(isNewerVersion("v1.0.1", "1.0.2"))
        assertFalse(isNewerVersion("junk", "1.0.1"))
        assertFalse(isNewerVersion("v1.0.2", "junk"))
        assertFalse(isNewerVersion("v1.0", "1.0.1"))
    }

    @Test
    fun `updateNotice flags app firmware both or neither`() {
        assertEquals(UpdateNotice("v1.0.2", true, false), updateNotice("v1.0.2", "1.0.1", "1.0.2"))
        assertEquals(UpdateNotice("v1.0.2", false, true), updateNotice("v1.0.2", "1.0.2", "1.0.1"))
        assertEquals(UpdateNotice("v1.0.2", true, true), updateNotice("v1.0.2", "1.0.1", "1.0.0"))
        assertNull(updateNotice("v1.0.2", "1.0.2", "1.0.2"))
        assertNull(updateNotice(null, "1.0.1", "1.0.0"))
    }

    @Test
    fun `updateNotice ignores firmware when not connected`() {
        assertNull(updateNotice("v1.0.2", "1.0.2", null))
    }
}
