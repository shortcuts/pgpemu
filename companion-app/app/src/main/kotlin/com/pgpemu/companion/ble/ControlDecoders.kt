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

/** One game phone the firmware has seen; [name] is empty until the user sets one. */
data class KnownDevice(
    /** Bluetooth address, `AA:BB:CC:DD:EE:FF`. */
    val address: String,
    /** Recency counter: a higher value was seen later. The device has no clock. */
    val seq: Long,
    val connected: Boolean,
    val name: String,
)

/** Counters of one phone, both lists in firmware order: caught, fled, spins, bag full, box full, balls empty. */
data class LifetimeStats(
    /** The phone is connected now, so [session] is live. */
    val connected: Boolean,
    val lifetime: List<Long>,
    val session: List<Int>,
)

/** Core dump summary of the last panic; [pcs] are stack words in code ranges, candidates to resolve against the build [elfSha]. */
data class CrashReport(
    val task: String,
    val mcause: Long,
    val mtval: Long,
    val pc: Long,
    val ra: Long,
    val sp: Long,
    val elfSha: String,
    val pcs: List<Long>,
)

const val CLIENT_SUMMARY_RECORD_SIZE = 11
const val EVENT_FRAME_SIZE = 3
private const val KNOWN_DEVICE_RECORD_SIZE = 27
private const val LIFETIME_COUNTERS = 6
private const val LIFETIME_STATS_SIZE = 1 + LIFETIME_COUNTERS * 4 + LIFETIME_COUNTERS * 2
private const val CRASH_REPORT_FIXED_SIZE = 47
private const val KNOWN_DEVICE_NAME_OFFSET = 11
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

/** Layout: see pgp_control.c's CONTROL_OP_GET_DEVICE_INFO case and protocol/control_vectors.txt. */
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

/** One Event characteristic notification; [slot] is the Device Profile index. See protocol/control_vectors.txt. */
data class DeviceEvent(
    val seq: Int,
    val type: Int,
    val slot: Int,
)

fun decodeEvent(p: ByteArray): Result<DeviceEvent> =
    p.require(EVENT_FRAME_SIZE, "event").map {
        DeviceEvent(p[0].toInt() and 0xFF, p[1].toInt() and 0xFF, p[2].toInt() and 0xFF)
    }

/** Layout: see pgp_control.c's CONTROL_OP_GET_CLIENT_SUMMARY case and protocol/control_vectors.txt; one record per firmware connection slot. */
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

/** Layout: see known_devices.h and protocol/control_vectors.txt: count, then one 27-byte record per phone, newest first. */
fun decodeKnownDevices(p: ByteArray): Result<List<KnownDevice>> =
    p.require(1, "known devices").mapCatching {
        val count = p[0].toInt() and 0xFF
        p.require(1 + count * KNOWN_DEVICE_RECORD_SIZE, "known devices").getOrThrow()
        (0 until count).map { i ->
            val base = 1 + i * KNOWN_DEVICE_RECORD_SIZE
            val nameBytes = p.copyOfRange(base + KNOWN_DEVICE_NAME_OFFSET, base + KNOWN_DEVICE_RECORD_SIZE)
            KnownDevice(
                address = (0 until 6).joinToString(":") { "%02X".format(p[base + it]) },
                seq = (0 until 4).sumOf { (p[base + 6 + it].toLong() and 0xFF) shl (8 * it) },
                connected = p[base + 10].toInt() and 0x01 != 0,
                name = String(nameBytes, 0, nameBytes.indexOf(0).let { if (it < 0) nameBytes.size else it }, Charsets.US_ASCII),
            )
        }
    }

/** Layout: see protocol/control_vectors.txt: flags u8, 6 x u32 LE lifetime, 6 x u16 LE session. */
fun decodeLifetimeStats(p: ByteArray): Result<LifetimeStats> =
    p.require(LIFETIME_STATS_SIZE, "lifetime stats").mapCatching {
        LifetimeStats(
            connected = p[0].toInt() and 0x01 != 0,
            lifetime = List(LIFETIME_COUNTERS) { i -> (0 until 4).sumOf { (p[1 + 4 * i + it].toLong() and 0xFF) shl (8 * it) } },
            session =
                List(LIFETIME_COUNTERS) { i ->
                    (p[25 + 2 * i].toInt() and 0xFF) or ((p[26 + 2 * i].toInt() and 0xFF) shl 8)
                },
        )
    }

/** Layout: see crash_report.h and protocol/control_vectors.txt. Null when the device holds no report. */
fun decodeCrashReport(p: ByteArray): Result<CrashReport?> =
    p.require(1, "crash report").mapCatching {
        if (p[0] == 0.toByte()) return@mapCatching null
        p.require(CRASH_REPORT_FIXED_SIZE, "crash report").getOrThrow()
        val count = p[CRASH_REPORT_FIXED_SIZE - 1].toInt() and 0xFF
        if (p.size != CRASH_REPORT_FIXED_SIZE + 4 * count) {
            throw DecodeError("crash report: expected ${CRASH_REPORT_FIXED_SIZE + 4 * count} bytes, got ${p.size}")
        }

        fun u32(offset: Int) = (0 until 4).sumOf { (p[offset + it].toLong() and 0xFF) shl (8 * it) }

        fun text(
            from: Int,
            to: Int,
        ) = String(p, from, p.copyOfRange(from, to).indexOf(0).let { if (it < 0) to - from else it }, Charsets.US_ASCII)
        CrashReport(
            task = text(21, 37),
            mcause = u32(1),
            mtval = u32(5),
            pc = u32(9),
            ra = u32(13),
            sp = u32(17),
            elfSha = text(37, 46),
            pcs = List(count) { u32(CRASH_REPORT_FIXED_SIZE + 4 * it) },
        )
    }

/** The 6 address bytes the known-device opcodes take, from `AA:BB:CC:DD:EE:FF`. */
fun parseAddress(address: String): ByteArray = address.split(":").map { it.toInt(16).toByte() }.toByteArray()

fun encodeRename(
    address: String,
    name: String,
): ByteArray = parseAddress(address) + name.toByteArray(Charsets.US_ASCII)
