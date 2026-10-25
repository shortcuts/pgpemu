package com.pgpemu.companion.ui.theme

import android.content.Context
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

interface ThemeStore {
    var mode: ThemeMode
}

class SharedPreferencesThemeStore
    @Inject
    constructor(
        @ApplicationContext context: Context,
    ) : ThemeStore {
        private val prefs = context.getSharedPreferences("theme", Context.MODE_PRIVATE)

        override var mode: ThemeMode
            get() = ThemeMode.entries.firstOrNull { it.name == prefs.getString(KEY_MODE, null) } ?: ThemeMode.System
            set(value) = prefs.edit().putString(KEY_MODE, value.name).apply()

        private companion object {
            const val KEY_MODE = "mode"
        }
    }
