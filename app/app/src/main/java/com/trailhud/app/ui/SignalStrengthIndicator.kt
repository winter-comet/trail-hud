package com.trailhud.app.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import com.trailhud.app.ui.theme.lightOlive
import com.trailhud.app.ui.theme.white

@Composable
fun SignalStrengthIndicator(
    isConnected: Boolean,
    rssiDbm: Int?,
    modifier: Modifier = Modifier,
    totalBars: Int = 28
) {
    var smoothedRssiDbm by remember { mutableStateOf<Float?>(null) }

    LaunchedEffect(isConnected, rssiDbm) {
        if (!isConnected || rssiDbm == null) {
            smoothedRssiDbm = null
        } else {
            val current = smoothedRssiDbm
            val target = rssiDbm.toFloat()

            smoothedRssiDbm = if (current == null) {
                target
            } else {
                current + ((target - current) * 0.35f)
            }
        }
    }

    val targetFilledBars = rssiToFilledBars(
        isConnected = isConnected,
        rssiDbm = smoothedRssiDbm,
        totalBars = totalBars
    )

    val animatedFilledBars by animateFloatAsState(
        targetValue = targetFilledBars,
        animationSpec = tween(durationMillis = 800),
        label = "signalStrengthBars"
    )

    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        repeat(totalBars) { index ->
            Box(
                modifier = Modifier
                    .width(6.dp)
                    .height(48.dp)
                    .clip(RoundedCornerShape(999.dp))
                    .background(
                        if (index < animatedFilledBars) lightOlive else white
                    )
            )
        }
    }
}

private fun rssiToFilledBars(
    isConnected: Boolean,
    rssiDbm: Float?,
    totalBars: Int
): Float {
    if (!isConnected || rssiDbm == null) return 0f

    val clampedRssi = rssiDbm.coerceIn(-100f, -45f)
    val normalized = (clampedRssi + 100f) / 55f

    return (normalized * totalBars)
        .coerceIn(0f, totalBars.toFloat())
}
