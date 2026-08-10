package com.pgpemu.companion

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import com.pgpemu.companion.ui.DeviceScreen
import com.pgpemu.companion.ui.theme.LocalPgpColors
import com.pgpemu.companion.ui.theme.PgpTheme
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            PgpTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = LocalPgpColors.current.bg) {
                    DeviceScreen()
                }
            }
        }
    }
}
