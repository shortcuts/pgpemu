package com.pgpemu.companion.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ControlDecodersTest {
    @Test
    fun `global settings reject short payload, accept exact and longer`() {
        assertTrue(decodeGlobalSettings(byteArrayOf(1, 1, 2)).exceptionOrNull() is DecodeError)
        val exact = decodeGlobalSettings(byteArrayOf(3, 1, 2, 4)).getOrThrow()
        assertEquals(GlobalSettings(3, true, 2, 4, null), exact)
        val longer = decodeGlobalSettings(byteArrayOf(3, 0, 2, 4, 2, 9)).getOrThrow()
        assertEquals(GlobalSettings(3, false, 2, 4, true), longer)
    }

    @Test
    fun `flag and log level reject empty payload, accept exact and longer`() {
        assertTrue(decodeFlag(byteArrayOf()).exceptionOrNull() is DecodeError)
        assertEquals(true, decodeFlag(byteArrayOf(1)).getOrThrow())
        assertEquals(false, decodeFlag(byteArrayOf(0, 1)).getOrThrow())
        assertTrue(decodeLogLevel(byteArrayOf()).exceptionOrNull() is DecodeError)
        assertEquals(3, decodeLogLevel(byteArrayOf(3)).getOrThrow())
        assertEquals(3, decodeLogLevel(byteArrayOf(3, 7)).getOrThrow())
    }

    @Test
    fun `client summary rejects short payload, accepts exact and longer`() {
        assertTrue(decodeClientSummary(ByteArray(CLIENT_SUMMARY_RECORD_SIZE - 1)).exceptionOrNull() is DecodeError)
        val empty = ByteArray(2 * CLIENT_SUMMARY_RECORD_SIZE) { 0xFF.toByte() }
        for (payload in listOf(empty, empty + byteArrayOf(0))) {
            val slots = decodeClientSummary(payload).getOrThrow()
            assertEquals(2, slots.size)
            assertEquals(false, slots[0].connected)
        }
    }

    @Test
    fun `device info rejects short payload, decodes fields and version tail`() {
        val head = byteArrayOf(1, 2, 4, 5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0)
        assertTrue(decodeDeviceInfo(head.copyOf(14)).exceptionOrNull() is DecodeError)
        val info = decodeDeviceInfo(head + "1.0.1".toByteArray()).getOrThrow()
        assertEquals(DeviceInfo(1, 2, 4, 5L, 65536L, 131072L, "1.0.1"), info)
        assertEquals("", decodeDeviceInfo(head).getOrThrow().firmwareVersion)
    }
}
