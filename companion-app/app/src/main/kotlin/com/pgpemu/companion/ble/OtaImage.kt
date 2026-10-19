package com.pgpemu.companion.ble

/** Size of one OTA slot (partitions.csv); a bigger image cannot be written. */
const val OTA_MAX_IMAGE_SIZE = 1792 * 1024

private const val IMAGE_MAGIC = 0xE9
private const val CHIP_ID_OFFSET = 12
private const val CHIP_ID_ESP32C3 = 5
private const val APP_DESC_OFFSET = 32
private const val APP_DESC_MAGIC = 0xABCD5432L
private const val PROJECT_NAME_OFFSET = APP_DESC_OFFSET + 48
private const val PROJECT_NAME_SIZE = 32
private const val PROJECT_NAME = "pgpemu"

/** A firmware image that passed the same header checks the device repeats; they only give a faster error. */
class OtaImage(
    val bytes: ByteArray,
)

/** Checks the ESP image header, the chip and the app descriptor's project name of a pgpemu.bin. */
fun parseOtaImage(bytes: ByteArray): Result<OtaImage> =
    runCatching {
        if (bytes.size > OTA_MAX_IMAGE_SIZE) throw DecodeError("image: larger than the ${OTA_MAX_IMAGE_SIZE / 1024} KiB slot")
        if (bytes.size < PROJECT_NAME_OFFSET + PROJECT_NAME_SIZE) throw DecodeError("image: too short")

        fun u(offset: Int) = bytes[offset].toLong() and 0xFF
        if (u(0).toInt() != IMAGE_MAGIC) throw DecodeError("image: not an ESP image")
        if ((u(CHIP_ID_OFFSET) or (u(CHIP_ID_OFFSET + 1) shl 8)).toInt() != CHIP_ID_ESP32C3) {
            throw DecodeError("image: built for another chip")
        }
        val magic = (0 until 4).sumOf { u(APP_DESC_OFFSET + it) shl (8 * it) }
        val name = String(bytes, PROJECT_NAME_OFFSET, PROJECT_NAME_SIZE, Charsets.US_ASCII).substringBefore('\u0000')
        if (magic != APP_DESC_MAGIC || name != PROJECT_NAME) throw DecodeError("image: not a pgpemu build")
        OtaImage(bytes)
    }
