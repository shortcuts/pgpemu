package com.pgpemu.companion.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SecretsCsvTest {
    private fun csv(
        name: String = "\"PKLMGOPLUS\"",
        mac: String = "e4cb0bc35963",
        dkey: String = "ab".repeat(16),
        blob: String = "cd".repeat(256),
    ) = "# comment\nkey,type,encoding,value\npgpsecret,namespace,,\n" +
        "name,data,string,$name\nmac,data,hex2bin,$mac\ndkey,data,hex2bin,$dkey\nblob,data,hex2bin,$blob\n"

    @Test
    fun `parses the firmware csv layout into the 294-byte payload`() {
        val payload = parseSecretsCsv(csv()).getOrThrow().toPayload()
        assertEquals(294, payload.size)
        assertArrayEquals("PKLMGOPLUS".toByteArray() + ByteArray(6), payload.copyOfRange(0, 16))
        assertArrayEquals(byteArrayOf(0xe4.toByte(), 0xcb.toByte(), 0x0b, 0xc3.toByte(), 0x59, 0x63), payload.copyOfRange(16, 22))
        assertEquals(0xab.toByte(), payload[22])
        assertEquals(0xcd.toByte(), payload[293])
    }

    @Test
    fun `rejects what the firmware would reject`() {
        val bad =
            listOf(
                csv(blob = "cd".repeat(255)),
                csv(dkey = "ab".repeat(15)),
                csv(mac = "000000000000"),
                csv(mac = "e4cb0bc3596"),
                csv(mac = "zzcb0bc35963"),
                csv(name = "\"0123456789ABCDEF\""),
                csv(name = "\"\""),
                "key,type,encoding,value\n",
            )
        for (text in bad) {
            assertTrue(parseSecretsCsv(text).exceptionOrNull() is DecodeError)
        }
        assertEquals(294, parseSecretsCsv(csv(name = "\"0123456789ABCDE\"")).getOrThrow().toPayload().size)
    }
}
