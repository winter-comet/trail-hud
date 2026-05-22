package com.trailhud.app.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat
import com.trailhud.app.protocol.TrailHudPacket
import java.nio.charset.StandardCharsets
import java.util.UUID

class Hm10BleClient(
    private val context: Context,
    private val onReady: () -> Unit,
    private val onDisconnected: () -> Unit,
    private val onRssiRead: (Int) -> Unit,
    private val onError: (String) -> Unit
) {
    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private val pendingChunks = ArrayDeque<ByteArray>()
    private var isWriting = false
    private val rxLineBuilder = StringBuilder()

    private val callback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError("BLE connection failed with status $status")
                close()
                return
            }

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                if (hasBluetoothConnectPermission()) {
                    gatt.discoverServices()
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                close()
                onDisconnected()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError("BLE service discovery failed with status $status")
                return
            }

            val characteristic = gatt
                .getService(HM10_SERVICE_UUID)
                ?.getCharacteristic(HM10_CHARACTERISTIC_UUID)

            if (characteristic == null) {
                onError("HM-10 BLE UART characteristic was not found")
                return
            }

            txCharacteristic = characteristic

            if (!enableIncomingData(gatt, characteristic)) {
                onReady()
            }
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            if (descriptor.uuid != CLIENT_CHARACTERISTIC_CONFIG_UUID) {
                return
            }

            if (status == BluetoothGatt.GATT_SUCCESS) {
                onReady()
            } else {
                onError("HM-10 notification setup failed with status $status")
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            isWriting = false

            if (status != BluetoothGatt.GATT_SUCCESS) {
                onError("BLE write failed with status $status")
                pendingChunks.clear()
                return
            }

            drainQueue()
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            @Suppress("DEPRECATION")
            handleIncomingBytes(characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleIncomingBytes(value)
        }

        override fun onReadRemoteRssi(gatt: BluetoothGatt, rssi: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                onRssiRead(rssi)
            }
        }
    }

    fun connect(device: BluetoothDevice) {
        if (!hasBluetoothConnectPermission()) {
            onError("Missing BLUETOOTH_CONNECT permission")
            return
        }

        @SuppressLint("MissingPermission")
        gatt = device.connectGatt(context, false, callback)
    }

    @SuppressLint("MissingPermission")
    fun close() {
        pendingChunks.clear()
        isWriting = false
        txCharacteristic = null
        rxLineBuilder.clear()

        if (hasBluetoothConnectPermission()) {
            gatt?.disconnect()
            gatt?.close()
        }

        gatt = null
    }

    fun writeLine(line: String) {
        val bytes = (line + "\n").toByteArray(StandardCharsets.UTF_8)
        var offset = 0

        while (offset < bytes.size) {
            val end = (offset + BLE_UART_CHUNK_SIZE).coerceAtMost(bytes.size)
            pendingChunks.add(bytes.copyOfRange(offset, end))
            offset = end
        }

        drainQueue()
    }

    @SuppressLint("MissingPermission")
    fun readRemoteRssi() {
        if (hasBluetoothConnectPermission()) {
            gatt?.readRemoteRssi()
        }
    }

    @SuppressLint("MissingPermission")
    private fun enableIncomingData(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic
    ): Boolean {
        val properties = characteristic.properties
        val cccdValue = when {
            properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 ->
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            properties and BluetoothGattCharacteristic.PROPERTY_INDICATE != 0 ->
                BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
            else -> return false
        }

        if (!hasBluetoothConnectPermission()) {
            onError("Missing BLUETOOTH_CONNECT permission")
            return false
        }

        if (!gatt.setCharacteristicNotification(characteristic, true)) {
            onError("HM-10 notification setup was rejected")
            return false
        }

        val descriptor = characteristic.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG_UUID)
        if (descriptor == null) {
            onError("HM-10 notification descriptor was not found")
            return false
        }

        return writeDescriptorCompat(gatt, descriptor, cccdValue)
    }

    @SuppressLint("MissingPermission")
    private fun writeDescriptorCompat(
        gatt: BluetoothGatt,
        descriptor: BluetoothGattDescriptor,
        value: ByteArray
    ): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeDescriptor(descriptor, value) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            descriptor.value = value
            @Suppress("DEPRECATION")
            gatt.writeDescriptor(descriptor)
        }
    }

    @SuppressLint("MissingPermission")
    private fun drainQueue() {
        val currentGatt = gatt ?: return
        val characteristic = txCharacteristic ?: return

        if (isWriting || pendingChunks.isEmpty() || !hasBluetoothConnectPermission()) {
            return
        }

        val nextChunk = pendingChunks.removeFirst()
        val writeType = if (characteristic.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0) {
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        } else {
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        }
        val waitsForCallback = writeType != BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE

        isWriting = true

        val accepted = writeCharacteristicCompat(currentGatt, characteristic, nextChunk, writeType)

        if (!accepted) {
            isWriting = false
            pendingChunks.addFirst(nextChunk)
            onError("BLE write was rejected")
        } else if (!waitsForCallback) {
            isWriting = false
            drainQueue()
        }
    }

    @SuppressLint("MissingPermission")
    private fun writeCharacteristicCompat(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray,
        writeType: Int
    ): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeCharacteristic(characteristic, value, writeType) == BluetoothStatusCodes.SUCCESS
        } else {
            characteristic.writeType = writeType
            @Suppress("DEPRECATION")
            characteristic.value = value
            @Suppress("DEPRECATION")
            gatt.writeCharacteristic(characteristic)
        }
    }

    private fun handleIncomingBytes(bytes: ByteArray) {
        bytes.forEach { byte ->
            when (val char = byte.toInt().toChar()) {
                '\r' -> Unit
                '\n' -> {
                    val line = rxLineBuilder.toString().trim()
                    rxLineBuilder.clear()
                    handleIncomingLine(line)
                }
                else -> {
                    if (rxLineBuilder.length < MAX_RX_LINE_LENGTH) {
                        rxLineBuilder.append(char)
                    } else {
                        rxLineBuilder.clear()
                    }
                }
            }
        }
    }

    private fun handleIncomingLine(line: String) {
        if (line == TrailHudPacket.STM32_PING_PACKET) {
            writeLine(TrailHudPacket.PHONE_PING_REPLY_PACKET)
        }
    }

    private fun hasBluetoothConnectPermission(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
                ContextCompat.checkSelfPermission(
                    context,
                    Manifest.permission.BLUETOOTH_CONNECT
                ) == PackageManager.PERMISSION_GRANTED
    }

    companion object {
        private val HM10_SERVICE_UUID: UUID =
            UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb")
        private val HM10_CHARACTERISTIC_UUID: UUID =
            UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb")
        private val CLIENT_CHARACTERISTIC_CONFIG_UUID: UUID =
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val BLE_UART_CHUNK_SIZE = 20
        private const val MAX_RX_LINE_LENGTH = 220
    }
}
