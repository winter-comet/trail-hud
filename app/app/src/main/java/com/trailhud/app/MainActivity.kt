package com.trailhud.app

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalInspectionMode
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import androidx.core.content.getSystemService
import androidx.core.view.WindowCompat
import androidx.lifecycle.lifecycleScope
import com.trailhud.app.ui.theme.TrailHUDTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.IOException
import java.util.UUID
import kotlin.math.*

// --- UI constants ---
val font = FontFamily.Monospace
val titleTextSize = 32.dp
val bodyTextSize = 24.dp
val iconRipplePadding = 6.dp
val smallBorderRadius = 6.dp
val borderWidth = 2.dp

// --- Universal colors ---
val lightOlive = Color(0xFF8FA380)
val darkOlive = Color(0xFF829373)
val lightBlack = Color(0x60000000)
val darkBlack = Color(0xFF000000)
val white = Color(0xFFFFFFFF)

class MainActivity : ComponentActivity() {

    // --- Bluetooth & sensors state ---
    private val sppUuid: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private var bluetoothSocket: BluetoothSocket? = null
    private var broadcastJob: Job? = null

    private var lastLocation: Location? = null
    private var lastRotation: FloatArray? = null

    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        getSystemService<BluetoothManager>()?.adapter
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)

        setContent {
            TrailHUDTheme {
                MainScreen(
                    onRequestBluetooth = { requestBluetoothPermissions() },
                    getPairedDevices = { getPairedDevices() },
                    onConnect = { device -> connectToDevice(device) },
                    onPickModel = { pickModelLauncher.launch("*/*") }
                )
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        broadcastJob?.cancel()
        bluetoothSocket?.close()
    }

    // --- Bluetooth & data logic ---
    private fun connectToDevice(device: BluetoothDevice) {
        if (ActivityCompat.checkSelfPermission(
                this, Manifest.permission.BLUETOOTH_CONNECT
            ) != PackageManager.PERMISSION_GRANTED
        ) return

        lifecycleScope.launch(Dispatchers.IO) {
            try {
                bluetoothSocket?.close()
                bluetoothSocket = device.createRfcommSocketToServiceRecord(sppUuid)
                bluetoothSocket?.connect()
                launch(Dispatchers.Main) { startBroadcasting() }
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }
    }

    private fun startBroadcasting() {
        val sensorManager = getSystemService<SensorManager>()
        val locationManager = getSystemService<LocationManager>()

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
        }

        sensorManager?.registerListener(
            sensorListener,
            sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR),
            SensorManager.SENSOR_DELAY_UI
        )

        if (ActivityCompat.checkSelfPermission(
                this, Manifest.permission.ACCESS_FINE_LOCATION
            ) == PackageManager.PERMISSION_GRANTED
        ) {
            locationManager?.requestLocationUpdates(
                LocationManager.GPS_PROVIDER, 1000L, 1f, locationListener
            )
        }

        broadcastJob?.cancel()
        broadcastJob = lifecycleScope.launch(Dispatchers.IO) {
            while (isActive) {
                val locStr = lastLocation?.let { "LOC:${it.latitude},${it.longitude}" } ?: "LOC:unknown"
                val rotStr = lastRotation?.let { "ROT:${it.joinToString(",")}" } ?: "ROT:unknown"
                sendData("$locStr|$rotStr")
                delay(500)
            }
        }
    }

    private fun sendData(data: String) {
        try {
            bluetoothSocket?.outputStream?.write((data + "\n").toByteArray())
        } catch (e: IOException) {
            // TODO: Handle lost connection
        }
    }

    private fun getPairedDevices(): List<BluetoothDevice> {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(
                    this, Manifest.permission.BLUETOOTH_CONNECT
                ) != PackageManager.PERMISSION_GRANTED
            ) return emptyList()
        }
        return bluetoothAdapter?.bondedDevices?.toList() ?: emptyList()
    }

    // --- Permissions logic ---
    private fun enableBluetoothAndScan() {
        if (bluetoothAdapter?.isEnabled == false) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            enableBluetoothLauncher.launch(enableBtIntent)
        }
    }

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
}

