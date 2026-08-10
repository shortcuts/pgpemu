# Research Android BLE central APIs and libraries

Type: research
Status: resolved

## Question

For a single-module Jetpack Compose + Hilt Android app (min/target SDK matching locationjoystick's compileSdk=36/minSdk=31) that must connect to one bonded BLE peripheral, discover a custom GATT service by name-scan-then-discover, write to a Command characteristic, and receive notify/indicate responses (chunked per ticket 01's findings): compare native `BluetoothGatt`/`BluetoothLeScanner` against a library such as Nordic's `no.nordicsemi.android:ble` — API ergonomics, coroutine/Flow support, bonding-flow handling, and current maintenance status/version to pin in `libs.versions.toml`. Recommend one.

## Answer

**Native `BluetoothGatt`/`BluetoothLeScanner`**

The official Android guide covers `connectGatt()`, `BluetoothGattCallback`, service discovery, and characteristic read/write/notify as separate async, callback-driven steps ([Connect to a GATT server](https://developer.android.com/develop/connectivity/bluetooth/ble/connect-gatt-server), [Transfer BLE data](https://developer.android.com/develop/connectivity/bluetooth/ble/transfer-ble-data)). Neither page documents built-in operation queueing. This matches the well-known Android platform limitation: only one outstanding GATT operation is allowed on a `BluetoothGatt` instance at a time — issuing a second `writeCharacteristic`/`readCharacteristic`/`writeDescriptor` before the previous operation's callback fires silently fails or corrupts state. The app must hand-roll a serial operation queue, connect retry/backoff logic, MTU negotiation, and bonding-state handling (`BluetoothDevice.createBond()`, `ACTION_BOND_STATE_CHANGED`) itself. There is no coroutine/Flow wrapper in the platform API; every call needs manual `suspendCancellableCoroutine` or callback-to-Flow plumbing per operation, and reassembling chunked notify/indicate payloads (per ticket 01) is entirely the app's responsibility.

**Nordic `no.nordicsemi.android:ble`**

The library's stated purpose is to close exactly these gaps: "Asynchronous and synchronous BLE operations using queue", "Connection, with automatic retries", MTU/connection-priority requests, and optional bonding handling, plus automatic Service Changed handling and reliable-write support ([GitHub — NordicSemiconductor/Android-BLE-Library](https://github.com/NordicSemiconductor/Android-BLE-Library)). The `ble-ktx` module adds first-class coroutine/Flow support: suspend functions on `Request` objects and `asFlow()` on `ValueChangedCallback`, which maps directly onto "await command ack" and "collect chunked notify stream" use cases from ticket 01. It builds on `BluetoothGattCallback` under the hood, so it is not a reimplementation of the BLE stack — it is a queueing/retry/coroutine layer on top of the same platform APIs.

**Maintenance status / version**

Latest stable release is **2.11.0**, tagged 2025-09-11, which bumped the library's own Kotlin version to 2.2.0 and target/compile SDK to API 36 ([Releases — NordicSemiconductor/Android-BLE-Library](https://github.com/NordicSemiconductor/Android-BLE-Library/releases), [Maven Central](https://search.maven.org/artifact/no.nordicsemi.android/ble/2.11.0/aar)) — release cadence is active (roughly quarterly) and current SDK targeting lines up with locationjoystick's compileSdk=36. Nordic also publishes a newer `no.nordicsemi.android.kotlin.ble` ("Kotlin BLE Library v2.0", core module latest stable 1.3.1) intended as the eventual successor, but it is younger, less documented, and not yet positioned by Nordic as the default recommendation — the classic `:ble`/`:ble-ktx` pair remains the maintained, production-proven option.

**Recommendation**

Use `no.nordicsemi.android:ble:2.11.0` with the `no.nordicsemi.android:ble-ktx:2.11.0` extension, pinned in `libs.versions.toml`. It directly solves the operation-queueing, retry, bonding, and chunked-notification-reassembly needs this app has, with coroutine/Flow ergonomics that fit a Compose+Hilt codebase, for the cost of one well-maintained dependency instead of hand-rolled queueing/retry/coroutine-bridging code around the native APIs.
