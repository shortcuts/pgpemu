package com.pgpemu.companion.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File
import java.lang.reflect.Modifier
import java.util.UUID

class ControlWireVectorsTest {
    private val lines: List<String> =
        File("../../protocol/control_vectors.txt").let { file ->
            check(file.isFile) { "missing ${file.absolutePath}" }
            file.readLines().filter { it.isNotBlank() && !it.startsWith("#") }
        }

    private val errorStatuses =
        mapOf(
            "unknown_opcode" to StatusCode.ERR_UNKNOWN_OPCODE,
            "bad_max_connections" to StatusCode.ERR_MALFORMED_PAYLOAD,
            "rename_known_device_unprintable" to StatusCode.ERR_MALFORMED_PAYLOAD,
            "rename_known_device_missing" to StatusCode.ERR_NOT_FOUND,
            "forget_known_device_connected" to StatusCode.ERR_BUSY,
            "forget_known_device_missing" to StatusCode.ERR_NOT_FOUND,
            "forget_known_device_short" to StatusCode.ERR_MALFORMED_PAYLOAD,
        )

    @Test
    fun `constants match the fixture`() {
        assertEquals(namedValues("opcode"), staticValues(Opcode::class.java))
        assertEquals(namedValues("status"), staticValues(StatusCode::class.java))
        assertEquals(namedValues("event"), staticValues(EventType::class.java))
    }

    @Test
    fun `uuids match the fixture`() {
        val fixture =
            lines.filter { it.startsWith("uuid ") }.associate {
                val (_, key, text) = it.split(" ")
                key to UUID.fromString(text)
            }
        assertEquals(
            mapOf(
                "control_service" to CONTROL_SERVICE_UUID,
                "command_char" to COMMAND_CHARACTERISTIC_UUID,
                "response_char" to RESPONSE_CHARACTERISTIC_UUID,
                "event_char" to EVENT_CHARACTERISTIC_UUID,
            ),
            fixture,
        )
    }

    @Test
    fun `vectors decode with the app decoders`() {
        val vectors = lines.filter { it.startsWith("vector ") }
        assertTrue(vectors.isNotEmpty())
        for (line in vectors) {
            val (_, name, _, requestHex, responseHex) = line.split(" ")
            val request = hex(requestHex)
            val response = hex(responseHex)
            val frame = ResponseFrame(response[0], response[1], response.copyOfRange(2, response.size))

            assertTrue("$name: response does not answer its request", frame.answers(request[0].toInt()))
            assertEquals(name, errorStatuses[name] ?: StatusCode.OK, frame.status)
            when (name) {
                "unknown_opcode",
                "bad_max_connections",
                "rename_known_device",
                "rename_known_device_unprintable",
                "rename_known_device_missing",
                "forget_known_device_connected",
                "forget_known_device_missing",
                "forget_known_device_short",
                -> assertEquals(0, frame.payload.size)
                "known_devices_list" ->
                    assertEquals(
                        listOf(
                            KnownDevice("AA:BB:CC:DD:EE:02", 2L, true, ""),
                            KnownDevice("AA:BB:CC:DD:EE:01", 1L, false, "Pixel"),
                        ),
                        decodeKnownDevices(frame.payload).getOrThrow(),
                    )
                "known_devices_empty" -> assertEquals(emptyList<KnownDevice>(), decodeKnownDevices(frame.payload).getOrThrow())
                "global_settings" ->
                    assertEquals(
                        GlobalSettings(2, true, 0, 1, true),
                        decodeGlobalSettings(frame.payload).getOrThrow(),
                    )
                "led_state" -> assertEquals(true, decodeFlag(frame.payload).getOrThrow())
                "set_log_level" -> assertEquals(3, decodeLogLevel(frame.payload).getOrThrow())
                "set_autospin" -> assertEquals(false, decodeFlag(frame.payload).getOrThrow())
                "client_summary" ->
                    assertEquals(
                        listOf(ClientSummary(true, true, false, 2, 1, 3)) +
                            List(3) { ClientSummary(false, null, null, null, null, null) },
                        decodeClientSummary(frame.payload).getOrThrow(),
                    )
                "device_info" ->
                    assertEquals(
                        DeviceInfo(7, 4, 3, 5L, 0L, 0L, "", false),
                        decodeDeviceInfo(frame.payload).getOrThrow(),
                    )
                "device_info_provisioning" ->
                    assertEquals(
                        DeviceInfo(7, 4, 3, 5L, 0L, 0L, "", true),
                        decodeDeviceInfo(frame.payload).getOrThrow(),
                    )
                else -> fail("no decode expectation for $name")
            }
        }
    }

    @Test
    fun `event frames decode with the app decoder`() {
        val frames = lines.filter { it.startsWith("vector_event ") }
        assertTrue(frames.isNotEmpty())
        val types = namedValues("event")
        for (line in frames) {
            val (_, name, typeName, frameHex) = line.split(" ")
            assertEquals(name, DeviceEvent(0, types.getValue(typeName), 1), decodeEvent(hex(frameHex)).getOrThrow())
        }
        assertTrue(decodeEvent(byteArrayOf(0, 6)).isFailure)
    }

    private fun namedValues(kind: String): Map<String, Int> =
        lines.filter { it.startsWith("$kind ") }.associate {
            val (_, name, value) = it.split(" ")
            name to value.toInt(16)
        }

    private fun staticValues(owner: Class<*>): Map<String, Int> =
        owner.declaredFields
            .filter {
                Modifier.isStatic(it.modifiers) &&
                    !it.name.startsWith("\$") &&
                    // Compose compiler adds a `$stable` field
                    (it.type == Int::class.javaPrimitiveType || it.type == Byte::class.javaPrimitiveType)
            }.associate { it.name to (it.get(null) as Number).toInt() }

    private fun hex(text: String): ByteArray = ByteArray(text.length / 2) { text.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
}
