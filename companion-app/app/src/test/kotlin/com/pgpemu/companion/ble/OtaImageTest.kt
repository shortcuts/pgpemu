package com.pgpemu.companion.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** The first 112 bytes of an ESP32-C3 pgpemu image: image header, segment header, app descriptor. */
fun otaImageBytes(
    size: Int = 4096,
    magic: Int = 0xE9,
    chip: Int = 5,
    project: String = "pgpemu",
): ByteArray {
    val b = ByteArray(size)
    b[0] = magic.toByte()
    b[12] = chip.toByte()
    val descMagic = byteArrayOf(0x32, 0x54, 0xCD.toByte(), 0xAB.toByte())
    descMagic.copyInto(b, 32)
    project.toByteArray().copyInto(b, 80)
    return b
}

class OtaImageTest {
    @Test
    fun `accepts a pgpemu image for the esp32c3`() {
        assertEquals(4096, parseOtaImage(otaImageBytes()).getOrThrow().bytes.size)
    }

    @Test
    fun `rejects wrong magic, chip, project name and sizes`() {
        val bad =
            mapOf(
                "not an ESP image" to otaImageBytes(magic = 0x00),
                "built for another chip" to otaImageBytes(chip = 9),
                "not a pgpemu build" to otaImageBytes(project = "pgpemu2"),
                "too short" to otaImageBytes(size = 100),
                "larger than" to otaImageBytes(size = OTA_MAX_IMAGE_SIZE + 1),
            )
        for ((reason, bytes) in bad) {
            val error = parseOtaImage(bytes).exceptionOrNull()
            assertTrue("$reason: ${error?.message}", error is DecodeError && error.message!!.contains(reason))
        }
        assertTrue(parseOtaImage(otaImageBytes(size = OTA_MAX_IMAGE_SIZE)).isSuccess)
    }
}
