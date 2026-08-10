package com.pgpemu.companion.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.pgpemu.companion.ble.ConnectionState

@Composable
fun DeviceScreen(viewModel: DeviceViewModel = hiltViewModel()) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()

    Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        Text(text = "Connection state: ${uiState.connectionState}")

        when (uiState.connectionState) {
            is ConnectionState.Ready -> Button(onClick = viewModel::disconnect) { Text("Disconnect") }
            is ConnectionState.Idle,
            is ConnectionState.Disconnected,
            is ConnectionState.Error -> Button(onClick = viewModel::connect) { Text("Connect") }
            else -> Unit // Scanning/Connecting/Bonding/DiscoveringServices: mid-transition, no action to offer
        }

        Text(text = "Screen content coming soon.")
    }
}
