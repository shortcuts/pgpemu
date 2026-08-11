package com.pgpemu.companion.ui

import android.Manifest
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ui.theme.LocalPgpColors

private val BLE_PERMISSIONS = arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)

@Composable
fun DeviceScreen(viewModel: DeviceViewModel = hiltViewModel()) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val colors = LocalPgpColors.current
    val context = LocalContext.current
    var permissionDenied by remember { mutableStateOf(false) }

    val permissionLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
        if (result.values.all { it }) {
            permissionDenied = false
            viewModel.connect()
        } else {
            permissionDenied = true
        }
    }

    fun requestConnect() {
        val granted = BLE_PERMISSIONS.all { ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED }
        if (granted) {
            permissionDenied = false
            viewModel.connect()
        } else {
            permissionLauncher.launch(BLE_PERMISSIONS)
        }
    }

    Column(modifier = Modifier.fillMaxSize().background(colors.bg).safeDrawingPadding()) {
        TopBar(connectionState = uiState.connectionState)

        if (uiState.connectionState is ConnectionState.Ready) {
            LazyColumn(
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                item { StatusSection(uiState.status) }
                item {
                    DeviceProfilesSection(
                        profiles = uiState.profiles,
                        maxConnections = uiState.settings.maxConnections,
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
                item { DisconnectRow(onDisconnect = viewModel::disconnect) }
            }
        } else {
            ConnectPanel(
                connectionState = uiState.connectionState,
                errorMessage = uiState.errorMessage,
                permissionDenied = permissionDenied,
                onConnect = ::requestConnect,
            )
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
private fun TopBar(connectionState: ConnectionState) {
    val colors = LocalPgpColors.current
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .border(width = 1.dp, color = colors.border)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = "PGP Companion", color = colors.text, fontWeight = FontWeight.SemiBold, fontSize = 16.sp)
        StatusPill(connectionState)
    }
}

@Composable
private fun StatusPill(connectionState: ConnectionState) {
    val colors = LocalPgpColors.current
    val (label, dotColor) = when (connectionState) {
        is ConnectionState.Ready -> "Ready" to colors.accent
        is ConnectionState.Scanning -> "Scanning" to colors.warn
        is ConnectionState.Connecting -> "Connecting" to colors.warn
        is ConnectionState.Bonding -> "Bonding" to colors.warn
        is ConnectionState.DiscoveringServices -> "Discovering" to colors.warn
        is ConnectionState.Error -> "Error" to colors.danger
        is ConnectionState.Disconnected -> "Disconnected" to colors.muted
        is ConnectionState.Idle -> "Idle" to colors.muted
    }
    val dimColor = dotColor.copy(alpha = 0.16f)
    Row(
        modifier = Modifier
            .background(color = dimColor, shape = RoundedCornerShape(99.dp))
            .padding(horizontal = 10.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(modifier = Modifier.size(6.dp).background(color = dotColor, shape = CircleShape))
        Text(text = label, color = dotColor, fontSize = 12.sp, fontWeight = FontWeight.Medium)
    }
}

@Composable
private fun ConnectPanel(
    connectionState: ConnectionState,
    errorMessage: String?,
    permissionDenied: Boolean,
    onConnect: () -> Unit,
) {
    val colors = LocalPgpColors.current
    val transitioning = connectionState is ConnectionState.Scanning ||
        connectionState is ConnectionState.Connecting ||
        connectionState is ConnectionState.Bonding ||
        connectionState is ConnectionState.DiscoveringServices

    Column(
        modifier = Modifier.fillMaxSize().padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        if (transitioning) {
            CircularProgressIndicator(color = colors.accent)
            Spacer(modifier = Modifier.height(16.dp))
            Text(text = "Looking for your Pokémon GO Plus clone…", color = colors.muted, textAlign = TextAlign.Center)
        } else {
            Text(text = "Not connected", color = colors.text, fontWeight = FontWeight.SemiBold, fontSize = 18.sp)
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                text = "Pair with the device over Bluetooth Low Energy to view status and change settings.",
                color = colors.muted,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(bottom = 20.dp),
            )
            Button(
                onClick = onConnect,
                colors = ButtonDefaults.buttonColors(containerColor = colors.accent, contentColor = colors.bg),
                shape = RoundedCornerShape(10.dp),
            ) { Text("Connect") }

            if (permissionDenied) {
                Spacer(modifier = Modifier.height(16.dp))
                Text(text = "Bluetooth permission is required to scan for the device.", color = colors.danger, textAlign = TextAlign.Center)
            }
            (errorMessage ?: (connectionState as? ConnectionState.Error)?.reason)?.let {
                Spacer(modifier = Modifier.height(16.dp))
                Text(text = it, color = colors.danger, textAlign = TextAlign.Center)
            }
        }
    }
}

@Composable
private fun DisconnectRow(onDisconnect: () -> Unit) {
    val colors = LocalPgpColors.current
    OutlinedButton(
        onClick = onDisconnect,
        modifier = Modifier.fillMaxWidth(),
        colors = ButtonDefaults.outlinedButtonColors(contentColor = colors.muted),
        border = androidx.compose.foundation.BorderStroke(1.dp, colors.border),
        shape = RoundedCornerShape(10.dp),
    ) { Text("Disconnect") }
}

@Composable
private fun StatusSection(status: StatusState) {
    SectionCard(title = "Status") {
        LabeledValue("LED", status.ledOn?.let { if (it) "On" else "Off" } ?: "—")
        LabeledValue("Advertising", status.advertisingEnabled?.let { if (it) "Enabled" else "Disabled" } ?: "—")
        LabeledValue("Connections", status.activeConnections?.toString() ?: "—")
        LabeledValue("Log level", status.logLevel?.let { logLevelName(it) } ?: "—", isLast = true)
    }
}

private fun logLevelName(level: Int) = when (level) { 3 -> "Verbose"; 2 -> "Info"; else -> "Debug" }

@Composable
private fun DeviceProfilesSection(
    profiles: List<ProfileState>,
    maxConnections: Int?,
    onToggleAutospin: (Int) -> Unit,
    onToggleAutocatch: (Int) -> Unit,
) {
    val active = profiles.filter { it.connected }.let { conn -> maxConnections?.let { conn.take(it) } ?: conn }
    SectionCard(title = "Device profiles") {
        if (active.isEmpty()) {
            Text(text = "No devices connected", color = LocalPgpColors.current.muted, fontSize = 12.sp)
        } else {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                active.forEach { profile ->
                    ProfileChip(
                        profile = profile,
                        modifier = Modifier.weight(1f),
                        onToggleAutospin = { onToggleAutospin(profile.index) },
                        onToggleAutocatch = { onToggleAutocatch(profile.index) },
                    )
                }
            }
        }
    }
}

@Composable
private fun RowScope.ProfileChip(
    profile: ProfileState,
    modifier: Modifier,
    onToggleAutospin: () -> Unit,
    onToggleAutocatch: () -> Unit,
) {
    val colors = LocalPgpColors.current
    Column(
        modifier = modifier
            .background(color = colors.surface2, shape = RoundedCornerShape(10.dp))
            .border(width = 1.dp, color = colors.border, shape = RoundedCornerShape(10.dp))
            .padding(8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(text = "#${profile.index}", color = colors.muted, fontSize = 11.sp)
        Spacer(modifier = Modifier.height(6.dp))
        FlagDot(label = "SPIN", active = profile.autospin == true, onClick = onToggleAutospin)
        Spacer(modifier = Modifier.height(4.dp))
        FlagDot(label = "CATCH", active = profile.autocatch == true, onClick = onToggleAutocatch)
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = "C:${profile.caught ?: "—"} F:${profile.fled ?: "—"} S:${profile.spin ?: "—"}",
            color = colors.muted,
            fontSize = 9.sp,
        )
    }
}

@Composable
private fun FlagDot(label: String, active: Boolean, onClick: () -> Unit) {
    val colors = LocalPgpColors.current
    val tint = if (active) colors.accent else colors.muted
    Row(
        modifier = Modifier.clickableNoRipple(onClick),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Box(modifier = Modifier.size(6.dp).background(color = tint, shape = CircleShape))
        Text(text = label, color = tint, fontSize = 9.sp)
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
    val colors = LocalPgpColors.current
    SectionCard(title = "Settings") {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
            Text(text = "Advertising", color = colors.text, modifier = Modifier.weight(1f))
            Switch(
                checked = status.advertisingEnabled == true,
                onCheckedChange = { onToggleAdvertising() },
                colors = SwitchDefaults.colors(checkedTrackColor = colors.accentDim, checkedThumbColor = colors.accent),
            )
        }
        Divider()
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
            Text(text = "Max connections", color = colors.text, modifier = Modifier.weight(1f))
            Text(text = "${settings.maxConnections ?: "—"}", color = colors.muted, fontFamily = FontFamily.Monospace, modifier = Modifier.padding(end = 8.dp))
            StepperButton("−") { onSetMaxConnections((settings.maxConnections ?: 1) - 1) }
            Spacer(modifier = Modifier.width(6.dp))
            StepperButton("+") { onSetMaxConnections((settings.maxConnections ?: 1) + 1) }
        }
        Divider()
        TextRow(label = "Log level: ${status.logLevel?.let { logLevelName(it) } ?: "—"}", onClick = onCycleLogLevel)
        Spacer(modifier = Modifier.height(10.dp))
        Button(
            onClick = onSave,
            modifier = Modifier.fillMaxWidth(),
            colors = ButtonDefaults.buttonColors(containerColor = colors.accent, contentColor = colors.bg),
            shape = RoundedCornerShape(10.dp),
        ) { Text("Save to device") }
    }
}

