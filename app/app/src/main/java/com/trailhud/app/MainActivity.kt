package com.trailhud.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.trailhud.app.ui.theme.TrailHUDTheme
import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.TextButton
import androidx.compose.ui.platform.LocalContext
import androidx.core.app.ActivityCompat
import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.bluetooth.BluetoothSocket
import java.io.IOException
import java.util.UUID
import java.util.Timer
import java.util.TimerTask
import androidx.compose.foundation.border

class MainActivity : ComponentActivity() {

    private var bluetoothSocket: BluetoothSocket? = null
    private val sppUuid: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private var broadcastTimer: Timer? = null

    private var lastLocation: Location? = null
    private var lastRotation: FloatArray? = null

    private fun connectToDevice(device: BluetoothDevice) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            return
        }
        Thread {
            try {
                bluetoothSocket?.close()
                bluetoothSocket = device.createRfcommSocketToServiceRecord(sppUuid)
                bluetoothSocket?.connect()
                runOnUiThread { startBroadcasting() }
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }.start()
    }

    private fun startBroadcasting() {
        val sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        val locationManager = getSystemService(Context.LOCATION_SERVICE) as LocationManager

        val sensorListener = object : SensorEventListener {
            override fun onSensorChanged(event: SensorEvent?) {
                if (event?.sensor?.type == Sensor.TYPE_ROTATION_VECTOR) {
                    lastRotation = event.values
                }
            }
            override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
        }

        val locationListener = object : LocationListener {
            override fun onLocationChanged(location: Location) {
                lastLocation = location
            }
            override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
            override fun onProviderEnabled(provider: String) {}
            override fun onProviderDisabled(provider: String) {}
        }

        sensorManager.registerListener(sensorListener, sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR), SensorManager.SENSOR_DELAY_UI)
        
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000L, 1f, locationListener)
        }

        broadcastTimer?.cancel()
        broadcastTimer = Timer()
        broadcastTimer?.scheduleAtFixedRate(object : TimerTask() {
            override fun run() {
                val locStr = lastLocation?.let { "LOC:${it.latitude},${it.longitude}" } ?: "LOC:unknown"
                val rotStr = lastRotation?.let { "ROT:${it.joinToString(",")}" } ?: "ROT:unknown"
                sendData("$locStr|$rotStr")
            }
        }, 0, 500) // 500ms interval
    }

    private fun sendData(data: String) {
        Thread {
            try {
                bluetoothSocket?.outputStream?.write((data + "\n").toByteArray())
            } catch (e: IOException) {
                // Connection lost
            }
        }.start()
    }

    // Bluetooth adapter initialization
    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        val bluetoothManager = getSystemService(BluetoothManager::class.java)
        bluetoothManager?.adapter
    }

    // Launcher for system Bluetooth enable request
    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { /* Bluetooth enabled */ }

    // Launcher for model file selection
    private val pickModelLauncher = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri ->
        // Handle the selected file URI
    }

    // Launcher for runtime permission requests
    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.values.all { it }) {
            enableBluetoothAndScan()
        }
    }

    // Request system to enable Bluetooth if it's currently off
    private fun enableBluetoothAndScan() {
        if (bluetoothAdapter?.isEnabled == false) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            enableBluetoothLauncher.launch(enableBtIntent)
        }
    }

    // Determine and request necessary Bluetooth/Location permissions based on API level
    private fun requestBluetoothPermissions() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        }
        requestPermissionLauncher.launch(permissions)
    }

    // Retrieve list of already paired Bluetooth devices
    private fun getPairedDevices(): List<BluetoothDevice> {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                return emptyList()
            }
        }
        return bluetoothAdapter?.bondedDevices?.toList() ?: emptyList()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Enable edge-to-edge and configure window for full screen
        enableEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)

        setContent {
            TrailHUDTheme {
                MainScreen(
                    onRequestBluetooth = {
                        requestBluetoothPermissions()
                    },
                    getPairedDevices = {
                        getPairedDevices()
                    },
                    onConnect = { device ->
                        connectToDevice(device)
                    },
                    onPickModel = {
                        pickModelLauncher.launch("*/*") // Or a specific type like "application/octet-stream"
                    }
                )
            }
        }
    }
}

