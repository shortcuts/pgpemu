package com.pgpemu.companion.ui

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.pgpemu.companion.R
import com.pgpemu.companion.monitor.GameMonitorViewModel
import com.pgpemu.companion.monitor.GameNotificationService
import com.pgpemu.companion.ui.theme.LocalPgpColors

@Composable
internal fun GameMonitorSection(viewModel: GameMonitorViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val colors = LocalPgpColors.current
    val context = LocalContext.current
    var showDisclosure by rememberSaveable { mutableStateOf(false) }
    val notificationPermission =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) {
            viewModel.refresh()
            if (!viewModel.state.value.accessGranted) context.openNotificationAccessSettings()
        }

    LifecycleStartEffect(Unit) {
        viewModel.refresh()
        onStopOrDispose {}
    }

    SectionCard(title = stringResource(R.string.section_monitor)) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
            Text(text = stringResource(R.string.monitor_switch), color = colors.text, modifier = Modifier.weight(1f))
            Switch(
                checked = state.enabled && state.accessGranted,
                onCheckedChange = { if (it) showDisclosure = true else viewModel.setEnabled(false) },
                colors = SwitchDefaults.colors(checkedTrackColor = colors.accentDim, checkedThumbColor = colors.accent),
            )
        }
        Divider()
        if (state.enabled && !state.accessGranted) {
            Text(
                text = stringResource(R.string.monitor_status_needs_access),
                color = colors.muted,
                modifier = Modifier.padding(vertical = 6.dp),
            )
            Divider()
        }
        TextRow(
            label = stringResource(R.string.monitor_open_access),
            onClick = context::openNotificationAccessSettings,
            isLast = true,
        )
    }

    if (showDisclosure) {
        AlertDialog(
            onDismissRequest = { showDisclosure = false },
            title = { Text(stringResource(R.string.monitor_disclosure_title)) },
            text = { Text(stringResource(R.string.monitor_disclosure_body)) },
            confirmButton = {
                TextButton(
                    onClick = {
                        showDisclosure = false
                        viewModel.setEnabled(true)
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
                            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) !=
                            PackageManager.PERMISSION_GRANTED
                        ) {
                            notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
                        } else if (!viewModel.state.value.accessGranted) {
                            context.openNotificationAccessSettings()
                        }
                    },
                ) { Text(stringResource(R.string.monitor_disclosure_continue), color = colors.accent) }
            },
            dismissButton = {
                TextButton(onClick = { showDisclosure = false }) {
                    Text(stringResource(R.string.confirm_cancel), color = colors.muted)
                }
            },
        )
    }
}

private fun Context.openNotificationAccessSettings() {
    val intent =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Intent(Settings.ACTION_NOTIFICATION_LISTENER_DETAIL_SETTINGS).putExtra(
                Settings.EXTRA_NOTIFICATION_LISTENER_COMPONENT_NAME,
                ComponentName(this, GameNotificationService::class.java).flattenToString(),
            )
        } else {
            Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
        }
    startActivity(intent)
}
