// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: Shizuku's binder wrapper replaced by a helper process that su starts as the shell user.
package com.pgpemu.companion.pairing

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.AttributionSource
import android.content.Context
import android.os.Build
import android.os.IInterface
import android.os.Looper
import android.os.Process
import androidx.annotation.RequiresApi
import java.time.Duration
import java.util.concurrent.TimeUnit
import kotlin.system.exitProcess

/** Starts the confirm helper through su; every failure returns false so Android's dialog stays. */
object RootBluetooth {
    private const val SHELL_UID = "2000"
    private const val GRANT_TIMEOUT_SECONDS = 30L
    private const val CONFIRM_TIMEOUT_SECONDS = 8L

    /** The first call shows the root manager's prompt, so run it off the main thread. */
    fun isRootGranted() = run(GRANT_TIMEOUT_SECONDS, "su", "-c", "id -u") == "0"

    fun setPairingConfirmation(
        context: Context,
        device: BluetoothDevice,
    ): Boolean {
        // The shell user is the identity the Bluetooth service accepts for this call; root's own uid has no package.
        val command =
            "CLASSPATH=${context.applicationInfo.sourceDir} app_process / " +
                "${RootConfirmMain::class.java.name} ${device.address}"
        return run(CONFIRM_TIMEOUT_SECONDS, "su", SHELL_UID, "-c", command) == "true"
    }

    private fun run(
        timeoutSeconds: Long,
        vararg command: String,
    ): String? =
        try {
            val process = ProcessBuilder(*command).redirectErrorStream(true).start()
            if (process.waitFor(timeoutSeconds, TimeUnit.SECONDS)) {
                process.inputStream
                    .bufferedReader()
                    .readText()
                    .trim()
                    .lines()
                    .last()
            } else {
                process.destroyForcibly()
                null
            }
        } catch (_: java.io.IOException) {
            null
        }
}

/** Runs under app_process as the shell user: no app process, so the hidden Bluetooth API is not blocked. */
object RootConfirmMain {
    private const val BLUETOOTH_JARJAR_PREFIX = "com.android.bluetooth.x."
    private const val SYNCHRONOUS_RESULT_RECEIVER =
        "${BLUETOOTH_JARJAR_PREFIX}com.android.modules.utils.SynchronousResultReceiver"
    private val synchronousResultTimeout: Duration = Duration.ofSeconds(5)

    @JvmStatic
    fun main(args: Array<String>) {
        // BluetoothAdapter.getDefaultAdapter() returns null without an app context, so borrow the system one.
        Looper.prepareMainLooper()
        val thread = Class.forName("android.app.ActivityThread")
        // Registers the Bluetooth service wrapper that app processes get at startup; absent before API 30.
        runCatching { thread.getMethod("initializeMainlineModules")(null) }
        val systemContext =
            thread.getMethod("getSystemContext")(thread.getMethod("systemMain")(null)) as Context
        val adapter = systemContext.getSystemService(BluetoothManager::class.java).adapter
        println(setPairingConfirmation(adapter, adapter.getRemoteDevice(args[0])))
        exitProcess(0)
    }

    // API 28-30 only expose the callback overload; API 31 added the no-argument one.
    private fun bluetoothService(adapter: BluetoothAdapter): IInterface =
        try {
            val method = BluetoothAdapter::class.java.getDeclaredMethod("getBluetoothService")
            method.isAccessible = true
            method(adapter) as IInterface
        } catch (_: NoSuchMethodException) {
            val method =
                BluetoothAdapter::class.java.getDeclaredMethod(
                    "getBluetoothService",
                    Class.forName("android.bluetooth.IBluetoothManagerCallback"),
                )
            method.isAccessible = true
            method(adapter, null) as IInterface
        }

    @RequiresApi(31)
    private fun shellAttributionSource() =
        AttributionSource
            .Builder(Process.SHELL_UID)
            .setPackageName("com.android.shell")
            .build()

    // Signatures change at API 31 (attribution source) and 33 (SynchronousResultReceiver, jarjar-shaded on 13-14).
    // One Motorola Android 13 build exposes the unshaded name, so the shaded name falls back to the plain one.
    private fun setPairingConfirmation(
        adapter: BluetoothAdapter,
        device: BluetoothDevice,
    ): Boolean {
        val service = bluetoothService(adapter)
        val serviceClass = Class.forName("android.bluetooth.IBluetooth")
        if (Build.VERSION.SDK_INT < 31) {
            val method = serviceClass.getMethod("setPairingConfirmation", BluetoothDevice::class.java, java.lang.Boolean.TYPE)
            return method(service, device, true) as Boolean
        }
        return try {
            val method =
                serviceClass.getMethod(
                    "setPairingConfirmation",
                    BluetoothDevice::class.java,
                    java.lang.Boolean.TYPE,
                    AttributionSource::class.java,
                )
            method(service, device, true, shellAttributionSource()) as Boolean
        } catch (_: NoSuchMethodException) {
            val receiverClass =
                try {
                    Class.forName(SYNCHRONOUS_RESULT_RECEIVER)
                } catch (_: ClassNotFoundException) {
                    Class.forName(SYNCHRONOUS_RESULT_RECEIVER.removePrefix(BLUETOOTH_JARJAR_PREFIX))
                }
            val method =
                serviceClass.getMethod(
                    "setPairingConfirmation",
                    BluetoothDevice::class.java,
                    java.lang.Boolean.TYPE,
                    AttributionSource::class.java,
                    receiverClass,
                )
            val awaitResult = receiverClass.getMethod("awaitResultNoInterrupt", Duration::class.java)
            val getValue = awaitResult.returnType.getMethod("getValue", Any::class.java)
            val receiver = receiverClass.getMethod("get")(null)
            method(service, device, true, shellAttributionSource(), receiver)
            val result =
                awaitResult(receiver, synchronousResultTimeout)
                    ?: error("SynchronousResultReceiver returned no result")
            getValue(result, false) as Boolean
        }
    }
}
