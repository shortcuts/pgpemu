package com.pgpemu.companion.ble

import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.withContext

data class OtaProgress(
    val sent: Int,
    val total: Int,
) {
    val percent: Int get() = if (total == 0) 0 else (sent.toLong() * 100 / total).toInt()
}

/** [status] is the device's error status, or null when the command never got an answer. */
class OtaException(
    val status: Byte?,
    message: String?,
    cause: Throwable? = null,
) : Exception(message, cause)

private fun u32(v: Int) = ByteArray(4) { (v ushr (8 * it)).toByte() }

/**
 * Streams an image over the Control Service: BEGIN, one DATA write per chunk, END. Each write waits for its
 * indication, which is the flow control. The device is told to ABORT when the transfer fails or is cancelled.
 */
class OtaTransfer(
    private val repository: BleControlRepository,
) {
    fun run(image: OtaImage): Flow<OtaProgress> =
        flow {
            val data = image.bytes
            val begin = send(Opcode.OTA_BEGIN, u32(data.size))
            val chunk = begin.payload.takeIf { it.size == 2 }?.let { (it[0].toInt() and 0xFF) or ((it[1].toInt() and 0xFF) shl 8) }
            // A failed BEGIN opened no session, so only later failures abort.
            try {
                if (chunk == null || chunk == 0) throw OtaException(null, "bad chunk size in reply")
                emit(OtaProgress(0, data.size))
                var offset = 0
                while (offset < data.size) {
                    val end = minOf(offset + chunk, data.size)
                    val reply = send(Opcode.OTA_DATA, u32(offset) + data.copyOfRange(offset, end))
                    // The reply names the next offset the device expects: a mismatch means a lost or repeated chunk.
                    val next =
                        reply.payload.takeIf { it.size == 4 }?.let { p ->
                            (0 until 4).sumOf { (p[it].toInt() and 0xFF) shl (8 * it) }
                        }
                    if (next != end) throw OtaException(null, "device expected offset $next, sent up to $end")
                    offset = end
                    emit(OtaProgress(offset, data.size))
                }
                send(Opcode.OTA_END)
            } catch (e: Throwable) {
                withContext(NonCancellable) { repository.sendCommand(Opcode.OTA_ABORT) }
                throw e
            }
        }

    private suspend fun send(
        opcode: Int,
        payload: ByteArray = ByteArray(0),
    ): ResponseFrame {
        val frame = repository.sendCommand(opcode, payload).getOrElse { throw OtaException(null, it.message, it) }
        if (!frame.isOk) throw OtaException(frame.status, "device status ${frame.status}")
        return frame
    }
}
