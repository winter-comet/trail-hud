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

object TrailHudPacket {
    const val DEFAULT_UPDATE_RATE_SECONDS = 1L
    const val STM32_PING_PACKET = "trailhud:ping"
    const val PHONE_PING_REPLY_PACKET = "trailhud:pong"

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
