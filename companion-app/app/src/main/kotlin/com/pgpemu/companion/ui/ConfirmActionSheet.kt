package com.pgpemu.companion.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.pgpemu.companion.ui.theme.LocalPgpColors

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfirmActionSheet(
    action: ConfirmAction,
    isBusy: Boolean,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit,
) {
    val colors = LocalPgpColors.current
    var typed by remember(action) { mutableStateOf("") }
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(),
        containerColor = colors.surface,
    ) {
        Column(modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 8.dp)) {
            Text(text = action.title, color = colors.text, fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
            Text(
                text = action.consequence,
                color = colors.muted,
                fontSize = 13.sp,
                modifier = Modifier.padding(top = 6.dp, bottom = 16.dp),
            )
            OutlinedTextField(
                value = typed,
                onValueChange = { typed = it },
                label = { Text("Type ${action.confirmWord} to confirm") },
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedBorderColor = colors.accent,
                    unfocusedBorderColor = colors.border,
                    focusedTextColor = colors.text,
                    unfocusedTextColor = colors.text,
                ),
                modifier = Modifier.fillMaxWidth(),
            )
            Row(modifier = Modifier.fillMaxWidth().padding(top = 16.dp, bottom = 16.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedButton(
                    onClick = onDismiss,
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = colors.text),
                    border = BorderStroke(1.dp, colors.border),
                    shape = RoundedCornerShape(10.dp),
                ) { Text("Cancel") }
                Button(
                    onClick = onConfirm,
                    enabled = typed == action.confirmWord && !isBusy,
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.buttonColors(containerColor = colors.danger, contentColor = colors.bg),
                    shape = RoundedCornerShape(10.dp),
                ) { Text(action.title) }
            }
        }
    }
}
