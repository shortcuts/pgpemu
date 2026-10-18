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

const val CLIENT_SUMMARY_SLOTS = 4
const val CLIENT_SUMMARY_RECORD_SIZE = 11
private const val GLOBAL_SETTINGS_MIN_SIZE = 4

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

/** LED state and the autospin/autocatch toggles reply with one flag byte. */
fun decodeFlag(p: ByteArray): Result<Boolean> = p.require(1, "flag").map { p[0] == 1.toByte() }

fun decodeLogLevel(p: ByteArray): Result<Int> = p.require(1, "log level").map { p[0].toInt() }

/** Layout: see pgp_control.c's CONTROL_OP_GET_CLIENT_SUMMARY case. */
fun decodeClientSummary(payload: ByteArray): Result<List<ClientSummary>> =
    payload.require(CLIENT_SUMMARY_SLOTS * CLIENT_SUMMARY_RECORD_SIZE, "client summary").map {
        (0 until CLIENT_SUMMARY_SLOTS).map { slot ->
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
