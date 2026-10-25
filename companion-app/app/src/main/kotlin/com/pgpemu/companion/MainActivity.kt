package com.pgpemu.companion

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.pgpemu.companion.ui.DeviceScreen
import com.pgpemu.companion.ui.theme.LocalPgpColors
import com.pgpemu.companion.ui.theme.PgpTheme
import com.pgpemu.companion.ui.theme.ThemeViewModel
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    private val themeViewModel: ThemeViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            val mode by themeViewModel.mode.collectAsStateWithLifecycle()
            PgpTheme(darkTheme = mode.isDark(isSystemInDarkTheme())) {
                Surface(modifier = Modifier.fillMaxSize(), color = LocalPgpColors.current.bg) {
                    DeviceScreen()
                }
            }
        }
    }
}
