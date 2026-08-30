package com.trailhud.app.service

import android.Manifest
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.annotation.RequiresPermission
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import com.trailhud.app.MainActivity
import com.trailhud.app.R
import com.trailhud.app.ble.TrailHudBleClient
import com.trailhud.app.ble.hasBluetoothConnectPermission
import com.trailhud.app.protocol.PhoneLocationPayload
import com.trailhud.app.protocol.PhoneRotationPayload
import com.trailhud.app.protocol.TrailHudPacket
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.math.PI

/**
 * UI-facing snapshot of everything [TrailHudService] tracks: the BLE link,
 * the last signal strength reading, and the phone's compass heading. The
 * discovered-device list is deliberately not part of this state - the UI
 * pulls it on demand via [TrailHudService.getPairedDevices] instead, the
 * same way the original Activity-owned implementation did.
 */
data class TrailHudUiState(
    val isBleConnected: Boolean = false,
    val rssiDbm: Int? = null,
    val rawHeadingDegrees: Float = 0f
)

/**
 * Foreground service that owns the HM-10 BLE connection, the phone's
 * rotation-vector sensor, and GPS updates, so the phone-pose broadcast to
 * the STM32 keeps running while the app is backgrounded or the screen is
 * off, instead of stopping the moment [MainActivity] leaves the foreground.
 *
 * This is also the single owner of the TYPE_ROTATION_VECTOR sensor: one
 * listener feeds both the outgoing BLE quaternion payload and the compass
 * heading shown in the UI, rather than the Activity and the Composable each
 * registering their own listener against the same sensor.
 *
 * The service is bound for the whole lifetime of [MainActivity] (so the
 * compass keeps working even while disconnected), but only promotes itself
 * to the foreground - showing a persistent notification - while a BLE
 * connection is actually established and streaming.
 */
class TrailHudService : Service() {

    inner class LocalBinder : Binder() {
        fun getService(): TrailHudService = this@TrailHudService
    }

    private val binder = LocalBinder()

    override fun onBind(intent: Intent?): IBinder = binder

    private val serviceJob = SupervisorJob()
    private val serviceScope = CoroutineScope(serviceJob + Dispatchers.Main)

    private val _uiState = MutableStateFlow(TrailHudUiState())
    val uiState: StateFlow<TrailHudUiState> = _uiState.asStateFlow()

    var updateRateSeconds: Double = TrailHudPacket.DEFAULT_UPDATE_RATE_SECONDS
    private val updateRateMs: Long
        get() = (updateRateSeconds * 1000.0).toLong().coerceAtLeast(100L)

    private var bleClient: TrailHudBleClient? = null
    private val vibrator by lazy { TrailHudVibrator(this) }
    private var broadcastJob: Job? = null
    private var scanJob: Job? = null
    private var locationListener: LocationListener? = null
    private var isForeground = false

    private val discoveredDevices = linkedMapOf<String, BluetoothDevice>()

    @Volatile
    private var lastLocation: Location? = null
    @Volatile
    private var lastRotationQuaternion: FloatArray? = null

    // Unwrapped heading target (can grow past 360 or below 0) so the compass
    // always spins the short way around 0/360 instead of snapping.
    private var unwrappedHeadingDegrees = 0f

