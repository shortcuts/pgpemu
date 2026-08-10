package com.pgpemu.companion.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfirmActionSheet(
    action: ConfirmAction,
    isBusy: Boolean,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit,
) {
    var typed by remember(action) { mutableStateOf("") }
    ModalBottomSheet(onDismissRequest = onDismiss, sheetState = rememberModalBottomSheetState()) {
        Column(modifier = Modifier.fillMaxWidth().padding(24.dp)) {
            Text(text = action.title)
            Text(text = action.consequence)
            OutlinedTextField(
                value = typed,
                onValueChange = { typed = it },
                label = { Text("Type ${action.confirmWord} to confirm") },
                modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
            )
            Button(
                onClick = onConfirm,
                enabled = typed == action.confirmWord && !isBusy,
                modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
            ) {
                Text(action.title)
            }
        }
    }
}
