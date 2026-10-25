package com.pgpemu.companion.ui.theme

enum class ThemeMode {
    System,
    Light,
    Dark,
    ;

    fun next() = entries[(ordinal + 1) % entries.size]

    fun isDark(systemDark: Boolean) =
        when (this) {
            System -> systemDark
            Light -> false
            Dark -> true
        }
}
