package com.trailhud.app.protocol

import java.util.Locale

data class PhoneLocationPayload(
    val latitude: Double,
    val longitude: Double,
    val altitudeMeters: Double?,
    val horizontalAccuracyMeters: Float?
)

data class PhoneRotationPayload(
    val qw: Float,
    val qx: Float,
    val qy: Float,
    val qz: Float
)

/**
 * Vibration alerts the STM32 raises from its gyroscope tilt reading.
 *
 * ONCE marks crossing the warning threshold, START crossing the critical one,
 * and STOP the return back under the warning threshold. The firmware owns the
 * hysteresis between the two thresholds, so the phone just follows what it is
 * told rather than tracking the angle itself.
 */
enum class VibrationCommand {
    ONCE,
    START,
    STOP
}

object TrailHudPacket {
    const val DEFAULT_UPDATE_RATE_SECONDS = 0.5
    const val STM32_PING_PACKET = "trailhud:ping"
    const val PHONE_PING_REPLY_PACKET = "trailhud:pong"

    const val STM32_VIBRATE_ONCE_PACKET = "trailhud:vibrate:once"
    const val STM32_VIBRATE_START_PACKET = "trailhud:vibrate:start"
    const val STM32_VIBRATE_STOP_PACKET = "trailhud:vibrate:stop"

    /**
     * Maps one received line onto a vibration command, or null when the line
     * is not one. The STM32 only sends these on a threshold crossing, so a
     * command is an edge and not a level: START is not repeated while the tilt
     * stays critical, and STOP is the only thing that ends it.
     */
    fun parseVibrationCommand(line: String): VibrationCommand? = when (line) {
        STM32_VIBRATE_ONCE_PACKET -> VibrationCommand.ONCE
        STM32_VIBRATE_START_PACKET -> VibrationCommand.START
        STM32_VIBRATE_STOP_PACKET -> VibrationCommand.STOP
        else -> null
    }

    fun encodePhonePose(
        location: PhoneLocationPayload?,
        rotation: PhoneRotationPayload?
    ): String {
        val latitude = location?.latitude ?: 0.0
        val longitude = location?.longitude ?: 0.0
        val altitude = location?.altitudeMeters ?: 0.0
        val horizontalAccuracy = location?.horizontalAccuracyMeters?.toDouble() ?: 0.0

        val qw = rotation?.qw ?: 1.0f
        val qx = rotation?.qx ?: 0.0f
        val qy = rotation?.qy ?: 0.0f
        val qz = rotation?.qz ?: 0.0f

        return String.format(
            Locale.US,
            "[%.6f,%.6f,%.2f,%.2f;%.5f,%.5f,%.5f,%.5f]",
            latitude,
            longitude,
            altitude,
            horizontalAccuracy,
            qw,
            qx,
            qy,
            qz
        )
    }
}
