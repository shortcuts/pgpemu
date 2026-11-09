package com.pgpemu.companion.pairing

import androidx.lifecycle.ViewModel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import javax.inject.Inject

data class PairingAssistantState(
    val serviceEnabled: Boolean,
    val privilegedEnabled: Boolean,
)

@HiltViewModel
class PairingAssistantViewModel
    @Inject
    constructor(
        private val store: PairingStore,
        private val service: PairingServiceAccess,
        private val privileged: PrivilegedPairingAccess,
    ) : ViewModel() {
        private val _state = MutableStateFlow(read())
        val state: StateFlow<PairingAssistantState> = _state

        fun refresh() {
            _state.value = read()
        }

        fun setPrivileged(enabled: Boolean) {
            if (enabled) privileged.requestPermission(::apply) else apply(false)
        }

        private fun apply(enabled: Boolean) {
            store.privilegedEnabled = enabled
            privileged.setReceiverEnabled(enabled)
            refresh()
        }

        private fun read() = PairingAssistantState(service.isEnabled(), store.privilegedEnabled)
    }
