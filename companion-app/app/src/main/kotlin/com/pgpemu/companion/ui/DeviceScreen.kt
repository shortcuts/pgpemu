package com.pgpemu.companion.ui

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.StringRes
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
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.pgpemu.companion.BuildConfig
import com.pgpemu.companion.R
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.DeviceInfo
import com.pgpemu.companion.ble.ScannedDevice
import com.pgpemu.companion.ble.StatusCode
import com.pgpemu.companion.ui.theme.LocalPgpColors
import com.pgpemu.companion.update.UpdateNotice
import com.pgpemu.companion.update.UpdateViewModel
import com.pgpemu.companion.update.updateNotice

// API 31+ (Android 12+) uses the dedicated BLUETOOTH_SCAN/CONNECT runtime permissions.
// Below that, BLE scanning is gated on location instead (no BLUETOOTH_CONNECT equivalent
// exists pre-31 — BLUETOOTH/BLUETOOTH_ADMIN in the manifest are install-time-granted there).
private val BLE_PERMISSIONS =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    } else {
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }

@Composable
fun DeviceScreen(
    viewModel: DeviceViewModel = hiltViewModel(),
    updateViewModel: UpdateViewModel = hiltViewModel(),
) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val latestTag by updateViewModel.latestTag.collectAsStateWithLifecycle()
    val notice = updateNotice(latestTag, BuildConfig.VERSION_NAME, uiState.deviceInfo?.firmwareVersion)
    val colors = LocalPgpColors.current
    val context = LocalContext.current
    var blocker by remember { mutableStateOf<ScanBlocker?>(null) }

    fun granted() = BLE_PERMISSIONS.all { ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED }

    fun adapterOn() = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter?.isEnabled == true

    // Rationale is false both before the first ask and after "don't ask again"; callers only use it after a denial.
    fun canAskAgain(): Boolean {
        var c = context
        while (c is ContextWrapper && c !is Activity) c = c.baseContext
        val activity = c as? Activity ?: return true
        return BLE_PERMISSIONS.any { ActivityCompat.shouldShowRequestPermissionRationale(activity, it) }
    }

    fun requestScan() {
        blocker = scanBlocker(granted(), true, adapterOn())
        if (blocker == null) viewModel.startScan()
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            blocker = scanBlocker(granted(), canAskAgain(), adapterOn())
            if (blocker == null) viewModel.startScan()
        }

    val enableBluetoothLauncher =
        rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            if (adapterOn()) requestScan()
        }

    LifecycleStartEffect(viewModel) {
        viewModel.setForeground(true)
        onStopOrDispose { viewModel.setForeground(false) }
    }

    // Result ignored: the repository re-reads its associations when it needs one.
    val associationLauncher = rememberLauncherForActivityResult(ActivityResultContracts.StartIntentSenderForResult()) {}
    LaunchedEffect(Unit) {
        viewModel.associationRequests.collect { associationLauncher.launch(IntentSenderRequest.Builder(it).build()) }
    }

    val snackbarHost = remember { SnackbarHostState() }
    val messageText = uiState.message?.let { messageText(it) }
    LaunchedEffect(uiState.message) {
        messageText?.let {
            snackbarHost.showSnackbar(it)
            viewModel.consumeMessage()
        }
    }

    Box(modifier = Modifier.fillMaxSize().background(colors.bg).safeDrawingPadding()) {
        Column(modifier = Modifier.fillMaxSize()) {
            TopBar(connectionState = uiState.connectionState)

            notice?.let {
                Box(modifier = Modifier.padding(start = 16.dp, top = 16.dp, end = 16.dp)) {
                    UpdateNoticeCard(
                        notice = it,
                        onOpen = {
                            context.startActivity(
                                Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/shortcuts/pgpemu/releases/tag/${it.tag}")),
                            )
                        },
                        onDismiss = updateViewModel::dismiss,
                    )
                }
            }

            if (uiState.connectionState is ConnectionState.Ready && uiState.deviceInfo?.provisioning == true) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    ProvisioningSection(isBusy = uiState.isBusy, onImport = viewModel::importSecrets)
                    DisconnectRow(onDisconnect = viewModel::disconnect)
                }
            } else if (uiState.connectionState is ConnectionState.Ready) {
                LazyColumn(
                    contentPadding = PaddingValues(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    item { StatusSection(uiState.status) }
                    item { DeviceInfoSection(uiState.deviceInfo, uiState.firmwareOutdated) }
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
                    if (uiState.otaSupported) {
                        item {
                            OtaSection(progress = uiState.otaProgress, isBusy = uiState.isBusy, onPicked = viewModel::requestFirmwareUpdate)
                        }
                    }
                    item {
                        DangerZoneSection(
                            onForgetPairings = viewModel::requestForgetPairings,
                            onResetSecrets = viewModel::requestResetSecrets,
                            onRestart = viewModel::requestRestart,
                        )
                    }
                    item { DisconnectRow(onDisconnect = viewModel::disconnect) }
                }
            } else {
                ConnectPanel(
                    connectionState = uiState.connectionState,
                    errorMessage = (uiState.connectionState as? ConnectionState.Disconnected)?.reason?.let { disconnectText(it) },
                    blocker = blocker,
                    devices = uiState.discoveredDevices,
                    hasScanned = uiState.hasScanned,
                    onScan = {
                        if (granted()) requestScan() else permissionLauncher.launch(BLE_PERMISSIONS)
                    },
                    onEnableBluetooth = { enableBluetoothLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)) },
                    onOpenSettings = {
                        context.startActivity(
                            Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", context.packageName, null)),
                        )
                    },
                    onConnect = viewModel::connectTo,
                )
            }
        }
        SnackbarHost(snackbarHost, modifier = Modifier.align(Alignment.BottomCenter))
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
private fun messageText(message: UiMessage): String =
    when (message) {
        is UiMessage.DeviceStatus ->
            stringResource(
                when (message.code.toByte()) {
                    StatusCode.ERR_UNKNOWN_OPCODE -> R.string.msg_status_unknown_opcode
                    StatusCode.ERR_MALFORMED_PAYLOAD -> R.string.msg_status_malformed
                    StatusCode.ERR_NOT_BONDED -> R.string.msg_status_not_bonded
                    StatusCode.ERR_BUSY -> R.string.msg_status_busy
                    StatusCode.ERR_INTERNAL -> R.string.msg_status_internal
                    StatusCode.ERR_NOT_PROVISIONING -> R.string.msg_status_not_provisioning
                    StatusCode.ERR_OTA_NOT_ARMED -> R.string.msg_status_ota_not_armed
                    StatusCode.ERR_OTA_STATE -> R.string.msg_status_ota_state
                    else -> R.string.msg_status_other
                },
                message.code,
            )
        is UiMessage.CommandFailed -> stringResource(R.string.msg_command_failed, message.detail ?: stringResource(R.string.dash))
        UiMessage.UnexpectedReply -> stringResource(R.string.msg_unexpected_reply)
        UiMessage.FirmwareOutdated -> stringResource(R.string.msg_firmware_outdated)
        is UiMessage.InvalidSecretsFile ->
            stringResource(
                R.string.msg_invalid_secrets_file,
                message.detail ?: stringResource(R.string.dash),
            )
        UiMessage.SecretsProvisioned -> stringResource(R.string.msg_secrets_provisioned)
        is UiMessage.InvalidFirmwareFile ->
            stringResource(R.string.msg_invalid_firmware_file, message.detail ?: stringResource(R.string.dash))
        UiMessage.FirmwareInstalled -> stringResource(R.string.msg_firmware_installed)
    }

