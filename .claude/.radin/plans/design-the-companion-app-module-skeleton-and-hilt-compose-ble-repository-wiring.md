# Plan: Design the Companion App module skeleton and Hilt/Compose/BLE-repository wiring

Source task: `.claude/.radin/backlog/tasks/design-the-companion-app-module-skeleton-and-hilt-compose-ble-repository-wiring.md`

Builds on the shell already committed at 93a4bba: `companion-app/` (Gradle root + `:app`,
package `com.pgpemu.companion`) with `ble/` (`ConnectionState`, `ResponseFrame`,
`StatusCode`, `Opcode`, `BleControlRepository`, `NordicBleControlRepository`) already in
place. This plan adds the pieces the prior task explicitly deferred: Hilt, one Compose
screen, one `DeviceViewModel`, and the `core/testing` fake convention.

## Decisions this plan settles

| Decision | Answer | Basis |
|---|---|---|
| Hilt version | `2.57.1` | Already pinned (not yet applied) in `.claude/.radin/plans/decide-android-ble-library-and-companion-app-connection-manager-architecture.md`'s version table, reserved there specifically for this task. Reused as-is for cross-plan consistency, even though Dagger's current documented stable at time of writing this plan is 2.60.1 — introducing a second, later version here would make the two plans disagree over one feature for no functional benefit. |
| Annotation processor: kapt vs KSP | `kapt` (`org.jetbrains.kotlin.kapt`, version = the existing `kotlin` catalog ref, `2.3.20`) | KSP is versioned as its own release train independent of the `kotlin` catalog entry (confirmed via GitHub API: latest tags are bare `2.3.11`, `2.3.10`, …, not tied 1:1 to `2.3.20`), so picking a KSP version correct for Kotlin `2.3.20` isn't verifiable from here without a live build. `kapt`'s plugin ships inside the Kotlin Gradle plugin itself and takes the *same* `kotlin` version already pinned in this catalog — zero new version-compatibility risk. Slower annotation processing is an acceptable tradeoff for a single-module app with one `@Module`/one `@HiltViewModel`; revisit KSP if compile time ever becomes a real problem. |
| Application/Activity entry point | Add minimal `CompanionApplication` (`@HiltAndroidApp`) and `MainActivity` (`@AndroidEntryPoint`) | Not named in ticket 09's resolved answer (which only lists `ble/`, `ui/`, `di/`), but Hilt injection has no effect without a `@HiltAndroidApp` `Application` and at least one `@AndroidEntryPoint` component, and the Compose screen has nowhere to render without an `Activity`. Omitting these would leave "Hilt/Compose … wiring" incomplete by definition. Kept intentionally minimal — no navigation, no multiple activities. |
| `hiltViewModel()` Compose artifact | `androidx.hilt:hilt-lifecycle-viewmodel-compose:1.4.0` (not `androidx.hilt:hilt-navigation-compose`) | This app has no Navigation Compose destinations (one screen, no `NavHost`). The `-lifecycle-viewmodel-compose` artifact carries `hiltViewModel()` without pulling in `androidx.navigation` transitively — the lighter, more accurate dependency for a single-screen app. |
| `FakeBleControlRepository` source set | `app/src/test/kotlin/...` (JVM unit-test source set, not `androidTest`) | `BleControlRepository`'s surface (`StateFlow`, `Result`, `ByteArray`) has no Android-framework dependency, so a JVM unit test (`test`) is sufficient and far faster than an instrumented (`androidTest`) one. `NordicBleControlRepository` (the real impl, which does need a device/emulator) stays untested at this layer — that's this feature's separate, not-yet-executed "broader testing strategy" backlog task, out of scope here. |
| Does the fake ship with a test using it | Yes — one `DeviceViewModelTest` | Ponytail's "lazy code without its check" rule: a fake with zero callers is exactly the kind of unused scaffolding this repo's guidelines reject. One minimal test both proves the fake works and gives `DeviceViewModel`'s state-mapping logic its required check. |

## Scope boundary