// --- Composables ---
@Composable
fun MainScreen(
    modifier: Modifier = Modifier,
    onRequestBluetooth: () -> Unit = {},
    getPairedDevices: () -> List<BluetoothDevice> = { emptyList() },
    onConnect: (BluetoothDevice) -> Unit = {},
    onPickModel: () -> Unit = {}
) {
    // --- App state ---
    var rawHeading by remember { mutableFloatStateOf(0f) }
    var menuExpanded by remember { mutableStateOf(false) }
    var showDeviceDialog by remember { mutableStateOf(false) }
    var pairedDevices by remember { mutableStateOf<List<BluetoothDevice>>(emptyList()) }

    val heading by animateFloatAsState(
        targetValue = rawHeading,
        animationSpec = spring(
            stiffness = Spring.StiffnessHigh,
            visibilityThreshold = 0.01f
        ),
        label = "heading"
    )

    val context = LocalContext.current
    val isPreview = LocalInspectionMode.current

    // --- Sensors logic ---
    if (!isPreview) {
        DisposableEffect(Unit) {
            val sensorManager = context.getSystemService<SensorManager>()
            val rotationSensor = sensorManager?.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)
            var lastTarget = 0f

            val listener = object : SensorEventListener {
                override fun onSensorChanged(event: SensorEvent?) {
                    if (event?.sensor?.type == Sensor.TYPE_ROTATION_VECTOR) {
                        val rotationMatrix = FloatArray(9)
                        SensorManager.getRotationMatrixFromVector(rotationMatrix, event.values)
                        val orientationValues = FloatArray(3)
                        SensorManager.getOrientation(rotationMatrix, orientationValues)

                        var azimuth = (orientationValues[0] * (180.0 / PI)).toFloat()
                        if (azimuth < 0) azimuth += 360f

                        var diff = azimuth - (lastTarget % 360f)
                        while (diff > 180f) diff -= 360f
                        while (diff < -180f) diff += 360f

                        lastTarget += diff
                        rawHeading = lastTarget
                    }
                }
                override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
            }

            sensorManager?.registerListener(listener, rotationSensor, SensorManager.SENSOR_DELAY_GAME)
            onDispose { sensorManager?.unregisterListener(listener) }
        }
    }

    // --- GUI layout ---
    Column(
        modifier = modifier
            .fillMaxSize()
            .background(lightOlive)
            .statusBarsPadding(),
    ) {

        // --- Header ---
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(32.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = "TRAIL-APP",
                fontSize = (titleTextSize.value - 4).sp,
                fontFamily = font,
                color = darkBlack,
                letterSpacing = 2.sp,
                modifier = Modifier.weight(1f),
            )
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

        // --- Body ---
        Column(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(horizontal = 32.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {

            // Compass area
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(1f)
                    .border(BorderStroke(borderWidth, darkBlack), RoundedCornerShape(smallBorderRadius)),
                contentAlignment = Alignment.Center
            ) {
                ArcCompass(heading = heading, color = darkBlack)

                Text(
                    text = "${(ceil(((heading % 360f + 360f) % 360f).toDouble()).toInt() % 360)}°",
                    fontFamily = font,
                    fontSize = (titleTextSize.value + 4).sp,
                    fontWeight = FontWeight.Bold,
                    color = darkBlack,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 24.dp)
                )
            }

            Spacer(modifier = Modifier.height(32.dp))

            // Placeholder view
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(160.dp)
                    .background(darkBlack, RoundedCornerShape(smallBorderRadius))
                    .border(BorderStroke(borderWidth, darkBlack), RoundedCornerShape(smallBorderRadius))
            )
        }

        // --- Controls ---
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 32.dp, vertical = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {

            // Bluetooth button
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

            // Model import button
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
                onDismiss = { showDeviceDialog = false }
            )
        }

        Spacer(modifier = Modifier.height(16.dp).navigationBarsPadding())
    }
}

