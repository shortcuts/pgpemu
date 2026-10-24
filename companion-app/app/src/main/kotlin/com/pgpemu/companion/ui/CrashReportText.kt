package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.CrashReport
import com.pgpemu.companion.ble.DeviceInfo

private fun hex(value: Long) = "0x%08x".format(value)

/** Plain English on purpose: the text goes to the maintainer, who resolves the addresses against the build named by `build`. */
fun formatCrashReport(
    report: CrashReport,
    info: DeviceInfo,
    appVersion: String,
): String =
    listOf(
        "app: $appVersion",
        "firmware: ${info.firmwareVersion}",
        "last restart reason: ${info.resetReason}",
        "task: ${report.task}",
        "mcause: ${hex(report.mcause)}",
        "mtval: ${hex(report.mtval)}",
        "pc: ${hex(report.pc)}",
        "ra: ${hex(report.ra)}",
        "sp: ${hex(report.sp)}",
        "build: ${report.elfSha}",
        "candidate backtrace: ${report.pcs.joinToString(" ") { hex(it) }}",
    ).joinToString("\n")