@Composable
fun MainScreen(
    modifier: Modifier = Modifier,
    onRequestBluetooth: () -> Unit = {},
    getPairedDevices: () -> List<BluetoothDevice> = { emptyList() },
    onConnect: (BluetoothDevice) -> Unit = {},
    onPickModel: () -> Unit = {}
) {

    // Universal colors
    val lightOlive = Color(0xFF8FA380)
    val darkOlive = Color(0xFF829373)
    val lightBlack = Color(0x60000000)
    val darkBlack = Color(0xFF000000)

    // Universal font settings
    val font = FontFamily.Monospace
    val titleTextSize = 32.dp

    val iconRipplePadding = 6.dp // Universal padding for touch area
    val smallBorderRadius = 6.dp // Universal small border radius
    val borderWidth = 2.dp

    var menuExpanded by remember { mutableStateOf(false) } // State for dropdown menu visibility
    var showDeviceDialog by remember { mutableStateOf(false) } // State for device selection dialog
    var pairedDevices by remember { mutableStateOf<List<BluetoothDevice>>(emptyList()) } // List of paired devices for the dialog

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(lightOlive)
            .statusBarsPadding(),
    ) {
        // Title row with menu icon
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(lightOlive)
                .padding(32.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            // Title text
            Text(
                text = "TRAIL-APP",
                fontSize = (titleTextSize.value - 4).sp,
                fontFamily = font,
                color = darkBlack,
                letterSpacing = 2.sp,
                modifier = Modifier.weight(1f),
            )
            // Menu icon with rounded square ripple effect and dropdown
            Box {
                Box(
                    modifier = Modifier
                        .size(titleTextSize + (iconRipplePadding * 2))
                        .clip(RoundedCornerShape(smallBorderRadius))
                        .clickable(
                            onClick = { menuExpanded = !menuExpanded },
                            indication = ripple(bounded = true, color = lightBlack),
                            interactionSource = remember { MutableInteractionSource() },
                        ),
                    contentAlignment = Alignment.Center,
                ) {
                    Icon(
                        painter = painterResource(id = R.drawable.menu),
                        contentDescription = "Menu",
                        tint = darkBlack,
                        modifier = Modifier.size(titleTextSize),
                    )
                }
            }
        }

        // Middle section with shape outlines
        Column(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(horizontal = 32.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            // Square outline
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(1f)
                    .border(BorderStroke(borderWidth, darkBlack), RoundedCornerShape(smallBorderRadius))
            )
            Spacer(modifier = Modifier.height(32.dp))
            // Rectangle outline
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(160.dp)
                    .background(darkBlack, RoundedCornerShape(smallBorderRadius))
                    .border(BorderStroke(borderWidth, darkBlack), RoundedCornerShape(smallBorderRadius))
            )
        }

        // Row for circular buttons at the bottom
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 32.dp, vertical = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Button for establishing a Bluetooth connection
            Box(
                modifier = Modifier
                    .size(64.dp)
                    .border(BorderStroke(borderWidth, darkOlive), CircleShape)
                    .clip(CircleShape)
                    .clickable(
                        onClick = {
                            onRequestBluetooth()
                            pairedDevices = getPairedDevices()
                            showDeviceDialog = true
                        },
                        indication = ripple(bounded = true, color = lightBlack),
                        interactionSource = remember { MutableInteractionSource() },
                    ),
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.bluetooth),
                    contentDescription = "Connect",
                    tint = darkBlack,
                    modifier = Modifier.size(32.dp)
                )
            }

            // Button for importing a 3D model of the device to be displayed
            Box(
                modifier = Modifier
                    .size(64.dp)
                    .border(BorderStroke(borderWidth, darkOlive), CircleShape)
                    .clip(CircleShape)
                    .clickable(
                        onClick = { onPickModel() },
                        indication = ripple(bounded = true, color = lightBlack),
                        interactionSource = remember { MutableInteractionSource() },
                    ),
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.cube),
                    contentDescription = "Import Model",
                    tint = darkBlack,
                    modifier = Modifier.size(32.dp)
                )
            }
        }

        if (showDeviceDialog) {
            BluetoothDeviceDialog(
                devices = pairedDevices,
                onDeviceSelected = { device ->
                    onConnect(device)
                    showDeviceDialog = false
                },
                onDismiss = { showDeviceDialog = false },
                font = font,
                darkBlack = darkBlack,
                darkOlive = darkOlive
            )
        }

        Spacer(
            modifier = Modifier
                .height(16.dp)
                .navigationBarsPadding(),
        )
    }
}

@Preview(showBackground = true)
@Composable
fun MainScreenPreview() {
    TrailHUDTheme {
        MainScreen()
    }
}

@Composable
fun BluetoothDeviceDialog(
    devices: List<BluetoothDevice>,
    onDeviceSelected: (BluetoothDevice) -> Unit,
    onDismiss: () -> Unit,
    font: FontFamily,
    darkBlack: Color,
    darkOlive: Color
) {
    val context = LocalContext.current
    val smallBorderRadius = 6.dp
    val borderWidth = 2.dp

    AlertDialog(
        onDismissRequest = onDismiss,
        properties = androidx.compose.ui.window.DialogProperties(usePlatformDefaultWidth = false),
        modifier = Modifier
            .fillMaxWidth()
            .padding(32.dp)
            .border(BorderStroke(borderWidth, darkBlack), RoundedCornerShape(smallBorderRadius)),
        shape = RoundedCornerShape(smallBorderRadius),
        containerColor = darkOlive,
        title = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    "BT-DEVICES",
                    fontFamily = font,
                    fontSize = 20.sp,
                    color = darkBlack,
                    letterSpacing = 2.sp
                )
                Spacer(modifier = Modifier.height(8.dp))
                Box(
                    modifier = Modifier
                        .fillMaxWidth(0.4f)
                        .height(borderWidth)
                        .background(darkBlack)
                )
            }
        },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                if (devices.isEmpty()) {
                    Text(
                        "NO PAIRED DEVICES",
                        fontFamily = font,
                        color = darkBlack.copy(alpha = 0.6f),
                        fontSize = 14.sp,
                        modifier = Modifier.align(Alignment.CenterHorizontally)
                    )
                } else {
                    devices.forEach { device ->
                        val hasPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            ActivityCompat.checkSelfPermission(
                                context,
                                Manifest.permission.BLUETOOTH_CONNECT
                            ) == PackageManager.PERMISSION_GRANTED
                        } else true

                        if (hasPermission) {
                            Box(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .border(
                                        BorderStroke(1.dp, darkBlack.copy(alpha = 0.2f)),
                                        RoundedCornerShape(smallBorderRadius)
                                    )
                                    .clip(RoundedCornerShape(smallBorderRadius))
                                    .clickable { onDeviceSelected(device) }
                                    .padding(12.dp)
                            ) {
                                Text(
                                    device.name?.uppercase() ?: "UNKNOWN",
                                    fontFamily = font,
                                    color = darkBlack,
                                    fontSize = 14.sp,
                                    letterSpacing = 1.sp
                                )
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text(
                    "CANCEL",
                    fontFamily = font,
                    color = darkBlack,
                    letterSpacing = 1.sp
                )
            }
        }
    )
}
