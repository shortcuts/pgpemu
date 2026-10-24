package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.DeviceInfo

private val SECRET_LINE =
    Regex(
        """^(\s*(?:state_0_nonce|the_challenge|main_nonce|outer_nonce|session_key|reconnect_challenge)): [0-9a-fA-F]*$""",
        RegexOption.MULTILINE,
    )

private const val DASH = "-"

fun redactClientStates(text: String): String = SECRET_LINE.replace(text, "$1: <redacted>")

fun formatDiagnosticsReport(
    diagnostics: DiagnosticsState,
    info: DeviceInfo?,
    appVersion: String,
    phoneModel: String,
    androidVersion: String,
): String =
    listOf(
        "app: $appVersion",
        "phone: $phoneModel",
        "android: $androidVersion",
        "firmware: ${info?.firmwareVersion ?: DASH}",
        "last restart reason: ${info?.resetReason ?: DASH}",
        "\n== runtime stats ==\n${diagnostics.runtimeStats ?: DASH}",
        "\n== task list ==\n${diagnostics.taskList ?: DASH}",
        "\n== client states ==\n${diagnostics.clientStates?.let(::redactClientStates) ?: DASH}",
    ).joinToString("\n")
