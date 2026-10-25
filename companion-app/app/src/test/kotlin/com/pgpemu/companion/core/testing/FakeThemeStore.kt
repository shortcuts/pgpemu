package com.pgpemu.companion.core.testing

import com.pgpemu.companion.ui.theme.ThemeMode
import com.pgpemu.companion.ui.theme.ThemeStore

class FakeThemeStore(
    override var mode: ThemeMode = ThemeMode.System,
) : ThemeStore
