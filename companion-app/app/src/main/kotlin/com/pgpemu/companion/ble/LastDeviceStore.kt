package com.pgpemu.companion.ble

import android.content.Context
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

interface LastDeviceStore {
    /** Address of the last device that reached Ready; null before the first connection. */
    var address: String?
}

class SharedPreferencesLastDeviceStore
    @Inject
    constructor(
        @ApplicationContext context: Context,
    ) : LastDeviceStore {
        private val prefs = context.getSharedPreferences("last_device", Context.MODE_PRIVATE)

        override var address: String?
            get() = prefs.getString(KEY_ADDRESS, null)
            set(value) = prefs.edit().putString(KEY_ADDRESS, value).apply()

        private companion object {
            const val KEY_ADDRESS = "address"
        }
    }
