package com.pgpemu.companion.monitor

import android.content.Context
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

interface MonitorStore {
    /** Opt-in switch for the Game Monitor; off until the user accepts the disclosure. */
    var enabled: Boolean
}

class SharedPreferencesMonitorStore
    @Inject
    constructor(
        @ApplicationContext context: Context,
    ) : MonitorStore {
        private val prefs = context.getSharedPreferences("game_monitor", Context.MODE_PRIVATE)

        override var enabled: Boolean
            get() = prefs.getBoolean(KEY_ENABLED, false)
            set(value) = prefs.edit().putBoolean(KEY_ENABLED, value).apply()

        private companion object {
            const val KEY_ENABLED = "enabled"
        }
    }
