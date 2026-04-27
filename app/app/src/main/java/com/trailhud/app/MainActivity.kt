package com.trailhud.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
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

class MainActivity : ComponentActivity() {

    // Bluetooth adapter initialization
    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        val bluetoothManager = getSystemService(BluetoothManager::class.java)
        bluetoothManager?.adapter
    }

    // Launcher for system Bluetooth enable request
    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { /* Bluetooth enabled */ }

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
    getPairedDevices: () -> List<BluetoothDevice> = { emptyList() }
) {

    // Universal colors
    val lightOlive = Color(0xFF90A481)
    val darkOlive = Color(0xFF839474)
    val lightBlack = Color(0x60000000)
    val darkBlack = Color(0xFF000000)

    // Universal font settings
    val font = FontFamily.Monospace
    val titleTextSize = 32.dp
    val bodyFontSize = 18.dp

    val iconRipplePadding = 6.dp // Universal padding for touch area
    val smallBorderRadius = 6.dp // Universal small border radius

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
                // Dropdown menu
                DropdownMenu(
                    expanded = menuExpanded,
                    onDismissRequest = { menuExpanded = false },
                    modifier = Modifier.background(darkOlive)
                ) {
                    DropdownMenuItem(
                        text = {
                            Text(
                                "OPTION 1",
                                fontFamily = font,
                                color = darkBlack
                            )
                        },
                        onClick = {
                            /* TODO: Option 1 trigger */
                        }
                    )
                    DropdownMenuItem(
                        text = {
                            Text(
                                "OPTION 2",
                                fontFamily = font,
                                color = darkBlack
                            )
                        },
                        onClick = {
                            /* TODO: Option 2 trigger */
                        }
                    )
                }
            }
        }

        // Spacer to push buttons to bottom
        Spacer(modifier = Modifier.weight(1f))

        // Button for establishing a Bluetooth connection
        Button(
            onClick = {
                onRequestBluetooth()
                pairedDevices = getPairedDevices()
                showDeviceDialog = true
            },
            modifier = Modifier
                .fillMaxWidth()
                .height(60.dp)
                .padding(horizontal = 16.dp, vertical = 4.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = darkOlive,
            ),
            shape = RoundedCornerShape(smallBorderRadius),
        ) {
            Text(
                "CONNECT",
                fontSize = (bodyFontSize.value).sp,
                fontFamily = font,
                letterSpacing = 1.sp,
                color = darkBlack,
            )
        }

        // Button for importing a 3D model of the device to be displayed
        Button(
            onClick = { /* TODO: Handle button click */ },
            modifier = Modifier
                .fillMaxWidth()
                .height(60.dp)
                .padding(horizontal = 16.dp, vertical = 4.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = darkOlive,
            ),
            shape = RoundedCornerShape(smallBorderRadius), // Subtle rounding
        ) {
            Text(
                "IMPORT MODEL",
                fontSize = (bodyFontSize.value).sp,
                fontFamily = font,
                letterSpacing = 1.sp,
                color = darkBlack,
            )
        }

        if (showDeviceDialog) {
            BluetoothDeviceDialog(
                devices = pairedDevices,
                onDeviceSelected = { device ->
                    /* TODO: Code for establishing a connection */
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

    // Bluetooth pop-up
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text("SELECT A DEVICE", fontFamily = font, color = darkBlack)
        },
        text = {
            Column {
                if (devices.isEmpty()) {
                    Text("No paired devices found", fontFamily = font, color = darkBlack)
                } else {
                    devices.forEach { device ->
                        // Check for Bluetooth connect permission on Android 12+
                        val hasPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            ActivityCompat.checkSelfPermission(
                                context,
                                Manifest.permission.BLUETOOTH_CONNECT
                            ) == PackageManager.PERMISSION_GRANTED
                        } else true

                        if (hasPermission) {
                            TextButton(
                                onClick = { onDeviceSelected(device) },
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Text(
                                    device.name ?: "Unknown Device",
                                    fontFamily = font,
                                    color = darkBlack
                                )
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", fontFamily = font, color = darkBlack)
            }
        },
        containerColor = darkOlive
    )
}