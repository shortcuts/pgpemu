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
import dagger.hilt.android.qualifiers.ApplicationContext
import java.util.UUID
import javax.inject.Inject
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

private val CONTROL_SERVICE_UUID: UUID = UUID.fromString("PLACEHOLDER-CONTROL-SERVICE-UUID")
private val COMMAND_CHARACTERISTIC_UUID: UUID = UUID.fromString("PLACEHOLDER-COMMAND-CHAR-UUID")
private val RESPONSE_CHARACTERISTIC_UUID: UUID = UUID.fromString("PLACEHOLDER-RESPONSE-CHAR-UUID")
private const val PGP_ADVERTISED_NAME = "PLACEHOLDER-PGP-DEVICE-NAME" // unchanged existing advertised name, ticket 06
private const val COMMAND_TIMEOUT_MS = 5_000L
private const val SCAN_TIMEOUT_MS = 15_000L
private const val CONNECT_TIMEOUT_MS = 15_000L

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
