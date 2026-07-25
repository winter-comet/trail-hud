package com.trailhud.app.ui

import android.bluetooth.BluetoothDevice
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.BorderStroke
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
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.trailhud.app.R
import com.trailhud.app.ui.theme.*
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.ceil

/**
 * Result of a tap on the Bluetooth control button: either the app just
 * disconnected an active link, or it wants the device picker shown.
 */
enum class BluetoothButtonResult {
    DISCONNECTED,
    SHOW_DEVICE_PICKER
}

@Composable
fun MainScreen(
    modifier: Modifier = Modifier,
    isBleConnected: Boolean = false,
    currentRssiDbm: Int? = null,
    rawHeadingDegrees: Float = 0f,
    onRequestBluetooth: () -> BluetoothButtonResult = { BluetoothButtonResult.SHOW_DEVICE_PICKER },
    getPairedDevices: () -> List<BluetoothDevice> = { emptyList() },
    onConnect: (BluetoothDevice) -> Unit = {},
    onPickModel: () -> Unit = {}
) {
    // --- App state ---
    var showDeviceDialog by remember { mutableStateOf(false) }
    var pairedDevices by remember { mutableStateOf<List<BluetoothDevice>>(emptyList()) }

    val heading by animateFloatAsState(
        targetValue = rawHeadingDegrees,
        animationSpec = spring(
            stiffness = Spring.StiffnessLow,
            visibilityThreshold = 0.01f
        ),
        label = "heading"
    )

    val scope = rememberCoroutineScope()

    // --- GUI layout ---
    Box {
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
                                onClick = { },
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

                // Compass area (Liquid Glass style)
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .aspectRatio(1f)
                        .shadow(
                            elevation = 20.dp,
                            shape = RoundedCornerShape(largeBorderRadius),
                            clip = false,
                            ambientColor = Color.Black.copy(alpha = 0.15f),
                            spotColor = Color.Black.copy(alpha = 0.25f)
                        )
                        .background(
                            brush = Brush.verticalGradient(
                                colors = listOf(
                                    lightOlive,
                                    lightOlive
                                )
                            ),
                            shape = RoundedCornerShape(largeBorderRadius)
                        )
                        .border(
                            width = 1.dp,
                            brush = Brush.linearGradient(
                                colors = listOf(
                                    Color.White.copy(alpha = 0.4f),
                                    Color.White.copy(alpha = 0.1f),
                                    Color.Transparent
                                ),
                                start = Offset(0f, 0f),
                                end = Offset(1000f, 1000f)
                            ),
                            shape = RoundedCornerShape(largeBorderRadius)
                        ),
                    contentAlignment = Alignment.Center
                ) {
                    // Section Title & Icon
                    Row(
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .padding(start = 16.dp, top = 16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Icon(
                            painter = painterResource(id = R.drawable.compass),
                            contentDescription = null,
                            tint = darkBlack,
                            modifier = Modifier.size(24.dp)
                        )
                        Text(
                            text = "COMPASS",
                            fontSize = subtitleTextSize.value.sp,
                            fontFamily = font,
                            color = darkBlack,
                            letterSpacing = 1.sp
                        )
                    }

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

                // Connection status
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(160.dp)
                        .background(darkBlack, RoundedCornerShape(largeBorderRadius))
                        .border(
                            BorderStroke(borderWidth, darkBlack),
                            RoundedCornerShape(largeBorderRadius)
                        )
                ) {
                    // Section Title & Icon
                    Row(
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .padding(start = 16.dp, top = 16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Icon(
                            painter = painterResource(id = R.drawable.cell_tower),
                            contentDescription = null,
                            tint = white,
                            modifier = Modifier.size(24.dp)
                        )
                        Text(
                            text = "CONNECTION",
                            fontSize = subtitleTextSize.value.sp,
                            fontFamily = font,
                            color = white,
                            letterSpacing = 1.sp
                        )
                    }

                    SignalStrengthIndicator(
                        isConnected = isBleConnected,
                        rssiDbm = currentRssiDbm,
                        modifier = Modifier
                            .align(Alignment.Center)
                            .fillMaxWidth()
                            .padding(horizontal = 20.dp)
                    )
                }
            }

            // --- Controls ---
            BoxWithConstraints(
                modifier = Modifier
                    .fillMaxWidth()
                    .navigationBarsPadding()
                    .padding(start = 32.dp, end = 32.dp, bottom = 32.dp)
            ) {
                val controlButtonSize = 64.dp
                val controlButtonGap = 16.dp
                val expandedBluetoothWidth = maxWidth - controlButtonSize - controlButtonGap

                val bluetoothButtonWidth by animateDpAsState(
                    targetValue = if (isBleConnected) expandedBluetoothWidth else controlButtonSize,
                    animationSpec = spring(
                        stiffness = Spring.StiffnessMediumLow,
                        dampingRatio = Spring.DampingRatioNoBouncy
                    ),
                    label = "bluetoothButtonWidth"
                )

                val disconnectTextAlpha by animateFloatAsState(
                    targetValue = if (isBleConnected) 1f else 0f,
                    animationSpec = spring(
                        stiffness = Spring.StiffnessMediumLow,
                        dampingRatio = Spring.DampingRatioNoBouncy
                    ),
                    label = "disconnectTextAlpha"
                )

                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(controlButtonSize)
                ) {

                    // Bluetooth button
                    Box(
                        modifier = Modifier
                            .align(Alignment.CenterStart)
                            .width(bluetoothButtonWidth)
                            .height(controlButtonSize)
                            .border(BorderStroke(borderWidth, darkOlive), CircleShape)
                            .clip(CircleShape)
                            .clickable(
                                onClick = {
                                    when (onRequestBluetooth()) {
                                        BluetoothButtonResult.SHOW_DEVICE_PICKER -> {
                                            pairedDevices = getPairedDevices()
                                            showDeviceDialog = true
                                            scope.launch {
                                                delay(4200L)
                                                pairedDevices = getPairedDevices()
                                            }
                                        }

                                        BluetoothButtonResult.DISCONNECTED -> {
                                            showDeviceDialog = false
                                            pairedDevices = emptyList()
                                        }
                                    }
                                },
                                indication = ripple(bounded = true, color = lightBlack),
                                interactionSource = remember { MutableInteractionSource() },
                            ),
                    ) {
                        Box(
                            modifier = Modifier
                                .size(controlButtonSize)
                                .align(Alignment.CenterStart),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                painter = painterResource(id = R.drawable.bluetooth),
                                contentDescription = if (isBleConnected) "Disconnect" else "Connect",
                                tint = darkBlack,
                                modifier = Modifier.size(32.dp)
                            )
                        }

                        Text(
                            text = "DISCONNECT",
                            fontFamily = font,
                            fontSize = subtitleTextSize.value.sp,
                            fontWeight = FontWeight.Bold,
                            color = darkBlack,
                            letterSpacing = 1.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Clip,
                            modifier = Modifier
                                .align(Alignment.CenterStart)
                                .padding(start = controlButtonSize + 12.dp, end = 20.dp)
                                .alpha(disconnectTextAlpha)
                        )
                    }

                    // Model import button
                    Box(
                        modifier = Modifier
                            .align(Alignment.CenterEnd)
                            .size(controlButtonSize)
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
            }
        }
    }

    BluetoothDeviceDialog(
        visible = showDeviceDialog,
        devices = pairedDevices,
        onDeviceSelected = { device ->
            onConnect(device)
            showDeviceDialog = false
        },
        onDismiss = { showDeviceDialog = false }
    )
}

@Preview(showBackground = true)
@Composable
fun MainScreenPreview() {
    TrailHUDTheme {
        MainScreen()
    }
}
