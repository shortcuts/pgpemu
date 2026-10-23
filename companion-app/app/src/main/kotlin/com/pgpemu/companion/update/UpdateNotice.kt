package com.pgpemu.companion.update

data class UpdateNotice(
    val tag: String,
    val appOutdated: Boolean,
    val firmwareOutdated: Boolean,
)

private fun parts(version: String): List<Int>? =
    version
        .removePrefix("v")
        .substringBefore("-")
        .split(".")
        .map { it.toIntOrNull() ?: return null }
        .takeIf { it.size == 3 }

// Alpha suffixes are stripped so `1.0.1-alpha.5` counts as `1.0.1`; any parse failure fails closed.
fun isNewerVersion(
    latest: String,
    current: String,
): Boolean {
    val l = parts(latest) ?: return false
    val c = parts(current) ?: return false
    for (i in 0..2) if (l[i] != c[i]) return l[i] > c[i]
    return false
}

fun updateNotice(
    latestTag: String?,
    appVersion: String,
    firmwareVersion: String?,
): UpdateNotice? {
    if (latestTag == null) return null
    val app = isNewerVersion(latestTag, appVersion)
    val fw = firmwareVersion != null && isNewerVersion(latestTag, firmwareVersion)
    return if (app || fw) UpdateNotice(latestTag, app, fw) else null
}
