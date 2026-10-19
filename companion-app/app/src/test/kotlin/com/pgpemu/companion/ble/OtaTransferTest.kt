package com.pgpemu.companion.ble

import com.pgpemu.companion.core.testing.FakeBleControlRepository
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

private fun ok(
    opcode: Int,
    payload: ByteArray = byteArrayOf(),
) = Result.success(ResponseFrame(StatusCode.OK, opcode.toByte(), payload))

private fun err(
    opcode: Int,
    status: Byte,
) = Result.success(ResponseFrame(status, opcode.toByte(), byteArrayOf()))

private fun le32(v: Int) = ByteArray(4) { (v ushr (8 * it)).toByte() }

class OtaTransferTest {
    private val image = OtaImage(otaImageBytes(size = 1000))

    /** Device with chunk size 480 that accepts offsets in order. */
    private fun device(
        repo: FakeBleControlRepository,
        failAtOffset: Int? = null,
    ) {
        repo.responder = { opcode, payload ->
            when (opcode) {
                Opcode.OTA_BEGIN -> ok(opcode, byteArrayOf(0xE0.toByte(), 0x01))
                Opcode.OTA_DATA -> {
                    val offset = (0 until 4).sumOf { (payload[it].toInt() and 0xFF) shl (8 * it) }
                    if (offset == failAtOffset) err(opcode, StatusCode.ERR_INTERNAL) else ok(opcode, le32(offset + payload.size - 4))
                }
                else -> ok(opcode)
            }
        }
    }

    @Test
    fun `sends begin, one data write per chunk, then end`() =
        runTest {
            val repo = FakeBleControlRepository()
            device(repo)
            val progress = OtaTransfer(repo).run(image).toList()

            assertEquals(listOf(0, 48, 96, 100), progress.map { it.percent })
            assertEquals(
                listOf(Opcode.OTA_BEGIN, Opcode.OTA_DATA, Opcode.OTA_DATA, Opcode.OTA_DATA, Opcode.OTA_END),
                repo.sentCommands.map { it.first },
            )
            assertArrayEquals(le32(1000), repo.sentCommands[0].second)
            val data = repo.sentCommands.filter { it.first == Opcode.OTA_DATA }.map { it.second }
            assertEquals(listOf(484, 484, 44), data.map { it.size })
            assertArrayEquals(le32(480), data[1].copyOfRange(0, 4))
            assertArrayEquals(le32(960), data[2].copyOfRange(0, 4))
            assertArrayEquals(image.bytes.copyOfRange(960, 1000), data[2].copyOfRange(4, 44))
        }

    @Test
    fun `a failed chunk sends abort and reports the device status`() =
        runTest {
            val repo = FakeBleControlRepository()
            device(repo, failAtOffset = 480)
            try {
                OtaTransfer(repo).run(image).toList()
                fail("expected OtaException")
            } catch (e: OtaException) {
                assertEquals(StatusCode.ERR_INTERNAL, e.status)
            }
            assertEquals(Opcode.OTA_ABORT, repo.sentCommands.last().first)
            assertTrue(repo.sentCommands.none { it.first == Opcode.OTA_END })
        }

    @Test
    fun `a rejected begin reports not armed and opens nothing to abort`() =
        runTest {
            val repo = FakeBleControlRepository()
            repo.responder = { opcode, _ -> err(opcode, StatusCode.ERR_OTA_NOT_ARMED) }
            try {
                OtaTransfer(repo).run(image).toList()
                fail("expected OtaException")
            } catch (e: OtaException) {
                assertEquals(StatusCode.ERR_OTA_NOT_ARMED, e.status)
            }
            assertEquals(listOf(Opcode.OTA_BEGIN), repo.sentCommands.map { it.first })
        }

    @Test
    fun `an unexpected next offset aborts`() =
        runTest {
            val repo = FakeBleControlRepository()
            repo.responder = { opcode, _ ->
                if (opcode == Opcode.OTA_BEGIN) ok(opcode, byteArrayOf(0xE0.toByte(), 0x01)) else ok(opcode, le32(7))
            }
            val error = runCatching { OtaTransfer(repo).run(image).toList() }.exceptionOrNull()
            assertNotNull(error)
            assertEquals(Opcode.OTA_ABORT, repo.sentCommands.last().first)
        }
}
