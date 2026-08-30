package com.trailhud.app.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.os.Build
import com.trailhud.app.protocol.TrailHudPacket
import com.trailhud.app.protocol.VibrationCommand
import java.nio.charset.StandardCharsets
import java.util.UUID

/**
 * BLE UART client for the STM32's Bluetooth module.
 *
 * The firmware's wiring notes call the module "HM-10 / AT-09" and mention
 * scanning for "BT05 or the configured HM-10 name" - and that lines up with
 * how these modules are actually described in the hobbyist ecosystem: the
 * HM-10, AT-09, and BT05/MLT-BT05 are commonly built around the same TI
 * CC2540/CC2541 BLE SoC, are sold interchangeably as "HM-10 compatible"
 * drop-ins, and expose the same custom UART service (0xFFE0) and
 * characteristic (0xFFE1) used below. There have been reported one-way
 * compatibility quirks between older HM-10 firmware and BT05/AT-09 clones,
 * said to be resolved from HM-10 firmware V700 onward, but the module
 * families are otherwise treated as compatible. So this class is named
 * after the app/protocol rather than one specific chip.
 */
class TrailHudBleClient(
    private val context: Context,
    private val onReady: () -> Unit,
    private val onDisconnected: () -> Unit,
    private val onRssiRead: (Int) -> Unit,
    private val onVibrationCommand: (VibrationCommand) -> Unit,
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
                if (context.hasBluetoothConnectPermission()) {
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
        if (!context.hasBluetoothConnectPermission()) {
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

        if (context.hasBluetoothConnectPermission()) {
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
        if (context.hasBluetoothConnectPermission()) {
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

        if (!context.hasBluetoothConnectPermission()) {
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

        if (isWriting || pendingChunks.isEmpty() || !context.hasBluetoothConnectPermission()) {
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
            return
        }

        TrailHudPacket.parseVibrationCommand(line)?.let(onVibrationCommand)
    }

    companion object {
        // The custom UART service/characteristic pair standardized by the
        // HM-10 and shared by its AT-09/BT05 compatible clones (see the
        // class doc comment above).
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