// GATT 8 = supervision timeout, the usual out-of-range drop; 0 = clean local disconnect.
@Composable
private fun disconnectText(reason: Int): String? =
    when (reason) {
        0 -> null
        8 -> stringResource(R.string.msg_disconnected_lost, reason)
        else -> stringResource(R.string.msg_disconnected, reason)
    }

@Composable
private fun TopBar(connectionState: ConnectionState) {
    val colors = LocalPgpColors.current
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .border(width = 1.dp, color = colors.border)
                .padding(horizontal = 16.dp, vertical = 14.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = stringResource(R.string.device_title), color = colors.text, fontWeight = FontWeight.SemiBold, fontSize = 16.sp)
        StatusPill(connectionState)
    }
}

@Composable
private fun StatusPill(connectionState: ConnectionState) {
    val colors = LocalPgpColors.current
    val (labelRes, dotColor) =
        when (connectionState) {
            is ConnectionState.Ready -> R.string.state_ready to colors.accent
            is ConnectionState.Scanning -> R.string.state_scanning to colors.warn
            is ConnectionState.Connecting -> R.string.state_connecting to colors.warn
            is ConnectionState.Bonding -> R.string.state_bonding to colors.warn
            is ConnectionState.DiscoveringServices -> R.string.state_discovering to colors.warn
            is ConnectionState.Error -> R.string.state_error to colors.danger
            is ConnectionState.Disconnected -> R.string.state_disconnected to colors.muted
            is ConnectionState.Idle -> R.string.state_idle to colors.muted
        }
    val dimColor = dotColor.copy(alpha = 0.16f)
    Row(
        modifier =
            Modifier
                .background(color = dimColor, shape = RoundedCornerShape(99.dp))
                .padding(horizontal = 10.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(modifier = Modifier.size(6.dp).background(color = dotColor, shape = CircleShape))
        Text(text = stringResource(labelRes), color = dotColor, fontSize = 12.sp, fontWeight = FontWeight.Medium)
    }
}

@Composable
private fun ConnectPanel(
    connectionState: ConnectionState,
    errorMessage: String?,
    blocker: ScanBlocker?,
    devices: List<ScannedDevice>,
    hasScanned: Boolean,
    onScan: () -> Unit,
    onEnableBluetooth: () -> Unit,
    onOpenSettings: () -> Unit,
    onConnect: (String) -> Unit,
) {
    val colors = LocalPgpColors.current
    val scanning = connectionState is ConnectionState.Scanning
    val connecting =
        connectionState is ConnectionState.Connecting ||
            connectionState is ConnectionState.Bonding ||
            connectionState is ConnectionState.DiscoveringServices

    Column(
        modifier = Modifier.fillMaxSize().padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        if (connecting) {
            CircularProgressIndicator(color = colors.accent)
            Spacer(modifier = Modifier.height(16.dp))
            Text(text = stringResource(R.string.device_connecting), color = colors.muted, textAlign = TextAlign.Center)
            return@Column
        }

        if (scanning) {
            CircularProgressIndicator(color = colors.accent)
            Spacer(modifier = Modifier.height(16.dp))
            Text(text = stringResource(R.string.device_scanning), color = colors.muted, textAlign = TextAlign.Center)
        } else if (!hasScanned) {
            Text(
                text = stringResource(R.string.device_not_connected),
                color = colors.text,
                fontWeight = FontWeight.SemiBold,
                fontSize = 18.sp,
            )
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                text = stringResource(R.string.device_scan_hint),
                color = colors.muted,
                textAlign = TextAlign.Center,
            )
        } else if (devices.isEmpty()) {
            Text(
                text = stringResource(R.string.device_none_found),
                color = colors.muted,
                textAlign = TextAlign.Center,
            )
        }

        if (devices.isNotEmpty()) {
            Spacer(modifier = Modifier.height(12.dp))
            Text(
                text = pluralStringResource(R.plurals.device_found, devices.size, devices.size),
                color = colors.text,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(modifier = Modifier.height(8.dp))
            LazyColumn(
                modifier = Modifier.weight(1f, fill = false),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                items(devices, key = { it.address }) { device -> DeviceRow(device, onClick = { onConnect(device.address) }) }
            }
        }

        if (!scanning) {
            Spacer(modifier = Modifier.height(20.dp))
            Button(
                onClick = onScan,
                colors = ButtonDefaults.buttonColors(containerColor = colors.accent, contentColor = colors.bg),
                shape = RoundedCornerShape(10.dp),
            ) { Text(if (hasScanned) stringResource(R.string.device_scan_again) else stringResource(R.string.device_scan)) }
        }

        blocker?.let {
            Spacer(modifier = Modifier.height(16.dp))
            Text(
                text =
                    stringResource(
                        if (it ==
                            ScanBlocker.BluetoothOff
                        ) {
                            R.string.device_bluetooth_off
                        } else {
                            R.string.device_permission_required
                        },
                    ),
                color = colors.danger,
                textAlign = TextAlign.Center,
            )
            when (it) {
                ScanBlocker.BluetoothOff ->
                    TextButton(
                        onClick = onEnableBluetooth,
                    ) { Text(stringResource(R.string.device_turn_on_bluetooth)) }
                ScanBlocker.PermissionPermanentlyDenied ->
                    TextButton(
                        onClick = onOpenSettings,
                    ) { Text(stringResource(R.string.device_open_app_settings)) }
                ScanBlocker.PermissionDenied -> Unit
            }
        }
        (errorMessage ?: (connectionState as? ConnectionState.Error)?.reason)?.let {
            Spacer(modifier = Modifier.height(16.dp))
            Text(text = it, color = colors.danger, textAlign = TextAlign.Center)
        }
    }
}

@StringRes
private fun signalLabel(rssi: Int) =
    when {
        rssi >= -60 -> R.string.signal_strong
        rssi >= -75 -> R.string.signal_medium
        else -> R.string.signal_weak
    }

@Composable
private fun DeviceRow(
    device: ScannedDevice,
    onClick: () -> Unit,
) {
    val colors = LocalPgpColors.current
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .heightIn(min = 48.dp)
                .background(colors.surface, RoundedCornerShape(10.dp))
                .border(1.dp, colors.border, RoundedCornerShape(10.dp))
                .clickable(
                    onClickLabel = stringResource(R.string.device_connect_to, device.name, device.address),
                    onClick = onClick,
                ).padding(12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Column {
            Text(text = device.name, color = colors.text, fontWeight = FontWeight.Medium)
            Text(text = device.address, color = colors.muted, fontSize = 12.sp)
        }
        Text(
            text = stringResource(R.string.device_signal, stringResource(signalLabel(device.rssi)), device.rssi),
            color = colors.muted,
            fontSize = 12.sp,
        )
    }
}

@Composable
private fun ProvisioningSection(
    isBusy: Boolean,
    onImport: (String?) -> Unit,
) {
    val context = LocalContext.current
    val picker =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                onImport(runCatching { context.contentResolver.openInputStream(uri)?.use { it.readBytes().decodeToString() } }.getOrNull())
            }
        }
    SectionCard(title = stringResource(R.string.provisioning_title)) {
        Text(text = stringResource(R.string.provisioning_body), color = LocalPgpColors.current.text)
        Spacer(modifier = Modifier.height(12.dp))
        Button(
            onClick = { picker.launch(arrayOf("*/*")) },
            enabled = !isBusy,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(stringResource(R.string.provisioning_import)) }
    }
}