This plan adds Hilt + one screen + one ViewModel + the testing-fake convention only.
It does **not** add:
- Any UI for the ~15 UART/opcode commands from ticket 08's full layout (Status cards,
  Device Profiles, Settings, Diagnostics, Danger zone) — that's a later task's screen
  build-out. The screen this plan adds shows **only** connection state and a single
  Connect/Disconnect action; everything else is a static "screen content coming soon"
  placeholder text.
- `sendCommand` usage anywhere (no opcode is invoked from the UI yet).
- Runtime Bluetooth permission request flow (`BLUETOOTH_SCAN`/`BLUETOOTH_CONNECT` are
  already declared in the manifest per the prior task; requesting them at runtime is
  screen-build-out work, not skeleton wiring).
- `NordicBleControlRepository`'s own test coverage, or the broader testing strategy
  (separate backlog task `decide-the-companion-app-s-broader-testing-strategy`).
- Any change to `ConnectionState`, `ResponseFrame`, `Opcode`, or `BleControlRepository`'s
  method signatures.

## Files to change

All paths relative to `/Users/k/Documents/pgpemu`.

### 1. `companion-app/gradle/libs.versions.toml` — add these entries only

```toml
[versions]
# ...existing entries unchanged...
hilt = "2.57.1"
androidxHilt = "1.4.0"
junit = "4.13.2"

[libraries]
# ...existing entries unchanged...
hilt-android = { group = "com.google.dagger", name = "hilt-android", version.ref = "hilt" }
hilt-android-compiler = { group = "com.google.dagger", name = "hilt-android-compiler", version.ref = "hilt" }
androidx-hilt-compiler = { group = "androidx.hilt", name = "hilt-compiler", version.ref = "androidxHilt" }
androidx-hilt-lifecycle-viewmodel-compose = { group = "androidx.hilt", name = "hilt-lifecycle-viewmodel-compose", version.ref = "androidxHilt" }
androidx-lifecycle-viewmodel-ktx = { group = "androidx.lifecycle", name = "lifecycle-viewmodel-ktx", version.ref = "lifecycleRuntimeKtx" }
androidx-lifecycle-runtime-compose = { group = "androidx.lifecycle", name = "lifecycle-runtime-compose", version.ref = "lifecycleRuntimeKtx" }
junit = { group = "junit", name = "junit", version.ref = "junit" }
kotlinx-coroutines-test = { group = "org.jetbrains.kotlinx", name = "kotlinx-coroutines-test", version.ref = "coroutinesAndroid" }

[plugins]
# ...existing entries unchanged...
hilt-android = { id = "com.google.dagger.hilt.android", version.ref = "hilt" }
kotlin-kapt = { id = "org.jetbrains.kotlin.kapt", version.ref = "kotlin" }
```

Reuses the existing `lifecycleRuntimeKtx` (`2.9.3`) and `coroutinesAndroid` (`1.11.0`)
version refs for the two lifecycle/coroutines-test artifacts above — no new version to
track for those. `hilt` and `androidxHilt` are new version keys because they're
independent release trains from anything already pinned.

### 2. `companion-app/build.gradle.kts` (root) — add two plugin aliases

```kotlin
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.compose) apply false
    alias(libs.plugins.hilt.android) apply false
    alias(libs.plugins.kotlin.kapt) apply false
}
```

### 3. `companion-app/app/build.gradle.kts` — apply plugins + add dependencies

```kotlin
plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.hilt.android)
    alias(libs.plugins.kotlin.kapt)
}

// ...existing android { } block unchanged...

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.lifecycle.viewmodel.ktx)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.graphics)
    implementation(libs.compose.ui.tooling.preview)
    implementation(libs.compose.material3)
    implementation(libs.nordic.ble)
    implementation(libs.nordic.ble.ktx)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.hilt.android)
    implementation(libs.androidx.hilt.lifecycle.viewmodel.compose)
    kapt(libs.hilt.android.compiler)
    kapt(libs.androidx.hilt.compiler)
    testImplementation(libs.junit)
    testImplementation(libs.kotlinx.coroutines.test)
}
```

Only the four new lines/blocks are additions; every pre-existing dependency line is
unchanged.

### 4. `companion-app/app/src/main/AndroidManifest.xml` — add application name + activity

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

    <application
        android:name=".CompanionApplication"
        android:allowBackup="true"
        android:label="Companion App"
        android:theme="@android:style/Theme.Material.Light.NoActionBar">

        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

    </application>

