package com.pgpemu.companion.ble

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ResponseFrameTest {
    @Test
    fun `frame answers only the request with its opcode`() {
        val frame = ResponseFrame(StatusCode.OK, Opcode.GET_LED_STATE.toByte(), byteArrayOf())

        assertTrue(frame.answers(Opcode.GET_LED_STATE))
        assertFalse(frame.answers(Opcode.GET_GLOBAL_SETTINGS))
    }
}