@Composable
private fun UpdateNoticeCard(
    notice: UpdateNotice,
    onOpen: () -> Unit,
    onDismiss: () -> Unit,
) {
    SectionCard(title = stringResource(R.string.update_title, notice.tag)) {
        if (notice.appOutdated) Text(text = stringResource(R.string.update_app), color = LocalPgpColors.current.text)
        if (notice.firmwareOutdated) Text(text = stringResource(R.string.update_firmware), color = LocalPgpColors.current.text)
        Spacer(modifier = Modifier.height(12.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onOpen) { Text(stringResource(R.string.update_view)) }
            TextButton(onClick = onDismiss) { Text(stringResource(R.string.update_dismiss)) }
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
    ) { Text(stringResource(R.string.device_disconnect)) }
}

@Composable
private fun StatusSection(status: StatusState) {
    val dash = stringResource(R.string.dash)
    SectionCard(title = stringResource(R.string.section_status)) {
        LabeledValue(
            stringResource(R.string.status_led),
            status.ledOn?.let { stringResource(if (it) R.string.status_on else R.string.status_off) } ?: dash,
        )
        LabeledValue(
            stringResource(R.string.status_advertising),
            status.advertisingEnabled?.let {
                stringResource(if (it) R.string.status_enabled else R.string.status_disabled)
            } ?: dash,
        )
        LabeledValue(
            stringResource(R.string.status_radio),
            status.advertisingActive?.let {
                stringResource(if (it) R.string.status_radio_active else R.string.status_radio_idle)
            } ?: dash,
        )
        LabeledValue(stringResource(R.string.status_connections), status.activeConnections?.toString() ?: dash)
        LabeledValue(
            stringResource(R.string.status_log_level),
            status.logLevel?.let { stringResource(logLevelName(it)) } ?: dash,
            isLast = true,
        )
    }
}

@Composable
private fun DeviceInfoSection(
    info: DeviceInfo?,
    firmwareOutdated: Boolean,
) {
    val dash = stringResource(R.string.dash)
    SectionCard(title = stringResource(R.string.section_device_info)) {
        if (firmwareOutdated) {
            Text(text = stringResource(R.string.about_update_firmware), color = LocalPgpColors.current.danger)
            return@SectionCard
        }
        LabeledValue(stringResource(R.string.about_firmware), info?.firmwareVersion ?: dash)
        LabeledValue(stringResource(R.string.about_max_connections), info?.maxConnections?.toString() ?: dash)
        LabeledValue(stringResource(R.string.about_uptime), info?.let { formatUptime(it.uptimeSeconds) } ?: dash)
        LabeledValue(
            stringResource(R.string.about_last_reset),
            info?.let { stringResource(resetReasonName(it.resetReason), it.resetReason) } ?: dash,
        )
        LabeledValue(stringResource(R.string.about_free_heap), info?.let { formatKib(it.freeHeap) } ?: dash)
        LabeledValue(stringResource(R.string.about_min_free_heap), info?.let { formatKib(it.minFreeHeap) } ?: dash, isLast = true)
    }
}

private fun formatUptime(seconds: Long) = "${seconds / 3600}h ${seconds / 60 % 60}m ${seconds % 60}s"

private fun formatKib(bytes: Long) = "${bytes / 1024} KiB"

// esp_reset_reason_t: 1 power-on, 3 software, 4 panic, 5-7 watchdogs, 9 brownout.
@StringRes
private fun resetReasonName(reason: Int) =
    when (reason) {
        1 -> R.string.reset_power_on
        3 -> R.string.reset_software
        4 -> R.string.reset_panic
        5, 6, 7 -> R.string.reset_watchdog
        9 -> R.string.reset_brownout
        else -> R.string.reset_unknown
    }

@StringRes
private fun logLevelName(level: Int) =
    when (level) {
        3 -> R.string.log_verbose
        2 -> R.string.log_info
        else -> R.string.log_debug
    }

@Composable
private fun DeviceProfilesSection(
    profiles: List<ProfileState>,
    maxConnections: Int?,
    onToggleAutospin: (Int) -> Unit,
    onToggleAutocatch: (Int) -> Unit,
) {
    val active = profiles.filter { it.connected }.let { conn -> maxConnections?.let { conn.take(it) } ?: conn }
    SectionCard(title = stringResource(R.string.section_profiles)) {
        if (active.isEmpty()) {
            Text(text = stringResource(R.string.device_no_devices_connected), color = LocalPgpColors.current.muted, fontSize = 12.sp)
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
    val dash = stringResource(R.string.dash)
    Column(
        modifier =
            modifier
                .background(color = colors.surface2, shape = RoundedCornerShape(10.dp))
                .border(width = 1.dp, color = colors.border, shape = RoundedCornerShape(10.dp))
                .padding(8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(text = "#${profile.index}", color = colors.muted, fontSize = 11.sp)
        Spacer(modifier = Modifier.height(6.dp))
        FlagDot(label = stringResource(R.string.profile_spin), active = profile.autospin == true, onClick = onToggleAutospin)
        Spacer(modifier = Modifier.height(4.dp))
        FlagDot(label = stringResource(R.string.profile_catch), active = profile.autocatch == true, onClick = onToggleAutocatch)
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text =
                stringResource(
                    R.string.profile_counters,
                    profile.caught ?: dash,
                    profile.fled ?: dash,
                    profile.spin ?: dash,
                ),
            color = colors.muted,
            fontSize = 9.sp,
        )
    }
}

@Composable
private fun FlagDot(
    label: String,
    active: Boolean,
    onClick: () -> Unit,
) {
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
) {
    val colors = LocalPgpColors.current
    SectionCard(title = stringResource(R.string.section_settings)) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
            Text(text = stringResource(R.string.settings_advertising), color = colors.text, modifier = Modifier.weight(1f))
            Switch(
                checked = status.advertisingEnabled == true,
                onCheckedChange = { onToggleAdvertising() },
                colors = SwitchDefaults.colors(checkedTrackColor = colors.accentDim, checkedThumbColor = colors.accent),
            )
        }
        Divider()
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
            Text(text = stringResource(R.string.settings_max_connections), color = colors.text, modifier = Modifier.weight(1f))
            Text(
                text = settings.maxConnections?.toString() ?: stringResource(R.string.dash),
                color = colors.muted,
                fontFamily = FontFamily.Monospace,
                modifier = Modifier.padding(end = 8.dp),
            )
            StepperButton("−") { onSetMaxConnections((settings.maxConnections ?: 1) - 1) }
            Spacer(modifier = Modifier.width(6.dp))
            StepperButton("+") { onSetMaxConnections((settings.maxConnections ?: 1) + 1) }
        }
        Divider()
        TextRow(
            label =
                stringResource(
                    R.string.settings_log_level,
                    status.logLevel?.let { stringResource(logLevelName(it)) } ?: stringResource(R.string.dash),
                ),
            onClick = onCycleLogLevel,
        )
    }
}

