package com.trailhud.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.unit.dp
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

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
        val roundedHeadingInt = ceil(normalizedHeading.toDouble()).toInt() % 360

        // Fading boundaries
        val fadeStart = size.width * 0.30f
        val fadeEnd = size.width * 0.45f

        // A tick is only ever drawn when it's within half of tickAngleRange of
        // the heading, so walk that arc directly instead of scanning all 360
        // degrees (and re-deriving each one's wrapped distance) every frame.
        val halfTickRange = (tickAngleRange / 2f).toInt()
        for (rawDeg in (roundedHeadingInt - halfTickRange + 1) until (roundedHeadingInt + halfTickRange)) {
            val deg = ((rawDeg % 360) + 360) % 360
            val diff = (rawDeg - roundedHeadingInt).toFloat()
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
