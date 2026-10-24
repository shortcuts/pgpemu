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
        val head = byteArrayOf(1, 2, 4, 5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0)
        assertTrue(decodeDeviceInfo(head.copyOf(15)).exceptionOrNull() is DecodeError)
        val info = decodeDeviceInfo(head + "1.0.1".toByteArray()).getOrThrow()
        assertEquals(DeviceInfo(1, 2, 4, 5L, 65536L, 131072L, "1.0.1", false), info)
        assertEquals("", decodeDeviceInfo(head).getOrThrow().firmwareVersion)
        val setup = head.copyOf().also { it[15] = 1 }
        assertTrue(decodeDeviceInfo(setup + "1.0.1".toByteArray()).getOrThrow().provisioning)
    }

    private fun knownRecord(
        last: Int,
        seq: Int,
        flags: Int,
        name: String,
    ) = byteArrayOf(0xAA.toByte(), 0xBB.toByte(), 0xCC.toByte(), 0xDD.toByte(), 0xEE.toByte(), last.toByte()) +
        byteArrayOf(seq.toByte(), 0, 0, 0, flags.toByte()) + name.toByteArray().copyOf(16)

    @Test
    fun `known devices decode empty, two records and reject a truncated list`() {
        assertEquals(emptyList<KnownDevice>(), decodeKnownDevices(byteArrayOf(0)).getOrThrow())
        assertTrue(decodeKnownDevices(byteArrayOf()).exceptionOrNull() is DecodeError)

        val payload = byteArrayOf(2) + knownRecord(2, 2, 1, "") + knownRecord(1, 1, 0, "Pixel")
        assertEquals(
            listOf(
                KnownDevice("AA:BB:CC:DD:EE:02", 2L, true, ""),
                KnownDevice("AA:BB:CC:DD:EE:01", 1L, false, "Pixel"),
            ),
            decodeKnownDevices(payload).getOrThrow(),
        )
        assertTrue(decodeKnownDevices(payload.copyOf(payload.size - 1)).exceptionOrNull() is DecodeError)
    }

    @Test
    fun `known device name may fill all 16 bytes without a terminator`() {
        val record = knownRecord(1, 1, 0, "").also { "0123456789abcdef".toByteArray().copyInto(it, 11) }
        assertEquals("0123456789abcdef", decodeKnownDevices(byteArrayOf(1) + record).getOrThrow().single().name)
    }

    @Test
    fun `lifetime stats decode a 37 byte frame and reject a 36 byte one`() {
        val frame =
            ByteArray(37).also {
                it[0] = 1
                it[1] = 0x2C // caught 300 = 0x012C
                it[2] = 0x01
                it[20] = 1 // box full u32, high byte
                it[25] = 7 // session caught
                it[36] = 1 // session balls empty, high byte
            }
        assertEquals(
            LifetimeStats(true, listOf(300L, 0L, 0L, 0L, 16777216L, 0L), listOf(7, 0, 0, 0, 0, 256)),
            decodeLifetimeStats(frame).getOrThrow(),
        )
        assertTrue(decodeLifetimeStats(frame.copyOf(36)).exceptionOrNull() is DecodeError)
    }

    @Test
    fun `address and rename payloads are the six address bytes then the name`() {
        assertEquals(listOf(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01), parseAddress("AA:BB:CC:DD:EE:01").map { it.toInt() and 0xFF })
        assertEquals("AABBCCDDEE01" + "41", encodeRename("AA:BB:CC:DD:EE:01", "A").joinToString("") { "%02X".format(it) })
        assertEquals(6, encodeRename("AA:BB:CC:DD:EE:01", "").size)
    }
}