@Composable
private fun StepperButton(
    symbol: String,
    onClick: () -> Unit,
) {
    val colors = LocalPgpColors.current
    Box(
        modifier =
            Modifier
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
    SectionCard(title = stringResource(R.string.section_diagnostics)) {
        DiagnosticDump(stringResource(R.string.diag_runtime_stats), diagnostics.runtimeStats, onRefreshStats)
        DiagnosticDump(stringResource(R.string.diag_task_list), diagnostics.taskList, onRefreshTasks)
        DiagnosticDump(stringResource(R.string.diag_client_states), diagnostics.clientStates, onRefreshClientStates)
        Spacer(modifier = Modifier.height(6.dp))
        TextRow(label = stringResource(R.string.diag_disconnect_all), onClick = onDisconnectAll, isLast = true)
    }
}

@Composable
private fun OtaSection(
    progress: Int?,
    isBusy: Boolean,
    onPicked: (ByteArray?) -> Unit,
) {
    val context = LocalContext.current
    val picker =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                onPicked(runCatching { context.contentResolver.openInputStream(uri)?.use { it.readBytes() } }.getOrNull())
            }
        }
    SectionCard(title = stringResource(R.string.section_ota)) {
        Text(text = stringResource(R.string.ota_body), color = LocalPgpColors.current.text)
        Spacer(modifier = Modifier.height(12.dp))
        if (progress != null) {
            Text(text = stringResource(R.string.ota_progress, progress), color = LocalPgpColors.current.muted)
            LinearProgressIndicator(progress = { progress / 100f }, modifier = Modifier.fillMaxWidth().padding(top = 6.dp))
        } else {
            Button(
                onClick = { picker.launch(arrayOf("*/*")) },
                enabled = !isBusy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(stringResource(R.string.ota_pick)) }
        }
    }
}

