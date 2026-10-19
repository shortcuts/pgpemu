package com.pgpemu.companion.ble

/** A response payload shorter than its opcode's layout, e.g. from older firmware or a truncated indication. */
class DecodeError(
    message: String,
) : Exception(message)

data class GlobalSettings(
    val logLevel: Int,
    val advertisingEnabled: Boolean,
    val activeConnections: Int,
    val maxConnections: Int,
    /** Radio state (starting or on); null when the firmware is too old to report it. */
    val advertisingActive: Boolean?,
)

data class ClientSummary(
    val connected: Boolean,
    val autospin: Boolean?,
    val autocatch: Boolean?,
    val caught: Int?,
    val fled: Int?,
    val spin: Int?,
)

/** Firmware facts from GET_DEVICE_INFO; the firmware version string is last, so a new field goes before it with a protocol bump. */
data class DeviceInfo(
    val protocolVersion: Int,
    val maxConnections: Int,
    /** ESP-IDF esp_reset_reason_t value. */
    val resetReason: Int,
    val uptimeSeconds: Long,
    val freeHeap: Long,
    val minFreeHeap: Long,
    val firmwareVersion: String,
    /** The device has no secrets and runs only the Control Service, waiting for PROVISION_SECRETS. */
    val provisioning: Boolean,
)

const val CLIENT_SUMMARY_RECORD_SIZE = 11
private const val GLOBAL_SETTINGS_MIN_SIZE = 4
private const val DEVICE_INFO_MIN_SIZE = 16

private fun ByteArray.require(
    size: Int,
    what: String,
): Result<Unit> =
    if (this.size >= size) {
        Result.success(Unit)
    } else {
        Result.failure(DecodeError("$what: expected $size bytes, got ${this.size}"))
    }

/** Layout: log level, advertising enabled, active connections, max connections, [advertising state]. */
fun decodeGlobalSettings(p: ByteArray): Result<GlobalSettings> =
    p.require(GLOBAL_SETTINGS_MIN_SIZE, "global settings").map {
        GlobalSettings(
            logLevel = p[0].toInt(),
            advertisingEnabled = p[1] == 1.toByte(),
            activeConnections = p[2].toInt() and 0xFF,
            maxConnections = p[3].toInt() and 0xFF,
            advertisingActive = p.getOrNull(4)?.let { it == 1.toByte() || it == 2.toByte() },
        )
    }

/** Layout: see pgp_control.c's CONTROL_OP_GET_DEVICE_INFO case. */
fun decodeDeviceInfo(p: ByteArray): Result<DeviceInfo> =
    p.require(DEVICE_INFO_MIN_SIZE, "device info").map {
        fun u32(offset: Int) = (0 until 4).sumOf { (p[offset + it].toLong() and 0xFF) shl (8 * it) }
        DeviceInfo(
            protocolVersion = p[0].toInt() and 0xFF,
            maxConnections = p[1].toInt() and 0xFF,
            resetReason = p[2].toInt() and 0xFF,
            uptimeSeconds = u32(3),
            freeHeap = u32(7),
            minFreeHeap = u32(11),
            firmwareVersion = String(p, DEVICE_INFO_MIN_SIZE, p.size - DEVICE_INFO_MIN_SIZE, Charsets.UTF_8),
            provisioning = p[15].toInt() and 0x01 != 0,
        )
    }

/** LED state and the autospin/autocatch toggles reply with one flag byte. */
fun decodeFlag(p: ByteArray): Result<Boolean> = p.require(1, "flag").map { p[0] == 1.toByte() }

fun decodeLogLevel(p: ByteArray): Result<Int> = p.require(1, "log level").map { p[0].toInt() }

/** Layout: see pgp_control.c's CONTROL_OP_GET_CLIENT_SUMMARY case; one record per firmware connection slot. */
fun decodeClientSummary(payload: ByteArray): Result<List<ClientSummary>> =
    payload.require(CLIENT_SUMMARY_RECORD_SIZE, "client summary").map {
        (0 until payload.size / CLIENT_SUMMARY_RECORD_SIZE).map { slot ->
            val base = slot * CLIENT_SUMMARY_RECORD_SIZE

            fun u16(offset: Int) =
                (payload[base + offset].toInt() and 0xFF) or
                    ((payload[base + offset + 1].toInt() and 0xFF) shl 8)
            val connId = u16(0)
            val flags = payload[base + 2].toInt() and 0xFF
            val hasSettings = flags and 0x01 != 0
            val hasStats = flags and 0x02 != 0
            val handshakeDone = flags and 0x04 != 0
            ClientSummary(
                connected = connId != 0xFFFF && handshakeDone,
                autospin = if (hasSettings) payload[base + 3] == 1.toByte() else null,
                autocatch = if (hasSettings) payload[base + 4] == 1.toByte() else null,
                caught = if (hasStats) u16(5) else null,
                fled = if (hasStats) u16(7) else null,
                spin = if (hasStats) u16(9) else null,
            )
        }
    }
