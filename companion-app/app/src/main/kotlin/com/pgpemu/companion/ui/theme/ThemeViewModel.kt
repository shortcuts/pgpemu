package com.pgpemu.companion.ui.theme

import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import javax.inject.Inject

@HiltViewModel
class ThemeViewModel
    @Inject
    constructor(
        private val store: ThemeStore,
    ) : ViewModel() {
        private val _mode = MutableStateFlow(store.mode)
        val mode: StateFlow<ThemeMode> = _mode

        fun cycle() {
            store.mode = _mode.value.next()
            _mode.value = store.mode
        }
    }
