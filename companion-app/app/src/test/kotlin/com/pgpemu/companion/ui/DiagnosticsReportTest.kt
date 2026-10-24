package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.DeviceInfo
import org.junit.Assert.assertEquals
import org.junit.Test

class DiagnosticsReportTest {
    private val clientStates =
        """
        [0] conn_id=1 cert_state=2 reconn_key=1 notify=0
          autospin=on catch=1
        state_0_nonce: 00112233
        the_challenge: aabbccdd
        main_nonce: 01020304
        outer_nonce: 05060708
        session_key: deadbeefdeadbeef
        reconnect_challenge: cafebabe
        """.trimIndent()

    @Test
    fun `key material is redacted and other lines stay`() {
        assertEquals(
            """
            [0] conn_id=1 cert_state=2 reconn_key=1 notify=0
              autospin=on catch=1
            state_0_nonce: <redacted>
            the_challenge: <redacted>
            main_nonce: <redacted>
            outer_nonce: <redacted>
            session_key: <redacted>
            reconnect_challenge: <redacted>
            """.trimIndent(),
            redactClientStates(clientStates),
        )
    }

    @Test
    fun `report joins versions and the three dumps`() {
        val info = DeviceInfo(9, 4, 1, 5L, 0L, 0L, "1.2.3", false)
        val report =
            formatDiagnosticsReport(
                DiagnosticsState(runtimeStats = "up 5", taskList = null, clientStates = "session_key: ab"),
                info,
                "0.9",
                "Pixel 8",
                "15",
            )

        assertEquals(
            """
            app: 0.9
            phone: Pixel 8
            android: 15
            firmware: 1.2.3
            last restart reason: 1

            == runtime stats ==
            up 5

            == task list ==
            -

            == client states ==
            session_key: <redacted>
            """.trimIndent(),
            report,
        )
    }
}
