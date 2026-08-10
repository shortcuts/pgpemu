# Plan: Decide Android BLE library and Companion App connection-manager architecture

Source task: `.claude/.radin/backlog/tasks/decide-android-ble-library-and-companion-app-connection-manager-architecture.md`
Design already resolved in `.scratch/ble-companion-control/issues/06-android-ble-architecture.md` (blocked by 03, 04, 10 — all resolved). This plan turns that resolved design into the first buildable slice of the Companion App: the Gradle/Android project shell plus the `ble/` package it names. It does **not** add Hilt wiring, Compose screens, or a testing-fake — those belong to backlog tasks 9–11 (see Scope boundary below).

## Decisions this plan settles (not explicit in the entry text, derived from repo conventions)

| Decision | Answer | Basis |
|---|---|---|
| Module location | `companion-app/`, new top-level directory, sibling to `pgpemu-esp32/` | No existing convention names anything else; README/CONTEXT.md are silent on an app directory name; `companion-app` mirrors the domain term "Companion App" from `CONTEXT.md`. `locationjoystick` (referenced in tickets 03/06/09) is a **separate, external prior-art repo** the map's own Explore pass already investigated for conventions to borrow (module/DI/testing scaffold) — it is not a directory that exists in, or needs to be copied into, this repo. |
| Application ID / package root | `com.pgpemu.companion` | Derived from the repo name `pgpemu` (matches `CONTEXT.md`'s own vocabulary) + the domain term "Companion App"; no existing Android package anywhere in the repo to inherit from (repo is firmware-only) and README has no stated app-store identity. |
| Gradle project shape | `companion-app/` is its own standalone Gradle root (own `settings.gradle.kts`, own wrapper) — **not** wired into the repo-root `Makefile` or any ESP-IDF build file | ESP-IDF (`pgpemu-esp32/`) and Gradle are unrelated build systems; AGENTS.md's "never run esp-idf yourself" scopes the Makefile to firmware only. Single-module Android app per ticket 06/09 ⇒ one Gradle module, `:app`. |
| SDK levels | `compileSdk = 36`, `targetSdk = 36`, `minSdk = 31` | Already fixed by ticket 03 ("matching locationjoystick's compileSdk=36/minSdk=31"), reconfirmed by ticket 09. |
| Version set (Gradle/AGP/Kotlin/Compose/Hilt) | See table below | Verified against each project's own current-stable release docs (Aug 2026), not invented. |

### Pinned versions (`companion-app/gradle/libs.versions.toml`)

| Artifact | Version | Note |
|---|---|---|
| Gradle wrapper | `9.1.0` | Minimum Gradle AGP 9.1.x requires. |
| Android Gradle Plugin (`com.android.application`) | `9.1.1` | Current stable 9.x line (Apr 2026). |
| Kotlin (`org.jetbrains.kotlin.android`, `org.jetbrains.kotlin.plugin.compose`) | `2.3.20` | Current stable; Compose compiler ships as a Kotlin compiler plugin (`org.jetbrains.kotlin.plugin.compose`) since Kotlin 2.0 — no separate `compose-compiler` artifact to pin. |
| Compose BOM (`androidx.compose:compose-bom`) | `2026.06.01` | Current stable BOM; pins all `androidx.compose.*` artifact versions transitively. |
| Hilt (`com.google.dagger.hilt.android`, `com.google.dagger:hilt-android`, `com.google.dagger:hilt-android-compiler`) | `2.57.1` | Current stable. Recorded here as the version task 9 should use — not added to `libs.versions.toml` by this task, since an unused plugin/library alias with no caller is dead weight until task 9 actually applies the plugin. |
| Nordic BLE (`no.nordicsemi.android:ble`, `no.nordicsemi.android:ble-ktx`) | `2.11.0` | Already decided in ticket 03. |
| `androidx.core:core-ktx` | `1.17.0` | Current stable. |
| `androidx.lifecycle:lifecycle-runtime-ktx` | `2.9.3` | Current stable. |
| `androidx.activity:activity-compose` | `1.11.0` | Current stable (last version before the `1.12.0` beta line). |
| `org.jetbrains.kotlinx:kotlinx-coroutines-android` | `1.11.0` | Current stable; needed explicitly since `BleControlRepository` exposes `StateFlow` and Nordic's `ble-ktx` suspend surface runs on coroutines. |

## Scope boundary (read before touching files)

This task creates the **project shell** and the **`ble/` package** (`BleControlRepository`, `NordicBleControlRepository`, `ConnectionState`, `ResponseFrame`, opcode table) — the pieces ticket 06 actually designed. It does **not** create:
- `di/BleModule.kt`, the Hilt plugin application, or `@HiltAndroidApp`/`@AndroidEntryPoint` annotations — ticket 09 / backlog task 9.
- `ui/` (screens, `DeviceViewModel`) — ticket 08/09 / backlog tasks 9–10.
- `FakeBleControlRepository` / any test source set — ticket 09/12 / backlog tasks 9 and 11.
- A launcher `Activity` or `Application` subclass — the manifest in this task has no `<activity>` by design; task 10 adds it.

If a future step needs any of the above, it belongs in that task's own plan, not a retrofit here.

## Files to create

All paths relative to repo root `/Users/k/Documents/pgpemu`.

### 1. Gradle project shell

- `companion-app/settings.gradle.kts`
  - `pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }`
  - `dependencyResolutionManagement { repositories { google(); mavenCentral() } }`
  - `rootProject.name = "companion-app"`
  - `include(":app")`

- `companion-app/build.gradle.kts` (root, empty plugin block)
  ```kotlin
  plugins {
      alias(libs.plugins.android.application) apply false
      alias(libs.plugins.kotlin.android) apply false
      alias(libs.plugins.kotlin.compose) apply false
  }
  ```
  (No Hilt plugin alias — not in the catalog either, per Scope boundary above.)

- `companion-app/gradle/libs.versions.toml` — version catalog with the `[versions]`/`[libraries]`/`[plugins]` blocks for every artifact this task actually uses. Plugin aliases: `android-application`, `kotlin-android`, `kotlin-compose`. Library aliases: `androidx-core-ktx`, `androidx-lifecycle-runtime-ktx`, `androidx-activity-compose`, `compose-bom`, `compose-ui`, `compose-ui-graphics`, `compose-ui-tooling-preview`, `compose-material3`, `nordic-ble`, `nordic-ble-ktx`, `kotlinx-coroutines-android`. No Hilt entries — task 9 adds its own catalog entries using the version pinned in the table above when it applies the plugin.

- `companion-app/gradle/wrapper/gradle-wrapper.properties` — `distributionUrl` for Gradle `9.1.0` (`https://services.gradle.org/distributions/gradle-9.1.0-bin.zip`), standard `distributionBase`/`zipStoreBase` = `GRADLE_USER_HOME`.
  - `gradlew` / `gradlew.bat` / `gradle/wrapper/gradle-wrapper.jar`: standard Gradle-generated wrapper scripts/jar, unmodified boilerplate. Generate by running `gradle wrapper --gradle-version 9.1.0` from inside `companion-app/` once `settings.gradle.kts` exists (requires a local `gradle` install; this is a Gradle operation, not an ESP-IDF one, so it's outside AGENTS.md's "never run esp-idf" restriction — but per this planning run's constraint, do **not** run it now; the executing step does).

- `companion-app/app/build.gradle.kts`
  ```kotlin
  plugins {
      alias(libs.plugins.android.application)
      alias(libs.plugins.kotlin.android)
      alias(libs.plugins.kotlin.compose)
  }

  android {
      namespace = "com.pgpemu.companion"
      compileSdk = 36

      defaultConfig {
          applicationId = "com.pgpemu.companion"
          minSdk = 31
          targetSdk = 36
          versionCode = 1
          versionName = "1.0"
      }

      buildTypes {
          release {
              isMinifyEnabled = false
          }
      }

      compileOptions {
          sourceCompatibility = JavaVersion.VERSION_17
          targetCompatibility = JavaVersion.VERSION_17
      }

      kotlinOptions {
          jvmTarget = "17"
      }

      buildFeatures {
          compose = true
      }
  }

  dependencies {
      implementation(libs.androidx.core.ktx)
      implementation(libs.androidx.lifecycle.runtime.ktx)
      implementation(libs.androidx.activity.compose)
      implementation(platform(libs.compose.bom))
      implementation(libs.compose.ui)
      implementation(libs.compose.ui.graphics)
      implementation(libs.compose.ui.tooling.preview)
      implementation(libs.compose.material3)
      implementation(libs.nordic.ble)
      implementation(libs.nordic.ble.ktx)
      implementation(libs.kotlinx.coroutines.android)
  }
  ```

- `companion-app/app/src/main/AndroidManifest.xml`
  ```xml
  <manifest xmlns:android="http://schemas.android.com/apk/res/android">

      <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
      <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

      <application
          android:allowBackup="true"
          android:label="Companion App"
          android:theme="@android:style/Theme.Material.Light.NoActionBar" />

  </manifest>
  ```
  minSdk 31 means the legacy `BLUETOOTH`/`BLUETOOTH_ADMIN`/`ACCESS_FINE_LOCATION` permissions are not needed (Android 12+ runtime BLE permission model only). No `<activity>` — see Scope boundary.

### 2. `ble/` package — `companion-app/app/src/main/kotlin/com/pgpemu/companion/ble/`

- **`ConnectionState.kt`** — sealed interface/class per ticket 06, one `StateFlow<ConnectionState>` surface:
  ```kotlin
  package com.pgpemu.companion.ble

  sealed interface ConnectionState {
      data object Idle : ConnectionState
      data object Scanning : ConnectionState
      data object Connecting : ConnectionState
      data object Bonding : ConnectionState
      data object DiscoveringServices : ConnectionState
      data object Ready : ConnectionState
      data class Disconnected(val reason: String) : ConnectionState
      data class Error(val reason: String) : ConnectionState
  }
  ```

- **`ResponseFrame.kt`** — wire-format struct + status codes, per ticket 04:
  ```kotlin
  package com.pgpemu.companion.ble

  data class ResponseFrame(
      val status: Byte,
      val opcode: Byte,
      val payload: ByteArray,
  ) {
      val isOk: Boolean get() = status == StatusCode.OK

      override fun equals(other: Any?): Boolean =
          other is ResponseFrame && status == other.status && opcode == other.opcode &&
              payload.contentEquals(other.payload)

      override fun hashCode(): Int =
          31 * (31 * status + opcode) + payload.contentHashCode()
  }

  object StatusCode {
      const val OK: Byte = 0x00
      const val ERR_UNKNOWN_OPCODE: Byte = 0x01
      const val ERR_MALFORMED_PAYLOAD: Byte = 0x02
      const val ERR_NOT_BONDED: Byte = 0x03
      const val ERR_BUSY: Byte = 0x04
      const val ERR_INTERNAL: Byte = 0x05
  }
  ```
  (Manual `equals`/`hashCode` needed because `ByteArray` breaks Kotlin `data class` structural equality — required here since tests will assert on parsed frames.)

- **`Opcode.kt`** — the 17-opcode table from ticket 04, ported 1:1:
  ```kotlin
  package com.pgpemu.companion.ble

  object Opcode {
      const val HELP: Int = 0x01
      const val GET_GLOBAL_SETTINGS: Int = 0x02
      const val SAVE_SETTINGS: Int = 0x03
      const val GET_SECRETS: Int = 0x04
      const val RESET_SECRETS: Int = 0x05
      const val RESTART: Int = 0x06
      const val GET_LED_STATE: Int = 0x07
      const val CYCLE_LOG_LEVEL: Int = 0x08
      const val GET_RUNTIME_STATS: Int = 0x09
      const val GET_TASK_LIST: Int = 0x0A
      const val ADVERTISE_START: Int = 0x0B
      const val ADVERTISE_STOP: Int = 0x0C
      const val GET_CLIENT_STATES: Int = 0x0D
      const val RESET_CLIENT_STATES: Int = 0x0E
      const val SET_MAX_CONNECTIONS: Int = 0x0F
      const val TOGGLE_AUTOSPIN: Int = 0x10
      const val TOGGLE_AUTOCATCH: Int = 0x11
  }
  ```
  No request/response payload parsing helpers here — each caller (task 10's screens) builds/reads the opcode-specific payload shape from ticket 04's table directly; adding a generic payload-codec layer now would be speculative for a single-command-at-a-time app with 17 fixed shapes.

- **`BleControlRepository.kt`** — the interface ticket 06 names:
  ```kotlin
  package com.pgpemu.companion.ble

  import kotlinx.coroutines.flow.StateFlow

  interface BleControlRepository {
      val connectionState: StateFlow<ConnectionState>

      suspend fun connect()

      suspend fun disconnect()

      suspend fun sendCommand(opcode: Int, payload: ByteArray = ByteArray(0)): Result<ResponseFrame>
  }
  ```
  `connect()`/`disconnect()` are on the interface (not just `sendCommand`) because ticket 06's scan→connect→bond→discover→ready state machine and ticket 06's "manual reconnect only" policy both require an explicit caller-driven entry/exit point — `sendCommand` alone can't express "start scanning."

- **`NordicBleControlRepository.kt`** — impl built on `no.nordicsemi.android:ble`/`ble-ktx`, per ticket 06/03:
  ```kotlin
  package com.pgpemu.companion.ble

  import android.bluetooth.BluetoothDevice
  import android.bluetooth.le.ScanCallback
  import android.bluetooth.le.ScanFilter
  import android.bluetooth.le.ScanResult
  import android.bluetooth.le.ScanSettings
  import android.content.Context
  import kotlinx.coroutines.CompletableDeferred
  import kotlinx.coroutines.flow.MutableStateFlow
  import kotlinx.coroutines.flow.StateFlow
  import kotlinx.coroutines.flow.asStateFlow
  import kotlinx.coroutines.suspendCancellableCoroutine
  import kotlinx.coroutines.withTimeoutOrNull
  import no.nordicsemi.android.ble.BleManager
  import no.nordicsemi.android.ble.observer.ConnectionObserver
  import java.util.UUID
  import kotlin.coroutines.resume
  import kotlin.coroutines.resumeWithException

  private val CONTROL_SERVICE_UUID: UUID = UUID.fromString("PLACEHOLDER-CONTROL-SERVICE-UUID")
  private val COMMAND_CHARACTERISTIC_UUID: UUID = UUID.fromString("PLACEHOLDER-COMMAND-CHAR-UUID")
  private val RESPONSE_CHARACTERISTIC_UUID: UUID = UUID.fromString("PLACEHOLDER-RESPONSE-CHAR-UUID")
  private const val PGP_ADVERTISED_NAME = "PLACEHOLDER-PGP-DEVICE-NAME" // unchanged existing advertised name, ticket 06
  private const val COMMAND_TIMEOUT_MS = 5_000L
  private const val SCAN_TIMEOUT_MS = 15_000L
  private const val CONNECT_TIMEOUT_MS = 15_000L

  class NordicBleControlRepository(
      context: Context,
  ) : BleControlRepository {

      private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Idle)
      override val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

      private var pendingResponse: CompletableDeferred<ResponseFrame>? = null

      private val manager = ControlBleManager(context)
      private val bluetoothAdapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager).adapter

      override suspend fun connect() {
          _connectionState.value = ConnectionState.Scanning
          val device = try {
              withTimeoutOrNull(SCAN_TIMEOUT_MS) { scanForDevice() }
                  ?: run { _connectionState.value = ConnectionState.Error("scan timed out"); return }
          } catch (e: Exception) {
              _connectionState.value = ConnectionState.Error("scan failed: ${e.message}")
              return
          }
          try {
              manager.connect(device)
                  .retry(3, 100)
                  .useAutoConnect(false)
                  .timeout(CONNECT_TIMEOUT_MS)
                  .suspend()
          } catch (e: Exception) {
              // ControlBleManager.onDeviceFailedToConnect already sets ConnectionState.Error
              // (mapping "service not supported" to "device not migrated") before this rethrows.
          }
      }

      /** Filters by [PGP_ADVERTISED_NAME] — the existing PGP advertised name, unchanged per ticket 06/the map. */
      private suspend fun scanForDevice(): BluetoothDevice = suspendCancellableCoroutine { cont ->
          val scanner = bluetoothAdapter.bluetoothLeScanner
          val filter = ScanFilter.Builder().setDeviceName(PGP_ADVERTISED_NAME).build()
          val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
          val callback = object : ScanCallback() {
              override fun onScanResult(callbackType: Int, result: ScanResult) {
                  scanner.stopScan(this)
                  cont.resume(result.device)
              }
              override fun onScanFailed(errorCode: Int) {
                  cont.resumeWithException(IllegalStateException("scan failed: $errorCode"))
              }
          }
          cont.invokeOnCancellation { scanner.stopScan(callback) }
          scanner.startScan(listOf(filter), settings, callback)
      }

      override suspend fun disconnect() {
          manager.disconnect().suspend()
          _connectionState.value = ConnectionState.Disconnected("user requested")
      }

      override suspend fun sendCommand(opcode: Int, payload: ByteArray): Result<ResponseFrame> =
          runCatching {
              val deferred = CompletableDeferred<ResponseFrame>()
              pendingResponse = deferred
              val request = byteArrayOf(opcode.toByte(), *payload)
              manager.writeCommand(request).suspend()
              withTimeoutOrNull(COMMAND_TIMEOUT_MS) { deferred.await() }
                  ?: throw java.util.concurrent.TimeoutException("no response for opcode $opcode")
          }.also {
              pendingResponse = null
          }

      private fun onDisconnected() {
          pendingResponse?.let { deferred ->
              if (!deferred.isCompleted) {
                  deferred.completeExceptionally(IllegalStateException("disconnected mid-command"))
              }
          }
          pendingResponse = null
      }

      /**
       * Nordic BleManager subclass — owns the GATT callback, service discovery,
       * and the Command/Response characteristic pair. Skeleton only: exact
       * ble-ktx suspend-extension names (`.suspend()` on `WriteRequest`/
       * `ConnectRequest`, `setNotificationCallback().with { }`) must be checked
       * against the pinned 2.11.0 docs/samples during implementation — this
       * plan is not build-verified.
       */
      private inner class ControlBleManager(context: Context) : BleManager(context) {
          init {
              connectionObserver = object : ConnectionObserver {
                  override fun onDeviceConnecting(device: android.bluetooth.BluetoothDevice) {
                      _connectionState.value = ConnectionState.Connecting
                  }
                  override fun onDeviceConnected(device: android.bluetooth.BluetoothDevice) {
                      _connectionState.value = ConnectionState.DiscoveringServices
                  }
                  override fun onDeviceReady(device: android.bluetooth.BluetoothDevice) {
                      _connectionState.value = ConnectionState.Ready
                  }
                  override fun onDeviceFailedToConnect(device: android.bluetooth.BluetoothDevice, reason: Int) {
                      // FailCallback.REASON_DEVICE_NOT_SUPPORTED (isRequiredServiceSupported() returned
                      // false, i.e. Control Service UUID absent post-discovery) — verify this constant
                      // name/value against the pinned 2.11.0 docs during implementation.
                      _connectionState.value = if (reason == no.nordicsemi.android.ble.callback.FailCallback.REASON_DEVICE_NOT_SUPPORTED) {
                          ConnectionState.Error("device not migrated")
                      } else {
                          ConnectionState.Error("connect failed: $reason")
                      }
                  }
                  override fun onDeviceDisconnecting(device: android.bluetooth.BluetoothDevice) {}
                  override fun onDeviceDisconnected(device: android.bluetooth.BluetoothDevice, reason: Int) {
                      onDisconnected()
                      _connectionState.value = ConnectionState.Disconnected(reason.toString())
                  }
              }
          }

          private var commandCharacteristic: android.bluetooth.BluetoothGattCharacteristic? = null
          private var responseCharacteristic: android.bluetooth.BluetoothGattCharacteristic? = null

          override fun getGattCallback(): BleManagerGattCallback = object : BleManagerGattCallback() {
              override fun isRequiredServiceSupported(gatt: android.bluetooth.BluetoothGatt): Boolean {
                  val service = gatt.getService(CONTROL_SERVICE_UUID) ?: return false
                  commandCharacteristic = service.getCharacteristic(COMMAND_CHARACTERISTIC_UUID)
                  responseCharacteristic = service.getCharacteristic(RESPONSE_CHARACTERISTIC_UUID)
                  return commandCharacteristic != null && responseCharacteristic != null
              }

              override fun initialize() {
                  setIndicationCallback(responseCharacteristic).with { _, data ->
                      val bytes = data.value ?: return@with
                      if (bytes.size >= 2) {
                          val frame = ResponseFrame(
                              status = bytes[0],
                              opcode = bytes[1],
                              payload = bytes.copyOfRange(2, bytes.size),
                          )
                          pendingResponse?.complete(frame)
                      }
                  }
                  enableIndications(responseCharacteristic).enqueue()
              }

              override fun onServicesInvalidated() {
                  commandCharacteristic = null
                  responseCharacteristic = null
              }
          }

          fun writeCommand(bytes: ByteArray) =
              writeCharacteristic(
                  commandCharacteristic,
                  bytes,
                  android.bluetooth.BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
              )
      }
  }
  ```
  The three `PLACEHOLDER-*-UUID` constants are intentional: the actual Control Service/Command/Response UUIDs are picked "at implementation time" per ticket 04 (`pgp_gatts.h`, firmware side) — this file cannot invent them. Whoever implements the firmware side of ticket 04 fills these in on both sides from the same generated UUID; flag this as a cross-cutting TODO, not a gap in this plan.

## Order of operations

1. Gradle shell first (`settings.gradle.kts` → root `build.gradle.kts` → `libs.versions.toml` → `app/build.gradle.kts` → manifest) — nothing under `ble/` compiles without it.
2. `ConnectionState.kt`, `Opcode.kt`, `ResponseFrame.kt` — no dependencies on each other beyond `StatusCode`/`ResponseFrame` living in the same file.
3. `BleControlRepository.kt` — depends on `ConnectionState`/`ResponseFrame`.
4. `NordicBleControlRepository.kt` — depends on all of the above plus the Nordic `ble`/`ble-ktx` dependency declared in step 1.
5. Generate the Gradle wrapper (`gradle wrapper --gradle-version 9.1.0` from `companion-app/`) — can happen any time after step 1, before or after the Kotlin sources.

## Verification

No build/compile verification is available (same constraint as the ESP-IDF firmware side — confirmed acceptable, per this task's brief). Verification is therefore structural, done by re-reading each file against its source of truth before considering the task done:

- `ConnectionState`'s eight variants match ticket 06's list exactly, no more, no fewer.
- `Opcode`'s 17 constants and values match ticket 04's table exactly (`0x01`–`0x11`).
- `StatusCode`'s 6 values match ticket 04's status-code convention exactly.
- `BleControlRepository.sendCommand`'s signature matches ticket 06's `suspend fun sendCommand(opcode: Int, payload: ByteArray): Result<ResponseFrame>` (default payload added for the zero-payload opcodes in ticket 04's table — a signature-compatible superset, not a deviation).
- `NordicBleControlRepository`'s 5-second timeout / immediate-fail-on-disconnect behavior matches ticket 10 exactly.
- Every version in `libs.versions.toml` matches the pinned-versions table above (no silent "latest" ranges).
- No file under `di/`, `ui/`, or any `test`/`androidTest` source set was created — confirms the Scope boundary held.
