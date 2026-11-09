package com.pgpemu.companion.pairing

import android.content.Context
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

interface PairingStore {
    /** The user's choice for privileged pairing; the receiver's component state is the effect. */
    var privilegedEnabled: Boolean
}

class SharedPreferencesPairingStore
    @Inject
    constructor(
        @ApplicationContext context: Context,
    ) : PairingStore {
        private val prefs = context.getSharedPreferences("pairing", Context.MODE_PRIVATE)

        override var privilegedEnabled: Boolean
            get() = prefs.getBoolean(KEY_PRIVILEGED, false)
            set(value) = prefs.edit().putBoolean(KEY_PRIVILEGED, value).apply()

        private companion object {
            const val KEY_PRIVILEGED = "privileged_enabled"
        }
    }