@Composable
private fun DangerZoneSection(
    onForgetPairings: () -> Unit,
    onResetSecrets: () -> Unit,
    onRestart: () -> Unit,
) {
    val colors = LocalPgpColors.current
    SectionCard(title = stringResource(R.string.section_danger), titleColor = colors.danger) {
        DangerButton(stringResource(R.string.danger_forget_pairings), onForgetPairings)
        Spacer(modifier = Modifier.height(8.dp))
        DangerButton(stringResource(R.string.danger_reset_secrets), onResetSecrets)
        Spacer(modifier = Modifier.height(8.dp))
        DangerButton(stringResource(R.string.danger_restart), onRestart)
    }
}

@Composable
private fun DangerButton(
    label: String,
    onClick: () -> Unit,
) {
    val colors = LocalPgpColors.current
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .background(color = colors.dangerDim, shape = RoundedCornerShape(10.dp))
                .clickableNoRipple(onClick)
                .padding(horizontal = 14.dp, vertical = 12.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = label, color = colors.danger, fontWeight = FontWeight.Medium)
        Text(text = stringResource(R.string.arrow), color = colors.danger)
    }
}

@Composable
private fun SectionCard(
    title: String,
    titleColor: Color? = null,
    content: @Composable ColumnScope.() -> Unit,
) {
    val colors = LocalPgpColors.current
    Column(
        modifier =
            Modifier
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
private fun LabeledValue(
    label: String,
    value: String,
    isLast: Boolean = false,
) {
    val colors = LocalPgpColors.current
    Row(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Text(text = label, color = colors.text, modifier = Modifier.weight(1f))
        Text(text = value, color = colors.muted)
    }
    if (!isLast) Divider()
}

@Composable
private fun TextRow(
    label: String,
    onClick: () -> Unit,
    isLast: Boolean = false,
) {
    val colors = LocalPgpColors.current
    Row(
        modifier = Modifier.fillMaxWidth().clickableNoRipple(onClick).padding(vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = label, color = colors.text)
        Text(text = stringResource(R.string.arrow), color = colors.muted)
    }
    if (!isLast) Divider()
}

@Composable
private fun DiagnosticDump(
    label: String,
    text: String?,
    onRefresh: () -> Unit,
) {
    val colors = LocalPgpColors.current
    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Text(text = label, color = colors.text, modifier = Modifier.weight(1f))
            TextButton(onClick = onRefresh) { Text(stringResource(R.string.diag_refresh), color = colors.accent) }
        }
        SelectionContainer {
            Column(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 140.dp)
                        .background(color = colors.surface2, shape = RoundedCornerShape(8.dp))
                        .verticalScroll(rememberScrollState())
                        .padding(10.dp),
            ) {
                Text(
                    text = text ?: stringResource(R.string.dash),
                    color = colors.muted,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 11.sp,
                )
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
