package com.trailhud.app.ble

import android.Manifest
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.annotation.RequiresPermission
import androidx.core.app.ActivityCompat
import java.nio.charset.StandardCharsets
import java.util.ArrayDeque
import java.util.UUID

class Hm10BleClient(
    private val context: Context,
    private val onReady: () -> Unit = {},
    private val onDisconnected: () -> Unit = {},
    private val onRssiRead: (Int) -> Unit = {},
    private val onError: (String) -> Unit = {}
) {
    private val hm10ServiceUuid: UUID = UUID.fromString("0000FFE0-0000-1000-8000-00805F9B34FB")
    private val hm10CharacteristicUuid: UUID = UUID.fromString("0000FFE1-0000-1000-8000-00805F9B34FB")

    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private val pendingChunks = ArrayDeque<ByteArray>()
    private var writeInProgress = false

    private val callback = object : BluetoothGattCallback() {
        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError("GATT connection failed: status=$status")
                close()
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    if (!hasConnectPermission()) return
                    gatt.discoverServices()
                }

                BluetoothProfile.STATE_DISCONNECTED -> {
                    onDisconnected()
                    close()
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError("GATT service discovery failed: status=$status")
                return
            }

            val characteristic = gatt
                .getService(hm10ServiceUuid)
                ?.getCharacteristic(hm10CharacteristicUuid)

            if (characteristic == null) {
                onError("HM-10 UART characteristic FFE1 was not found")
                return
            }

            txCharacteristic = characteristic
            onReady()
        }

        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            writeInProgress = false
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError("GATT write failed: status=$status")
                pendingChunks.clear()
                return
            }
            drainQueue()
        }

        override fun onReadRemoteRssi(gatt: BluetoothGatt, rssi: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                onRssiRead(rssi)
            } else {
                onError("Remote RSSI read failed: status=$status")
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun connect(device: BluetoothDevice) {
        if (!hasConnectPermission()) return
        close()
        gatt = device.connectGatt(context, false, callback)
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun readRemoteRssi(): Boolean {
        if (!hasConnectPermission()) return false

        val accepted = gatt?.readRemoteRssi() ?: false
        if (!accepted) {
            onError("Remote RSSI read request was not accepted by Android")
        }
        return accepted
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun writeLine(line: String) {
        val bytes = (line + "\n").toByteArray(StandardCharsets.US_ASCII)

        synchronized(pendingChunks) {
            bytes.asIterable()
                .chunked(MAX_HM10_CHUNK_BYTES)
                .forEach { chunk ->
                    pendingChunks.add(chunk.toByteArray())
                }
        }

        drainQueue()
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun close() {
        val currentGatt = gatt
        gatt = null
        txCharacteristic = null
        pendingChunks.clear()
        writeInProgress = false

        if (hasConnectPermission()) {
            currentGatt?.disconnect()
            currentGatt?.close()
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun drainQueue() {
        if (writeInProgress) return
        if (!hasConnectPermission()) return

        val nextChunk = synchronized(pendingChunks) {
            if (pendingChunks.isEmpty()) null else pendingChunks.removeFirst()
        } ?: return

        val currentGatt = gatt ?: return
        val characteristic = txCharacteristic ?: return

        val properties = characteristic.properties
        val writeType = if ((properties and BluetoothGattCharacteristic.PROPERTY_WRITE) != 0) {
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        } else {
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        }

        writeInProgress = true

        val accepted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            currentGatt.writeCharacteristic(characteristic, nextChunk, writeType) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                characteristic.writeType = writeType
                characteristic.value = nextChunk
                currentGatt.writeCharacteristic(characteristic)
            }
        }

        if (!accepted) {
            writeInProgress = false
            onError("GATT write was not accepted by Android")
        } else if (writeType == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) {
            writeInProgress = false
            drainQueue()
        }
    }

    private fun hasConnectPermission(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            ActivityCompat.checkSelfPermission(
                context,
                Manifest.permission.BLUETOOTH_CONNECT
            ) == PackageManager.PERMISSION_GRANTED
    }

    private companion object {
        const val MAX_HM10_CHUNK_BYTES = 20
    }
}
