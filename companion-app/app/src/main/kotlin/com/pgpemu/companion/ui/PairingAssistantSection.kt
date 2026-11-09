package com.pgpemu.companion.ui

import android.content.Context
import android.content.Intent
import android.provider.Settings
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
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.pgpemu.companion.R
import com.pgpemu.companion.pairing.PairingAssistantViewModel
import com.pgpemu.companion.ui.theme.LocalPgpColors

@Composable
internal fun PairingAssistantSection(viewModel: PairingAssistantViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val colors = LocalPgpColors.current
    val context = LocalContext.current
    var showDisclosure by rememberSaveable { mutableStateOf(false) }

    LifecycleStartEffect(Unit) {
        viewModel.refresh()
        onStopOrDispose {}
    }

    SectionCard(title = stringResource(R.string.section_pairing)) {
        SwitchRow(
            label = stringResource(R.string.pairing_switch),
            checked = state.serviceEnabled,
            onCheckedChange = { if (it) showDisclosure = true else context.openAccessibilitySettings() },
        )
        Divider()
        SwitchRow(
            label = stringResource(R.string.pairing_privileged_switch),
            checked = state.privilegedEnabled,
            onCheckedChange = viewModel::setPrivileged,
        )
        Text(
            text = stringResource(R.string.pairing_privileged_needs_root),
            color = colors.muted,
            modifier = Modifier.padding(vertical = 6.dp),
        )
    }

    if (showDisclosure) {
        AlertDialog(
            onDismissRequest = { showDisclosure = false },
            title = { Text(stringResource(R.string.pairing_disclosure_title)) },
            text = { Text(stringResource(R.string.pairing_disclosure_body)) },
            confirmButton = {
                TextButton(
                    onClick = {
                        showDisclosure = false
                        context.openAccessibilitySettings()
                    },
                ) { Text(stringResource(R.string.pairing_disclosure_continue), color = colors.accent) }
            },
            dismissButton = {
                TextButton(onClick = { showDisclosure = false }) {
                    Text(stringResource(R.string.confirm_cancel), color = colors.muted)
                }
            },
        )
    }
}

@Composable
private fun SwitchRow(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    val colors = LocalPgpColors.current
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Text(text = label, color = colors.text, modifier = Modifier.weight(1f))
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(checkedTrackColor = colors.accentDim, checkedThumbColor = colors.accent),
        )
    }
}

private fun Context.openAccessibilitySettings() = startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS))
