package com.pgpemu.companion.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color

/** Non-Material token set — mirrors the approved screen-prototype artifact's CSS custom properties. */
data class PgpColors(
    val bg: Color,
    val surface: Color,
    val surface2: Color,
    val border: Color,
    val text: Color,
    val muted: Color,
    val accent: Color,
    val accentDim: Color,
    val danger: Color,
    val dangerDim: Color,
    val warn: Color,
    val ok: Color,
)

private val DarkPgpColors = PgpColors(
    bg = Color(0xFF121317),
    surface = Color(0xFF1B1D23),
    surface2 = Color(0xFF23262E),
    border = Color(0xFF33363F),
    text = Color(0xFFE9EAEE),
    muted = Color(0xFF8B8E99),
    accent = Color(0xFF45E0C2),
    accentDim = Color(0xFF1F4740),
    danger = Color(0xFFEF5B5B),
    dangerDim = Color(0xFF47201F),
    warn = Color(0xFFE8B74F),
    ok = Color(0xFF5BC46D),
)

private val LightPgpColors = PgpColors(
    bg = Color(0xFFF1F1EE),
    surface = Color(0xFFFFFFFF),
    surface2 = Color(0xFFF6F6F3),
    border = Color(0xFFDCDCD6),
    text = Color(0xFF1A1B1E),
    muted = Color(0xFF6B6E77),
    accent = Color(0xFF0E8F78),
    accentDim = Color(0xFFD8F3EE),
    danger = Color(0xFFC53D3D),
    dangerDim = Color(0xFFF8DEDE),
    warn = Color(0xFF93690E),
    ok = Color(0xFF23843A),
)

val LocalPgpColors = staticCompositionLocalOf { DarkPgpColors }

@Composable
fun PgpTheme(darkTheme: Boolean = isSystemInDarkTheme(), content: @Composable () -> Unit) {
    val colors = if (darkTheme) DarkPgpColors else LightPgpColors
    val materialScheme = if (darkTheme) {
        androidx.compose.material3.darkColorScheme(
            primary = colors.accent,
            onPrimary = colors.bg,
            background = colors.bg,
            onBackground = colors.text,
            surface = colors.surface,
            onSurface = colors.text,
            error = colors.danger,
            onError = colors.bg,
        )
    } else {
        androidx.compose.material3.lightColorScheme(
            primary = colors.accent,
            onPrimary = Color.White,
            background = colors.bg,
            onBackground = colors.text,
            surface = colors.surface,
            onSurface = colors.text,
            error = colors.danger,
            onError = Color.White,
        )
    }
    CompositionLocalProvider(LocalPgpColors provides colors) {
        MaterialTheme(colorScheme = materialScheme, content = content)
    }
}
