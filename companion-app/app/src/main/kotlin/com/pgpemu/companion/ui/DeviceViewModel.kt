package com.pgpemu.companion.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

data class DeviceUiState(
    val connectionState: ConnectionState = ConnectionState.Idle,
)

@HiltViewModel
class DeviceViewModel @Inject constructor(
    private val repository: BleControlRepository,
) : ViewModel() {

    val uiState: StateFlow<DeviceUiState> =
        repository.connectionState
            .map { DeviceUiState(connectionState = it) }
            .stateIn(
                scope = viewModelScope,
                started = SharingStarted.WhileSubscribed(5_000),
                initialValue = DeviceUiState(),
            )

    fun connect() {
        viewModelScope.launch { repository.connect() }
    }

    fun disconnect() {
        viewModelScope.launch { repository.disconnect() }
    }
}
