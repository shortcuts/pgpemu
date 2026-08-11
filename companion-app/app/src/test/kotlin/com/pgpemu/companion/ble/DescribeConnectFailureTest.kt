package com.pgpemu.companion.ble

import no.nordicsemi.android.ble.callback.FailCallback
import org.junit.Assert.assertEquals
import org.junit.Test

class DescribeConnectFailureTest {

    @Test
    fun `known negative reason maps to human text with code in parens`() {
        assertEquals(
            "connection timed out (${FailCallback.REASON_TIMEOUT})",
            describeConnectFailure(FailCallback.REASON_TIMEOUT),
        )
    }

    @Test
    fun `positive reason is parsed as a GATT status code`() {
        assertEquals("GATT INSUF AUTHENTICATION (5)", describeConnectFailure(5))
    }

    @Test
    fun `unknown negative reason falls back to unknown error with code`() {
        assertEquals("unknown error (-12345)", describeConnectFailure(-12345))
    }
}