    private val sensorManager: SensorManager? by lazy { getSystemService<SensorManager>() }
    private val locationManager: LocationManager? by lazy { getSystemService<LocationManager>() }
    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        getSystemService<BluetoothManager>()?.adapter
    }

    private val rotationSensorListener = object : SensorEventListener {
        override fun onSensorChanged(event: SensorEvent?) {
            if (event?.sensor?.type != Sensor.TYPE_ROTATION_VECTOR) return

            val quaternion = FloatArray(4)
            SensorManager.getQuaternionFromVector(quaternion, event.values)
            lastRotationQuaternion = quaternion

            val rotationMatrix = FloatArray(9)
            SensorManager.getRotationMatrixFromVector(rotationMatrix, event.values)
            val orientationValues = FloatArray(3)
            SensorManager.getOrientation(rotationMatrix, orientationValues)

            var azimuth = (orientationValues[0] * (180.0 / PI)).toFloat()
            if (azimuth < 0) azimuth += 360f

            var diff = azimuth - (unwrappedHeadingDegrees % 360f)
            while (diff > 180f) diff -= 360f
            while (diff < -180f) diff += 360f

            unwrappedHeadingDegrees += diff
            _uiState.value = _uiState.value.copy(rawHeadingDegrees = unwrappedHeadingDegrees)
        }

        override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            serviceScope.launch { addDiscoveredDevice(result) }
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            serviceScope.launch {
                results.forEach { addDiscoveredDevice(it) }
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Scan failed with error: $errorCode")
        }
    }

    private fun addDiscoveredDevice(result: ScanResult) {
        discoveredDevices[result.device.address] = result.device
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        registerRotationSensor()
    }

    override fun onDestroy() {
        vibrator.stop()
        disconnectFromDevice()
        unregisterRotationSensor()
        serviceJob.cancel()
        super.onDestroy()
    }

    /**
     * Registers the single TYPE_ROTATION_VECTOR listener used for both the
     * compass heading and the outgoing BLE quaternion. Some devices (no
     * gyroscope/magnetometer) report no default sensor for this type, so
     * this quietly no-ops instead of registering against a null sensor.
     */
    private fun registerRotationSensor() {
        val manager = sensorManager ?: return
        val rotationSensor = manager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)

        if (rotationSensor == null) {
            Log.w(TAG, "No TYPE_ROTATION_VECTOR sensor available on this device")
            return
        }

        manager.registerListener(rotationSensorListener, rotationSensor, SensorManager.SENSOR_DELAY_GAME)
    }

    private fun unregisterRotationSensor() {
        sensorManager?.unregisterListener(rotationSensorListener)
    }

    // --- Bluetooth connection lifecycle ---

    fun connectToDevice(device: BluetoothDevice) {
        if (!hasBluetoothConnectPermission()) return

        disconnectFromDevice()

        bleClient = TrailHudBleClient(
            context = this,
            onReady = {
                serviceScope.launch {
                    _uiState.value = _uiState.value.copy(isBleConnected = true)
                    promoteToForeground()
                    startBroadcasting()
                }
            },
            onDisconnected = {
                // The tilt alert is driven by STM32 edges, so a dropped link
                // would otherwise leave a continuous alert buzzing forever.
                vibrator.stop()
                serviceScope.launch {
                    _uiState.value = _uiState.value.copy(isBleConnected = false, rssiDbm = null)
                    stopBroadcasting()
                    demoteFromForeground()
                }
            },
            onRssiRead = { rssi ->
                serviceScope.launch {
                    _uiState.value = _uiState.value.copy(rssiDbm = rssi)
                }
            },
            onVibrationCommand = { command ->
                serviceScope.launch { vibrator.handle(command) }
            },
            onError = { error ->
                Log.e(TAG, error)
                serviceScope.launch {
                    _uiState.value = _uiState.value.copy(isBleConnected = false, rssiDbm = null)
                    stopBroadcasting()
                    demoteFromForeground()
                }
            }
        )

        bleClient?.connect(device)
    }

    fun disconnectFromDevice() {
        stopBleScan()
        stopBroadcasting()
        demoteFromForeground()
        _uiState.value = _uiState.value.copy(isBleConnected = false, rssiDbm = null)
        bleClient?.close()
        bleClient = null
    }

    private fun startBroadcasting() {
        stopBroadcasting()

        if (hasLocationPermission()) {
            lastLocation = locationManager?.getLastKnownLocation(LocationManager.GPS_PROVIDER)
                ?: locationManager?.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)

            val listener = object : LocationListener {
                override fun onLocationChanged(location: Location) {
                    lastLocation = location
                }
            }
            locationListener = listener

            locationManager?.requestLocationUpdates(
                LocationManager.GPS_PROVIDER,
                updateRateMs,
                1f,
                listener
            )
        }

        broadcastJob = serviceScope.launch(Dispatchers.IO) {
            while (isActive) {
                val locationPayload = lastLocation?.let { location ->
                    PhoneLocationPayload(
                        latitude = location.latitude,
                        longitude = location.longitude,
                        altitudeMeters = if (location.hasAltitude()) location.altitude else null,
                        horizontalAccuracyMeters = if (location.hasAccuracy()) location.accuracy else null
                    )
                }

                val rotationPayload = lastRotationQuaternion?.let { q ->
                    PhoneRotationPayload(
                        qw = q[0],
                        qx = q[1],
                        qy = q[2],
                        qz = q[3]
                    )
                }

                val packet = TrailHudPacket.encodePhonePose(
                    location = locationPayload,
                    rotation = rotationPayload
                )

                bleClient?.writeLine(packet)

                /*
                 * The phone can read BLE signal strength directly from the
                 * active GATT connection, so the STM32 does not need to send
                 * an RSSI packet back to the phone.
                 */
                bleClient?.readRemoteRssi()

                delay(updateRateMs)
            }
        }
    }

    private fun stopBroadcasting() {
        broadcastJob?.cancel()
        broadcastJob = null

        locationListener?.let { locationManager?.removeUpdates(it) }
        locationListener = null
    }

    // --- Scanning ---

    fun getPairedDevices(): List<BluetoothDevice> = discoveredDevices.values.toList()

    fun isBluetoothAdapterEnabled(): Boolean = bluetoothAdapter?.isEnabled == true

    @RequiresPermission(Manifest.permission.BLUETOOTH_SCAN)
    fun startBleScan() {
        if (!hasBluetoothScanPermission()) return
        if (bluetoothAdapter?.isEnabled != true) return

        discoveredDevices.clear()

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bluetoothAdapter?.bluetoothLeScanner?.startScan(null, settings, scanCallback)

        scanJob?.cancel()
        scanJob = serviceScope.launch {
            delay(4000L)
            stopBleScan()
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_SCAN)
    fun stopBleScan() {
        if (hasBluetoothScanPermission()) {
            bluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback)
        }
        scanJob?.cancel()
        scanJob = null
    }

    // --- Permissions ---

    private fun hasBluetoothScanPermission(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
                ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.BLUETOOTH_SCAN
                ) == PackageManager.PERMISSION_GRANTED
    }

    private fun hasLocationPermission(): Boolean {
        return ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED || ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_COARSE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED
    }

    // --- Foreground promotion / notification ---

    private fun promoteToForeground() {
        if (isForeground) return

        try {
            ServiceCompat.startForeground(
                this,
                NOTIFICATION_ID,
                buildNotification(connected = true),
                foregroundServiceTypes()
            )
            isForeground = true
        } catch (e: Exception) {
            // Best effort: if the OS refuses to let us start a foreground
            // service right now (e.g. Android 12+ background-start limits),
            // streaming continues normally as long as the app stays visible;
            // it just isn't protected from background throttling this time.
            Log.e(TAG, "Failed to promote Trail HUD service to the foreground", e)
        }
    }

    private fun demoteFromForeground() {
        if (!isForeground) return
        isForeground = false
        stopForeground(STOP_FOREGROUND_REMOVE)
    }

    private fun foregroundServiceTypes(): Int {
        var types = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
        if (hasLocationPermission()) {
            types = types or ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
        }
        return types
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return

        val channel = NotificationChannel(
            CHANNEL_ID,
            "Trail HUD connection",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Shows the status of the connection to your Trail HUD device"
        }

        getSystemService<NotificationManager>()?.createNotificationChannel(channel)
    }

    private fun buildNotification(connected: Boolean): Notification {
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Trail HUD")
            .setContentText(
                if (connected) "Connected — streaming location and orientation" else "Not connected"
            )
            .setSmallIcon(R.drawable.bluetooth)
            .setOngoing(true)
            .setContentIntent(contentIntent)
            .build()
    }

    companion object {
        private const val TAG = "TrailHudService"
        private const val CHANNEL_ID = "trail_hud_connection"
        private const val NOTIFICATION_ID = 1
    }
}
