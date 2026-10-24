package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.CrashReport
import com.pgpemu.companion.ble.DeviceInfo
import org.junit.Assert.assertEquals
import org.junit.Test

class CrashReportTextTest {
    @Test
    fun `report text lists the registers in hex and the candidate backtrace`() {
        val report = CrashReport("main", 7L, 0L, 0x42001234L, 0x42005678L, 0x3fc90000L, "abcdef012", listOf(0x42000100L, 0x4037c000L))
        val info = DeviceInfo(9, 4, 4, 5L, 0L, 0L, "1.2.3", false)

        assertEquals(
            """
            app: 0.9
            firmware: 1.2.3
            last restart reason: 4
            task: main
            mcause: 0x00000007
            mtval: 0x00000000
            pc: 0x42001234
            ra: 0x42005678
            sp: 0x3fc90000
            build: abcdef012
            candidate backtrace: 0x42000100 0x4037c000
            """.trimIndent(),
            formatCrashReport(report, info, "0.9"),
        )
    }
}
