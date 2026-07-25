package com.trailhud.app

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresPermission
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.app.ActivityCompat
import androidx.core.view.WindowCompat
import com.trailhud.app.service.TrailHudService
import com.trailhud.app.service.TrailHudUiState
import com.trailhud.app.ui.BluetoothButtonResult
import com.trailhud.app.ui.MainScreen
import com.trailhud.app.ui.theme.TrailHUDTheme

class MainActivity : ComponentActivity() {

    // --- Foreground service connection ---
    private var trailHudService by mutableStateOf<TrailHudService?>(null)

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            trailHudService = (binder as? TrailHudService.LocalBinder)?.getService()
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            trailHudService = null
        }
    }

    // --- Activity launchers ---
    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { /* Bluetooth enabled */ }

    private val pickModelLauncher = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri ->
        // TODO: Handle the selected file URI
    }

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.values.all { it }) {
            enableBluetoothAndScan()
        }
    }

    private fun handleBluetoothButtonPress(): BluetoothButtonResult {
        val service = trailHudService ?: return BluetoothButtonResult.DISCONNECTED

        if (service.uiState.value.isBleConnected) {
            service.disconnectFromDevice()
            return BluetoothButtonResult.DISCONNECTED
        }

        requestBluetoothPermissions()
        return BluetoothButtonResult.SHOW_DEVICE_PICKER
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)

        bindService(
            Intent(this, TrailHudService::class.java),
            serviceConnection,
            Context.BIND_AUTO_CREATE
        )

        setContent {
            TrailHUDTheme {
                val service = trailHudService
                val uiState = if (service != null) {
                    service.uiState.collectAsState().value
                } else {
                    TrailHudUiState()
                }

                MainScreen(
                    isBleConnected = uiState.isBleConnected,
                    currentRssiDbm = uiState.rssiDbm,
                    rawHeadingDegrees = uiState.rawHeadingDegrees,
                    onRequestBluetooth = { handleBluetoothButtonPress() },
                    getPairedDevices = { service?.getPairedDevices() ?: emptyList() },
                    onConnect = { device -> service?.connectToDevice(device) },
                    onPickModel = { pickModelLauncher.launch("*/*") }
                )
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unbindService(serviceConnection)
    }

    // --- Permissions logic ---
    @RequiresPermission(Manifest.permission.BLUETOOTH_SCAN)
    private fun enableBluetoothAndScan() {
        val service = trailHudService ?: return

        if (!service.isBluetoothAdapterEnabled()) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            enableBluetoothLauncher.launch(enableBtIntent)
        } else {
            service.startBleScan()
        }
    }

    private fun requestBluetoothPermissions() {
        val basePermissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        } else {
            arrayOf(
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN,
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        }

        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            basePermissions + Manifest.permission.POST_NOTIFICATIONS
        } else {
            basePermissions
        }

        val missing = permissions.filter {
            ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missing.isEmpty()) {
            enableBluetoothAndScan()
        } else {
            requestPermissionLauncher.launch(permissions)
        }
    }
}