@Composable
private fun StepperButton(symbol: String, onClick: () -> Unit) {
    val colors = LocalPgpColors.current
    Box(
        modifier = Modifier
            .size(32.dp)
            .background(color = colors.surface2, shape = RoundedCornerShape(8.dp))
            .border(width = 1.dp, color = colors.border, shape = RoundedCornerShape(8.dp))
            .clickableNoRipple(onClick),
        contentAlignment = Alignment.Center,
    ) { Text(text = symbol, color = colors.text) }
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
        Spacer(modifier = Modifier.height(6.dp))
        TextRow(label = "Disconnect all clients", onClick = onDisconnectAll, isLast = true)
    }
}

@Composable
private fun DangerZoneSection(onResetSecrets: () -> Unit, onRestart: () -> Unit) {
    val colors = LocalPgpColors.current
    SectionCard(title = "Danger zone", titleColor = colors.danger) {
        DangerButton("Reset session secrets", onResetSecrets)
        Spacer(modifier = Modifier.height(8.dp))
        DangerButton("Restart device", onRestart)
    }
}

@Composable
private fun DangerButton(label: String, onClick: () -> Unit) {
    val colors = LocalPgpColors.current
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(color = colors.dangerDim, shape = RoundedCornerShape(10.dp))
            .clickableNoRipple(onClick)
            .padding(horizontal = 14.dp, vertical = 12.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = label, color = colors.danger, fontWeight = FontWeight.Medium)
        Text(text = "→", color = colors.danger)
    }
}