@Composable
fun ArcCompass(
    heading: Float,
    modifier: Modifier = Modifier,
    color: Color = Color.Black
) {
    // --- Compass arc ---
    Canvas(modifier = modifier.fillMaxSize()) {
        val arcCenter = Offset(size.width / 2, size.height * 2.8f)
        val radius = size.height * 2.2f
        val tickAngleRange = 90f

        val normalizedHeading = (heading % 360f + 360f) % 360f
        val roundedHeading = ceil(normalizedHeading.toDouble()).toFloat() % 360f

        // Fading boundaries
        val fadeStart = size.width * 0.35f
        val fadeEnd = size.width * 0.75f

        for (deg in 0 until 360 step 1) {
            var diff = deg - roundedHeading
            while (diff > 180) diff -= 360
            while (diff < -180) diff += 360

            if (abs(diff) < tickAngleRange / 2) {
                val angleRad = (diff - 90) * PI / 180.0

                // Use the horizontal position of the tick to determine alpha
                val tickMidRadius = radius + (size.height * 0.125f)
                val xPos = arcCenter.x + tickMidRadius * cos(angleRad).toFloat()
                val distFromCenter = abs(xPos - size.width / 2)

                val alpha = when {
                    distFromCenter <= fadeStart -> 1f
                    distFromCenter >= fadeEnd -> 0f
                    else -> 1f - (distFromCenter - fadeStart) / (fadeEnd - fadeStart)
                }

                if (alpha <= 0f) continue

                val baseLength = size.height * 0.25f
                val isMajorAxis = deg % 90 == 0
                val isTenDegree = deg % 10 == 0

                val tickLength = when {
                    isMajorAxis -> baseLength
                    isTenDegree -> baseLength * 0.7f
                    else -> baseLength * 0.5f
                }

                val weight = when {
                    isMajorAxis -> 5.dp.toPx()
                    isTenDegree -> 4.dp.toPx()
                    else -> 2.dp.toPx()
                }

                val start = Offset(
                    arcCenter.x + radius * cos(angleRad).toFloat(),
                    arcCenter.y + radius * sin(angleRad).toFloat()
                )
                val end = Offset(
                    arcCenter.x + (radius + tickLength) * cos(angleRad).toFloat(),
                    arcCenter.y + (radius + tickLength) * sin(angleRad).toFloat()
                )

                drawLine(
                    color = color.copy(alpha = alpha),
                    start = start,
                    end = end,
                    strokeWidth = weight,
                    cap = StrokeCap.Round
                )
            }
        }

        // Heading indicator
        val indicatorY = arcCenter.y - radius
        val edgeLength = 20.dp.toPx()
        val tipY = indicatorY + 6.dp.toPx()

        val trianglePath = Path().apply {
            val tipX = size.width / 2
            val triangleHeight = edgeLength * (sqrt(3.0) / 2.0).toFloat()

            moveTo(tipX, tipY)
            lineTo(tipX - edgeLength / 2, tipY + triangleHeight)
            lineTo(tipX + edgeLength / 2, tipY + triangleHeight)
            close()
        }
        drawPath(path = trianglePath, color = color)
    }
}

@Composable
fun BluetoothDeviceDialog(
    devices: List<BluetoothDevice>,
    onDeviceSelected: (BluetoothDevice) -> Unit,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    val menuTextSize = 12.dp

    // Bluetooth devices select
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
                    fontSize = (bodyTextSize.value).sp,
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
                        fontSize = (menuTextSize.value).sp,
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
                                    fontSize = (menuTextSize.value).sp,
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
                    fontSize = (menuTextSize.value).sp,
                    letterSpacing = 1.sp
                )
            }
        }
    )
}

@Preview(showBackground = true)
@Composable
fun MainScreenPreview() {
    TrailHUDTheme {
        MainScreen()
    }
}