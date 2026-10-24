package com.pgpemu.companion.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.pgpemu.companion.BuildConfig
import com.pgpemu.companion.R

enum class DeviceTab(
    @StringRes val label: Int,
) {
    Dashboard(R.string.tab_dashboard),
    Settings(R.string.section_settings),
    Diagnostics(R.string.section_diagnostics),
    About(R.string.tab_about),
}

private const val WIKI = "https://pgpemu.shrtcts.fr"

@Composable
internal fun DeviceTabBar(
    selected: DeviceTab,
    onSelect: (DeviceTab) -> Unit,
) {
    NavigationBar {
        DeviceTab.entries.forEach {
            NavigationBarItem(
                selected = it == selected,
                onClick = { onSelect(it) },
                icon = {},
                label = { Text(stringResource(it.label)) },
                alwaysShowLabel = true,
            )
        }
    }
}

@Composable
private fun TabList(content: androidx.compose.foundation.lazy.LazyListScope.() -> Unit) =
    LazyColumn(
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
        content = content,
    )

@Composable
internal fun DashboardTab(
    uiState: DeviceUiState,
    viewModel: DeviceViewModel,
) = TabList {
    val crash = uiState.crashReport
    val info = uiState.deviceInfo
    if (uiState.crashSupported && crash != null && info != null) {
        item { CrashReportSection(crash, info, onDismiss = viewModel::dismissCrashReport) }
    }
    item { StatusSection(uiState.status) }
    item {
        DeviceProfilesSection(
            profiles = uiState.profiles,
            knownDevices = if (uiState.profilePhoneSupported) uiState.knownDevices else emptyList(),
            maxConnections = uiState.settings.maxConnections,
            onToggleAutospin = viewModel::toggleAutospin,
            onCycleCatchMode = viewModel::cycleCatchMode,
        )
    }
    if (uiState.eventsSupported) {
        item { ActivityFeedSection(uiState.activity, onClear = viewModel::clearActivity) }
    }
    item { DisconnectRow(onDisconnect = viewModel::disconnect) }
}

@Composable
internal fun SettingsTab(
    uiState: DeviceUiState,
    viewModel: DeviceViewModel,
) = TabList {
    item {
        SettingsSection(
            status = uiState.status,
            settings = uiState.settings,
            onToggleAdvertising = viewModel::toggleAdvertising,
            onToggleStatusLed = viewModel::toggleStatusLed,
            onSetMaxConnections = viewModel::setMaxConnections,
            onCycleLogLevel = viewModel::cycleLogLevel,
        )
    }
    if (uiState.knownDevicesSupported) {
        item {
            KnownDevicesSection(
                devices = uiState.knownDevices,
                lifetime = uiState.knownLifetime,
                onRename = viewModel::renameKnownDevice,
                onForget = viewModel::requestForgetKnownDevice,
                onResetStats = viewModel::requestResetLifetimeStats,
            )
        }
    }
}

@Composable
internal fun DiagnosticsTab(
    uiState: DeviceUiState,
    viewModel: DeviceViewModel,
) = TabList {
    item {
        DiagnosticsSection(
            diagnostics = uiState.diagnostics,
            info = uiState.deviceInfo,
            onRefreshStats = viewModel::refreshRuntimeStats,
            onRefreshTasks = viewModel::refreshTaskList,
            onRefreshClientStates = viewModel::refreshClientStates,
            onDisconnectAll = viewModel::disconnectAllClients,
        )
    }
    item {
        DangerZoneSection(
            onForgetPairings = viewModel::requestForgetPairings,
            onResetSecrets = viewModel::requestResetSecrets,
            onRestart = viewModel::requestRestart,
        )
    }
}

@Composable
internal fun AboutTab(
    uiState: DeviceUiState,
    viewModel: DeviceViewModel,
) = TabList {
    item { DeviceInfoSection(uiState.deviceInfo, uiState.firmwareOutdated) }
    if (uiState.otaSupported) {
        item { OtaSection(progress = uiState.otaProgress, isBusy = uiState.isBusy, onPicked = viewModel::requestFirmwareUpdate) }
    }
    item { AboutLinksSection() }
}

@Composable
private fun AboutLinksSection() {
    val context = LocalContext.current
    val links =
        listOf(
            R.string.about_link_wiki to WIKI,
            R.string.about_link_troubleshooting to "$WIKI/troubleshooting.html",
            R.string.about_link_privacy to "$WIKI/privacy.html",
            R.string.about_link_acknowledgements to "$WIKI/acknowledgements.html",
            R.string.about_link_issues to "https://github.com/shortcuts/pgpemu/issues",
        )
    SectionCard(title = stringResource(R.string.section_about_links)) {
        LabeledValue(stringResource(R.string.about_app_version), BuildConfig.VERSION_NAME)
        links.forEachIndexed { i, (label, url) ->
            TextRow(stringResource(label), onClick = { context.open(url) }, isLast = i == links.lastIndex)
        }
    }
}

private fun Context.open(url: String) = startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
