package com.pgpemu.companion.monitor

import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import javax.inject.Inject

data class GameMonitorState(
    val enabled: Boolean,
    val accessGranted: Boolean,
)

@HiltViewModel
class GameMonitorViewModel
    @Inject
    constructor(
        private val store: MonitorStore,
        private val access: NotificationAccess,
    ) : ViewModel() {
        private val _state = MutableStateFlow(read())
        val state: StateFlow<GameMonitorState> = _state

        fun refresh() {
            _state.value = read()
        }

        fun setEnabled(enabled: Boolean) {
            store.enabled = enabled
            refresh()
        }

        private fun read() = GameMonitorState(store.enabled, access.isGranted())
    }
