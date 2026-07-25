package com.trailhud.app.ui

import android.Manifest
import android.bluetooth.BluetoothDevice
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import com.trailhud.app.R
import com.trailhud.app.ui.theme.*

private data class BleDeviceMenuItem(
    val address: String,
    val name: String,
    val device: BluetoothDevice
)

private data class ScrollbarMetrics(
    val isVisible: Boolean,
    val heightFraction: Float,
    val progress: Float
)

@Composable
fun BluetoothDeviceDialog(
    visible: Boolean,
    devices: List<BluetoothDevice>,
    onDeviceSelected: (BluetoothDevice) -> Unit,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    val menuTextSize = 12.dp
    val listState = rememberLazyListState()

    val hasPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        ActivityCompat.checkSelfPermission(
            context,
            Manifest.permission.BLUETOOTH_CONNECT
        ) == PackageManager.PERMISSION_GRANTED
    } else true

    // devices is already deduplicated by getPairedDevices(), so no need to
    // distinctBy again here.
    val visibleDevices = remember(devices, hasPermission) {
        if (!hasPermission) {
            emptyList()
        } else {
            devices.map { device ->
                BleDeviceMenuItem(
                    address = device.address,
                    name = device.name?.uppercase() ?: "UNKNOWN BLE DEVICE",
                    device = device
                )
            }
        }
    }

    val scrollbarMetrics by remember {
        derivedStateOf {
            val layoutInfo = listState.layoutInfo
            val totalItems = layoutInfo.totalItemsCount
            val visibleItems = layoutInfo.visibleItemsInfo.size

            if (totalItems <= visibleItems || totalItems == 0) {
                ScrollbarMetrics(
                    isVisible = false,
                    heightFraction = 1f,
                    progress = 0f
                )
            } else {
                val firstVisibleItem = layoutInfo.visibleItemsInfo.firstOrNull()
                val itemHeight = firstVisibleItem?.size?.coerceAtLeast(1) ?: 1
                val offsetProgress =
                    listState.firstVisibleItemScrollOffset.toFloat() / itemHeight.toFloat()

                val scrollProgress = (
                        (listState.firstVisibleItemIndex + offsetProgress) /
                                (totalItems - visibleItems).coerceAtLeast(1).toFloat()
                        ).coerceIn(0f, 1f)

                ScrollbarMetrics(
                    isVisible = true,
                    heightFraction = (visibleItems.toFloat() / totalItems.toFloat())
                        .coerceIn(0.18f, 1f),
                    progress = scrollProgress
                )
            }
        }
    }

    if (visible) {
        BackHandler(onBack = onDismiss)
    }

    Box(modifier = Modifier.fillMaxSize()) {
        AnimatedVisibility(
            visible = visible,
            enter = fadeIn(animationSpec = tween(durationMillis = 180)),
            exit = fadeOut(animationSpec = tween(durationMillis = 150))
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color(0x99D9D9D9))
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null,
                        onClick = onDismiss
                    )
            )
        }

        AnimatedVisibility(
            visible = visible,
            enter = fadeIn(animationSpec = tween(durationMillis = 180)) +
                    scaleIn(
                        animationSpec = tween(durationMillis = 240),
                        initialScale = 0.96f
                    ),
            exit = fadeOut(animationSpec = tween(durationMillis = 140)) +
                    scaleOut(
                        animationSpec = tween(durationMillis = 180),
                        targetScale = 0.98f
                    ),
            modifier = Modifier.fillMaxSize()
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 28.dp, vertical = 48.dp),
                contentAlignment = Alignment.Center
            ) {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .widthIn(max = 420.dp)
                        .heightIn(max = 470.dp)
                        .clickable(
                            interactionSource = remember { MutableInteractionSource() },
                            indication = null,
                            onClick = {}
                        ),
                    shape = RoundedCornerShape(30.dp),
                    color = Color.White,
                    shadowElevation = 16.dp,
                    tonalElevation = 0.dp,
                    border = BorderStroke(1.dp, Color.White)
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(22.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(12.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(42.dp)
                                    .clip(RoundedCornerShape(14.dp))
                                    .background(darkBlack.copy(alpha = 0.06f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    painter = painterResource(id = R.drawable.bluetooth),
                                    contentDescription = null,
                                    tint = darkBlack,
                                    modifier = Modifier.size(24.dp)
                                )
                            }

                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = "BLE-DEVICES",
                                    fontFamily = font,
                                    fontSize = bodyTextSize.value.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = darkBlack,
                                    letterSpacing = 1.5.sp
                                )

                                Text(
                                    text = "SELECT A MODULE TO CONNECT",
                                    fontFamily = font,
                                    fontSize = menuTextSize.value.sp,
                                    color = darkBlack.copy(alpha = 0.5f),
                                    letterSpacing = 0.8.sp
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(18.dp))

                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(1.dp)
                                .background(darkBlack.copy(alpha = 0.08f))
                        )

                        Spacer(modifier = Modifier.height(14.dp))

                        BoxWithConstraints(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(260.dp)
                        ) {
                            val thumbHeight = (maxHeight * scrollbarMetrics.heightFraction)
                                .coerceAtLeast(36.dp)
                            val thumbOffset = (maxHeight - thumbHeight) * scrollbarMetrics.progress

                            Row(modifier = Modifier.fillMaxSize()) {
                                LazyColumn(
                                    state = listState,
                                    modifier = Modifier
                                        .weight(1f)
                                        .fillMaxHeight()
                                        .padding(end = if (scrollbarMetrics.isVisible) 12.dp else 0.dp),
                                    verticalArrangement = Arrangement.spacedBy(10.dp),
                                    contentPadding = PaddingValues(vertical = 2.dp)
                                ) {
                                    if (visibleDevices.isEmpty()) {
                                        item {
                                            Box(
                                                modifier = Modifier
                                                    .fillMaxWidth()
                                                    .height(120.dp),
                                                contentAlignment = Alignment.Center
                                            ) {
                                                Text(
                                                    text = "NO BLE DEVICES",
                                                    fontFamily = font,
                                                    color = darkBlack.copy(alpha = 0.45f),
                                                    fontSize = menuTextSize.value.sp,
                                                    letterSpacing = 1.sp
                                                )
                                            }
                                        }
                                    } else {
                                        items(
                                            items = visibleDevices,
                                            key = { it.address }
                                        ) { item ->
                                            BleDeviceMenuRow(
                                                item = item,
                                                menuTextSize = menuTextSize,
                                                onDeviceSelected = onDeviceSelected
                                            )
                                        }
                                    }
                                }

                                if (scrollbarMetrics.isVisible) {
                                    Box(
                                        modifier = Modifier
                                            .fillMaxHeight()
                                            .width(4.dp)
                                            .clip(CircleShape)
                                            .background(darkBlack.copy(alpha = 0.08f))
                                    ) {
                                        Box(
                                            modifier = Modifier
                                                .offset(y = thumbOffset)
                                                .fillMaxWidth()
                                                .height(thumbHeight)
                                                .clip(CircleShape)
                                                .background(darkBlack.copy(alpha = 0.35f))
                                        )
                                    }
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(18.dp))

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.End
                        ) {
                            Box(
                                modifier = Modifier
                                    .clip(CircleShape)
                                    .background(darkBlack)
                                    .clickable { onDismiss() }
                                    .padding(horizontal = 18.dp, vertical = 10.dp)
                            ) {
                                Text(
                                    text = "CANCEL",
                                    fontFamily = font,
                                    color = white,
                                    fontSize = menuTextSize.value.sp,
                                    fontWeight = FontWeight.Bold,
                                    letterSpacing = 1.sp
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun BleDeviceMenuRow(
    item: BleDeviceMenuItem,
    menuTextSize: Dp,
    onDeviceSelected: (BluetoothDevice) -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 54.dp)
            .clip(RoundedCornerShape(18.dp))
            .background(Color(0xFFF6F6F6))
            .border(
                BorderStroke(1.dp, darkBlack.copy(alpha = 0.08f)),
                RoundedCornerShape(18.dp)
            )
            .clickable { onDeviceSelected(item.device) }
            .padding(horizontal = 14.dp, vertical = 12.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Box(
                modifier = Modifier
                    .size(32.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(darkBlack.copy(alpha = 0.06f)),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.bluetooth),
                    contentDescription = null,
                    tint = darkBlack,
                    modifier = Modifier.size(18.dp)
                )
            }

            Text(
                text = item.name,
                fontFamily = font,
                color = darkBlack,
                fontSize = menuTextSize.value.sp,
                letterSpacing = 1.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f)
            )
        }
    }
}
