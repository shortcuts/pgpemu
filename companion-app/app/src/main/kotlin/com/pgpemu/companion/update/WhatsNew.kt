package com.pgpemu.companion.update

import android.content.Context
import androidx.lifecycle.ViewModel
import com.pgpemu.companion.BuildConfig
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import javax.inject.Inject

interface WhatsNewStore {
    /** Version whose notes the user last saw; null on a fresh install. */
    var seenVersion: String?
}

class SharedPreferencesWhatsNewStore
    @Inject
    constructor(
        @ApplicationContext context: Context,
    ) : WhatsNewStore {
        private val prefs = context.getSharedPreferences("whats_new", Context.MODE_PRIVATE)

        override var seenVersion: String?
            get() = prefs.getString(KEY_VERSION, null)
            set(value) = prefs.edit().putString(KEY_VERSION, value).apply()

        private companion object {
            const val KEY_VERSION = "version"
        }
    }

// A fresh install has nothing new to announce.
fun whatsNewDue(
    seen: String?,
    current: String,
): Boolean = seen != null && seen != current

@HiltViewModel
class WhatsNewViewModel
    @Inject
    constructor(
        private val store: WhatsNewStore,
    ) : ViewModel() {
        private val _visible = MutableStateFlow(whatsNewDue(store.seenVersion, BuildConfig.VERSION_NAME))
        val visible: StateFlow<Boolean> = _visible

        init {
            if (store.seenVersion == null) store.seenVersion = BuildConfig.VERSION_NAME
        }

        fun dismiss() {
            store.seenVersion = BuildConfig.VERSION_NAME
            _visible.value = false
        }
    }