</manifest>
```

### 5. `companion-app/app/src/main/kotlin/com/pgpemu/companion/CompanionApplication.kt` (new)

```kotlin
package com.pgpemu.companion

import android.app.Application
import dagger.hilt.android.HiltAndroidApp

@HiltAndroidApp
class CompanionApplication : Application()
```

### 6. `companion-app/app/src/main/kotlin/com/pgpemu/companion/MainActivity.kt` (new)

```kotlin
package com.pgpemu.companion

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import com.pgpemu.companion.ui.DeviceScreen
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface {
                    DeviceScreen()
                }
            }
        }
    }
}
```

### 7. `companion-app/app/src/main/kotlin/com/pgpemu/companion/ble/NordicBleControlRepository.kt` — add constructor injection

Smallest possible edit: change the class header only, so `@Binds` in `BleModule` has an
`@Inject`-constructed target to bind. Everything else in the file (already read in full)
is unchanged.

```kotlin
package com.pgpemu.companion.ble

// ...existing imports unchanged, plus:
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

// ...

class NordicBleControlRepository @Inject constructor(
    @ApplicationContext context: Context,
) : BleControlRepository {
```

(Only the class declaration line changes: `class NordicBleControlRepository(` →
`class NordicBleControlRepository @Inject constructor(`, plus the `@ApplicationContext`
annotation on the `context` parameter and the two new imports. No other line in this
464-line-history file changes.)

### 8. `companion-app/app/src/main/kotlin/com/pgpemu/companion/di/BleModule.kt` (new)

```kotlin
package com.pgpemu.companion.di

import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.NordicBleControlRepository
import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class BleModule {

    @Binds
    @Singleton
    abstract fun bindBleControlRepository(impl: NordicBleControlRepository): BleControlRepository
}
```

Matches locationjoystick's `di/XModule.kt` naming convention per ticket 09's resolved
answer. One binding, one repository — no other bindings belong here yet.

### 9. `companion-app/app/src/main/kotlin/com/pgpemu/companion/ui/DeviceViewModel.kt` (new)

UI state shape: wraps only the repository's `ConnectionState` for now — nothing else is
rendered yet, so nothing else belongs in the state.

```kotlin
package com.pgpemu.companion.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

data class DeviceUiState(
    val connectionState: ConnectionState = ConnectionState.Idle,
)

@HiltViewModel
class DeviceViewModel @Inject constructor(
    private val repository: BleControlRepository,
) : ViewModel() {

    val uiState: StateFlow<DeviceUiState> =
        repository.connectionState
            .map { DeviceUiState(connectionState = it) }
            .stateIn(
                scope = viewModelScope,
                started = SharingStarted.WhileSubscribed(5_000),
                initialValue = DeviceUiState(),
            )

    fun connect() {
        viewModelScope.launch { repository.connect() }
    }

    fun disconnect() {
        viewModelScope.launch { repository.disconnect() }
    }
}
```

`WhileSubscribed(5_000)` matches the standard NowInAndroid/Android-recommended pattern
for a screen-scoped `StateFlow` — keeps collecting briefly through a configuration
change instead of restarting the BLE state stream on every rotation.

### 10. `companion-app/app/src/main/kotlin/com/pgpemu/companion/ui/DeviceScreen.kt` (new)

Renders exactly two things, per the Scope boundary above: the current `ConnectionState`
as text, and a single Connect/Disconnect button. Nothing else — no Status cards, no
Device Profiles, no Settings, no Diagnostics, no Danger zone (those are ticket 08's
full layout, a later task).

```kotlin
package com.pgpemu.companion.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
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

        Text(text = "Screen content coming soon.")
    }
}
```

The `androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel` import is the package
`hilt-lifecycle-viewmodel-compose` (artifact added in file 1) publishes it under —
distinct from `androidx.hilt.navigation.compose.hiltViewModel`, which lives in the
navigation-coupled artifact this plan deliberately does not add. If that import path
turns out wrong when this actually builds, this is the one line most worth
double-checking first (no live build available to confirm the package name from here).

### 11. `companion-app/app/src/test/kotlin/com/pgpemu/companion/core/testing/FakeBleControlRepository.kt` (new)

Exact interface implementation — a `MutableStateFlow` the test controls directly, plus
recording of `connect()`/`disconnect()`/`sendCommand()` calls for assertions.

```kotlin
package com.pgpemu.companion.core.testing

import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.ble.ResponseFrame
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class FakeBleControlRepository : BleControlRepository {
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Idle)
    override val connectionState: StateFlow<ConnectionState> = _connectionState

    fun setConnectionState(state: ConnectionState) {
        _connectionState.value = state
    }

    override suspend fun connect() = Unit

    override suspend fun disconnect() = Unit

    override suspend fun sendCommand(opcode: Int, payload: ByteArray): Result<ResponseFrame> =
        Result.failure(UnsupportedOperationException("not stubbed"))
}
```

`setConnectionState` is the fake's one test-only extension beyond the interface — needed
because a real `BleControlRepository` never lets a caller set its own state directly;
tests need exactly that seam. `connect()`/`disconnect()`/`sendCommand()` have no
call-count tracking or configurable results — this plan's one test never asserts on
those, so tracking them now would be unused flexibility. Add a call counter or a
configurable result field the first time a test actually needs one.

### 12. `companion-app/app/src/test/kotlin/com/pgpemu/companion/ui/DeviceViewModelTest.kt` (new)

One test: proves `DeviceViewModel.uiState` reflects the repository's `ConnectionState`,
using the fake from file 11. This is the "one runnable check" for this plan's only
non-trivial logic (the `ConnectionState` → `DeviceUiState` mapping).

```kotlin
package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.core.testing.FakeBleControlRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class DeviceViewModelTest {

    private val dispatcher = StandardTestDispatcher()

    @Before
    fun setUp() {
        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun `uiState reflects repository connection state`() = runTest {
        val repository = FakeBleControlRepository()
        val viewModel = DeviceViewModel(repository)

        repository.setConnectionState(ConnectionState.Ready)
        dispatcher.scheduler.advanceUntilIdle()

        assertEquals(ConnectionState.Ready, viewModel.uiState.value.connectionState)
    }
}
```

## Order of operations

1. `libs.versions.toml` first — nothing below resolves without the new catalog entries.
2. Root `build.gradle.kts`, then `app/build.gradle.kts` — plugins before the code that
   needs them.
3. `AndroidManifest.xml`, `CompanionApplication.kt`, `MainActivity.kt` — the entry point,
   before the screen it hosts.
4. `NordicBleControlRepository.kt`'s constructor edit, then `di/BleModule.kt` — the
   `@Binds` target must exist and be injectable before the module that binds it.
5. `DeviceViewModel.kt`, then `DeviceScreen.kt` — the screen references the ViewModel.
6. `core/testing/FakeBleControlRepository.kt`, then `DeviceViewModelTest.kt` — the test
   needs the fake.

## Verification

No build/compile verification available (same accepted constraint as the ESP-IDF side).
Verification is structural, re-reading each file against its source of truth:

- `BleModule` binds `BleControlRepository` to `NordicBleControlRepository` as a
  `@Singleton`, nothing else — matches ticket 09's resolved answer exactly.
- `DeviceViewModel` is the only ViewModel added — no per-section split, matches ticket
  09's "one `DeviceViewModel`, not per-section."
- `DeviceScreen` is the only screen added, and renders only connection state + one
  action button + a placeholder line — confirms the Scope boundary held (no Status/
  Device Profiles/Settings/Diagnostics/Danger-zone content from ticket 08 leaked in).
- `FakeBleControlRepository` implements every `BleControlRepository` member with no
  omissions and no extra abstraction (no factory, no interface-of-the-fake).
- Every new `libs.versions.toml` entry is used by at least one Gradle file (no unused
  catalog aliases) — `hilt` used at `2.57.1` everywhere, not `2.60.1`.
- No file under `ble/` changed except the one-line constructor signature on
  `NordicBleControlRepository`.
- No opcode/command UI, no runtime permission request flow, no `sendCommand` caller
  exists anywhere in this diff — confirms the Scope boundary held on the "later task" side.
