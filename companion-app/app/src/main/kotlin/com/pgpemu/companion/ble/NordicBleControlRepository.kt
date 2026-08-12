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
import no.nordicsemi.android.ble.callback.FailCallback
import no.nordicsemi.android.ble.error.GattError
import no.nordicsemi.android.ble.exception.RequestFailedException
import no.nordicsemi.android.ble.ktx.suspend
import no.nordicsemi.android.ble.observer.BondingObserver
import no.nordicsemi.android.ble.observer.ConnectionObserver
import dagger.hilt.android.qualifiers.ApplicationContext
import java.util.UUID
import javax.inject.Inject
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

// Same 128-bit vendor UUIDs as pgp_control.c's GATTS_SERVICE_UUID_CONTROL /
// GATTS_CHAR_UUID_CONTROL_COMMAND / GATTS_CHAR_UUID_CONTROL_RESPONSE
// (little-endian byte arrays there, reordered to standard UUID string form here).
private val CONTROL_SERVICE_UUID: UUID = UUID.fromString("bbe87709-5b89-4433-ab7f-8b8eef0d8e40")
private val COMMAND_CHARACTERISTIC_UUID: UUID = UUID.fromString("bbe87709-5b89-4433-ab7f-8b8eef0d8e41")
private val RESPONSE_CHARACTERISTIC_UUID: UUID = UUID.fromString("bbe87709-5b89-4433-ab7f-8b8eef0d8e42")
private const val PGP_ADVERTISED_NAME = "Pokemon GO Plus" // matches PGP_DEVICE_NAME in pgpemu-esp32/main/pgp_gatts.c
private const val COMMAND_TIMEOUT_MS = 5_000L
private const val SCAN_TIMEOUT_MS = 15_000L
private const val CONNECT_TIMEOUT_MS = 15_000L

/**
 * Human-readable text for a [ConnectionObserver.onDeviceFailedToConnect] reason code, with the
 * raw code kept in parens (e.g. "connection timed out (-5)") so it's still reportable.
 * Negative codes are [FailCallback.REASON_*][FailCallback]; positive codes are GATT status codes.
 */
internal fun describeConnectFailure(reason: Int): String {
    val description = when (reason) {
        FailCallback.REASON_DEVICE_DISCONNECTED -> "device disconnected before connecting"
        FailCallback.REASON_TIMEOUT -> "connection timed out"
        FailCallback.REASON_CANCELLED -> "connection cancelled"
        FailCallback.REASON_NOT_ENABLED -> "Bluetooth adapter is off"
        FailCallback.REASON_BLUETOOTH_DISABLED -> "Bluetooth is disabled"
        FailCallback.REASON_NULL_ATTRIBUTE -> "device is missing a required attribute"
        FailCallback.REASON_REQUEST_FAILED -> "connection request failed"
        FailCallback.REASON_VALIDATION -> "invalid connection request"
        FailCallback.REASON_UNSUPPORTED_CONFIGURATION -> "unsupported Bluetooth configuration"
        else -> if (reason > 0) GattError.parse(reason) else "unknown error"
    }
    return "$description ($reason)"
}

class NordicBleControlRepository @Inject constructor(
    @ApplicationContext context: Context,
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
            // ControlBleManager.onDeviceFailedToConnect usually sets ConnectionState.Error
            // (mapping "service not supported" to "device not migrated") before this rethrows,
            // but connect() can also throw synchronously without that callback firing (e.g. the
            // manager still holding a stale GATT from a prior unexpected disconnect) — leaving
            // the UI stuck on the Scanning/Connecting spinner forever. Fall back to Error so the
            // Connect button always comes back.
            if (_connectionState.value !is ConnectionState.Error) {
                _connectionState.value = ConnectionState.Error(e.message ?: "connect failed")
            }
        }
    }

    /** Filters by [PGP_ADVERTISED_NAME] — the existing PGP advertised name, unchanged per ticket 06/the map. */
    private suspend fun scanForDevice(): BluetoothDevice = suspendCancellableCoroutine { cont ->
        val scanner = bluetoothAdapter.bluetoothLeScanner
            ?: run { cont.resumeWithException(IllegalStateException("bluetooth adapter has no LE scanner (BT off?)")); return@suspendCancellableCoroutine }
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
            try {
                manager.writeCommand(request).suspend()
            } catch (e: RequestFailedException) {
                if (e.status in setOf(GattError.GATT_INSUF_AUTHENTICATION, GattError.GATT_INSUF_AUTHORIZATION,
                        GattError.GATT_INSUF_ENCRYPTION, GattError.GATT_AUTH_FAIL)) {
                    // Android still thinks the device is bonded, but the peripheral rejected
                    // encryption with our stored key (e.g. its keys were reset by a reflash).
                    // Drop the stale bond so the next connect re-pairs instead of hitting this
                    // same failure forever.
                    manager.forgetBond()
                }
                throw IllegalStateException("command write failed: ${GattError.parse(e.status)} (status ${e.status})", e)
            }
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
     * and the Command/Response characteristic pair. API surface (`.suspend()`
     * on `WriteRequest`/`ConnectRequest`, `setIndicationCallback().with { }`,
     * `FailCallback.REASON_DEVICE_NOT_SUPPORTED`) verified against the
     * no.nordicsemi.android:ble / ble-ktx 2.11.0 sources.
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
                    // false, i.e. Control Service UUID absent post-discovery); confirmed against the
                    // ble-ktx 2.11.0 sources.
                    _connectionState.value = if (reason == FailCallback.REASON_DEVICE_NOT_SUPPORTED) {
                        ConnectionState.Error("device not migrated")
                    } else {
                        ConnectionState.Error("connect failed: ${describeConnectFailure(reason)}")
                    }
                }
                override fun onDeviceDisconnecting(device: android.bluetooth.BluetoothDevice) {}
                override fun onDeviceDisconnected(device: android.bluetooth.BluetoothDevice, reason: Int) {
                    onDisconnected()
                    _connectionState.value = ConnectionState.Disconnected(reason.toString())
                }
            }
            bondingObserver = object : BondingObserver {
                override fun onBondingRequired(device: android.bluetooth.BluetoothDevice) {
                    _connectionState.value = ConnectionState.Bonding
                }
                override fun onBonded(device: android.bluetooth.BluetoothDevice) { /* next observer/init callback advances state */ }
                override fun onBondingFailed(device: android.bluetooth.BluetoothDevice) {
                    _connectionState.value = ConnectionState.Error("bonding failed")
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
                // Default ATT MTU is 23 bytes (20-byte payload). GET_CLIENT_SUMMARY alone
                // needs 44; GET_SECRETS needs 294. Request the max before anything else so
                // every later indication arrives whole instead of silently truncated.
                requestMtu(517).enqueue()

                // ensureBond() calls Android's createBond(), which returns false for an
                // already-bonded device; the library reads that as a stale bond and force
                // removes+recreates it, killing an already-working connection. Only ask for
                // it when there's no bond yet.
                if (bluetoothDevice?.bondState != BluetoothDevice.BOND_BONDED) {
                    ensureBond()
                        .fail { _, status -> _connectionState.value = ConnectionState.Error("bonding failed: ${GattError.parse(status)} ($status)") }
                        .enqueue()
                }
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

        // removeBond() is protected on BleManager; expose it for the stale-bond recovery
        // in sendCommand(), which holds a ControlBleManager reference, not a subclass of it.
        fun forgetBond() = removeBond().enqueue()
    }
}
