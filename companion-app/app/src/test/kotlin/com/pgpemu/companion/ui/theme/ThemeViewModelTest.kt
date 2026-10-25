package com.pgpemu.companion.ui.theme

import com.pgpemu.companion.core.testing.FakeThemeStore
import org.junit.Assert.assertEquals
import org.junit.Test

class ThemeViewModelTest {
    private val store = FakeThemeStore(ThemeMode.Light)
    private val viewModel = ThemeViewModel(store)

    @Test
    fun `initial mode comes from the store`() {
        assertEquals(ThemeMode.Light, viewModel.mode.value)
    }

    @Test
    fun `cycle walks system, light, dark and persists each step`() {
        viewModel.cycle()
        assertEquals(ThemeMode.Dark, store.mode)
        viewModel.cycle()
        assertEquals(ThemeMode.System, store.mode)
        viewModel.cycle()
        assertEquals(ThemeMode.Light, viewModel.mode.value)
    }

    @Test
    fun `isDark follows the system only in system mode`() {
        assertEquals(true, ThemeMode.System.isDark(systemDark = true))
        assertEquals(false, ThemeMode.System.isDark(systemDark = false))
        assertEquals(false, ThemeMode.Light.isDark(systemDark = true))
        assertEquals(true, ThemeMode.Dark.isDark(systemDark = false))
    }
}