@Composable
private fun SectionCard(title: String, titleColor: Color? = null, content: @Composable ColumnScope.() -> Unit) {
    val colors = LocalPgpColors.current
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(color = colors.surface, shape = RoundedCornerShape(14.dp))
            .border(width = 1.dp, color = colors.border, shape = RoundedCornerShape(14.dp))
            .padding(14.dp),
    ) {
        Text(
            text = title.uppercase(),
            color = titleColor ?: colors.muted,
            fontSize = 11.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(bottom = 8.dp),
        )
        content()
    }
}

@Composable
private fun LabeledValue(label: String, value: String, isLast: Boolean = false) {
    val colors = LocalPgpColors.current
    Row(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Text(text = label, color = colors.text, modifier = Modifier.weight(1f))
        Text(text = value, color = colors.muted)
    }
    if (!isLast) Divider()
}

@Composable
private fun TextRow(label: String, onClick: () -> Unit, isLast: Boolean = false) {
    val colors = LocalPgpColors.current
    Row(
        modifier = Modifier.fillMaxWidth().clickableNoRipple(onClick).padding(vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = label, color = colors.text)
        Text(text = "→", color = colors.muted)
    }
    if (!isLast) Divider()
}

@Composable
private fun DiagnosticDump(label: String, text: String?, onRefresh: () -> Unit) {
    val colors = LocalPgpColors.current
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Text(text = label, color = colors.text, modifier = Modifier.weight(1f))
            TextButton(onClick = onRefresh) { Text("Refresh", color = colors.accent) }
        }
        SelectionContainer {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 140.dp)
                    .background(color = colors.surface2, shape = RoundedCornerShape(8.dp))
                    .verticalScroll(rememberScrollState())
                    .padding(10.dp),
            ) {
                Text(text = text ?: "—", color = colors.muted, fontFamily = FontFamily.Monospace, fontSize = 11.sp)
            }
        }
    }
}

@Composable
private fun Divider() {
    val colors = LocalPgpColors.current
    Box(modifier = Modifier.fillMaxWidth().height(1.dp).background(colors.border))
}

@Composable
private fun Modifier.clickableNoRipple(onClick: () -> Unit): Modifier {
    val interactionSource = remember { MutableInteractionSource() }
    return this.clickable(interactionSource = interactionSource, indication = null, onClick = onClick)
}
