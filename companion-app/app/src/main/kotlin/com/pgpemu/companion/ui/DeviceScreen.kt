package com.pgpemu.companion.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
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

        uiState.errorMessage?.let { Text(text = it) }

        if (uiState.connectionState is ConnectionState.Ready) {
            LazyColumn {
                item { StatusSection(uiState.status) }
                item {
                    DeviceProfilesSection(
                        profiles = uiState.profiles,
                        onToggleAutospin = viewModel::toggleAutospin,
                        onToggleAutocatch = viewModel::toggleAutocatch,
                    )
                }
                item {
                    SettingsSection(
                        status = uiState.status,
                        settings = uiState.settings,
                        onToggleAdvertising = viewModel::toggleAdvertising,
                        onSetMaxConnections = viewModel::setMaxConnections,
                        onCycleLogLevel = viewModel::cycleLogLevel,
                        onSave = viewModel::saveSettings,
                    )
                }
                item {
                    DiagnosticsSection(
                        diagnostics = uiState.diagnostics,
                        onRefreshStats = viewModel::refreshRuntimeStats,
                        onRefreshTasks = viewModel::refreshTaskList,
                        onRefreshClientStates = viewModel::refreshClientStates,
                        onDisconnectAll = viewModel::disconnectAllClients,
                    )
                }
                item {
                    DangerZoneSection(
                        onResetSecrets = viewModel::requestResetSecrets,
                        onRestart = viewModel::requestRestart,
                    )
                }
            }
        }
    }

    uiState.pendingConfirmation?.let { action ->
        ConfirmActionSheet(
            action = action,
            isBusy = uiState.isBusy,
            onConfirm = viewModel::confirmPendingAction,
            onDismiss = viewModel::dismissConfirmation,
        )
    }
}

@Composable
private fun StatusSection(status: StatusState) {
    SectionCard(title = "Status") {
        LabeledValue("LED", status.ledOn?.let { if (it) "On" else "Off" } ?: "—")
        LabeledValue("Advertising", status.advertisingEnabled?.let { if (it) "On" else "Off" } ?: "—")
        LabeledValue("Active connections", status.activeConnections?.toString() ?: "—")
        LabeledValue("Log level", status.logLevel?.let { logLevelName(it) } ?: "—")
    }
}

private fun logLevelName(level: Int) = when (level) { 3 -> "VERBOSE"; 2 -> "INFO"; else -> "DEBUG" }

@Composable
private fun DeviceProfilesSection(
    profiles: List<ProfileState>,
    onToggleAutospin: (Int) -> Unit,
    onToggleAutocatch: (Int) -> Unit,
) {
    SectionCard(title = "Device Profiles") {
        profiles.forEach { profile ->
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
                Text(text = "Profile ${profile.index + 1}", modifier = Modifier.weight(1f))
                FilterChip(selected = profile.autospin == true, onClick = { onToggleAutospin(profile.index) }, label = { Text("Autospin") })
                FilterChip(selected = profile.autocatch == true, onClick = { onToggleAutocatch(profile.index) }, label = { Text("Autocatch") })
            }
        }
    }
}

@Composable
private fun SettingsSection(
    status: StatusState,
    settings: SettingsState,
    onToggleAdvertising: () -> Unit,
    onSetMaxConnections: (Int) -> Unit,
    onCycleLogLevel: () -> Unit,
    onSave: () -> Unit,
) {
    SectionCard(title = "Settings") {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(text = "Advertising", modifier = Modifier.weight(1f))
            Switch(checked = status.advertisingEnabled == true, onCheckedChange = { onToggleAdvertising() })
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(text = "Max connections: ${settings.maxConnections ?: "—"}", modifier = Modifier.weight(1f))
            Button(onClick = { onSetMaxConnections((settings.maxConnections ?: 1) - 1) }) { Text("-") }
            Button(onClick = { onSetMaxConnections((settings.maxConnections ?: 1) + 1) }) { Text("+") }
        }
        Button(onClick = onCycleLogLevel) { Text("Cycle log level") }
        Button(onClick = onSave) { Text("Save settings") }
    }
}

@Composable
private fun DiagnosticsSection(
    diagnostics: DiagnosticsState,
    onRefreshStats: () -> Unit,
    onRefreshTasks: () -> Unit,
    onRefreshClientStates: () -> Unit,
    onDisconnectAll: () -> Unit,
) {
    SectionCard(title = "Diagnostics") {
        DiagnosticDump("Runtime stats", diagnostics.runtimeStats, onRefreshStats)
        DiagnosticDump("Task list", diagnostics.taskList, onRefreshTasks)
        DiagnosticDump("Client states", diagnostics.clientStates, onRefreshClientStates)
        Button(onClick = onDisconnectAll) { Text("Disconnect all") }
    }
}

@Composable
private fun DangerZoneSection(onResetSecrets: () -> Unit, onRestart: () -> Unit) {
    SectionCard(title = "Danger Zone") {
        Button(onClick = onResetSecrets, colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.errorContainer)) { Text("Reset secrets") }
        Button(onClick = onRestart, colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.errorContainer)) { Text("Restart device") }
    }
}

@Composable
private fun SectionCard(title: String, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(text = title, style = MaterialTheme.typography.titleMedium)
            content()
        }
    }
}

@Composable
private fun LabeledValue(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth()) {
        Text(text = label, modifier = Modifier.weight(1f))
        Text(text = value)
    }
}

@Composable
private fun DiagnosticDump(label: String, text: String?, onRefresh: () -> Unit) {
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Text(text = label, modifier = Modifier.weight(1f))
            Button(onClick = onRefresh) { Text("Refresh") }
        }
        Column(modifier = Modifier.fillMaxWidth().heightIn(max = 160.dp).verticalScroll(rememberScrollState())) {
            Text(text = text ?: "—")
        }
    }
}
