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
    const val DEFAULT_UPDATE_RATE_MS = DEFAULT_UPDATE_RATE_SECONDS * 1000L

    /**
     * Newline-framed phone pose packet sent through the HM-10 UART bridge.
     *
     * Format:
     *   [<lat>,<lon>,<alt-m>,<hacc-m>;<qw>,<qx>,<qy>,<qz>]
     *
     * Example:
     *   [11.2233440,55.6677880,na,1.23;0.909090,0.090909,-0.001122,0.001122]
     *
     * Missing values are encoded as "na" so the field positions stay stable.
     * Future extensions should be added as separate encoder functions rather
     * than changing this base pose frame.
     */
    fun encodePhonePose(
        location: PhoneLocationPayload?,
        rotation: PhoneRotationPayload?
    ): String {
        return "[${encodeLocation(location)};${encodeRotation(rotation)}]"
    }

    private fun encodeLocation(location: PhoneLocationPayload?): String {
        if (location == null) {
            return listOf("na", "na", "na", "na").joinToString(",")
        }

        val altitude = location.altitudeMeters?.let { formatDouble(it, 2) } ?: "na"
        val accuracy = location.horizontalAccuracyMeters?.let { formatFloat(it, 2) } ?: "na"

        return listOf(
            formatDouble(location.latitude, 7),
            formatDouble(location.longitude, 7),
            altitude,
            accuracy
        ).joinToString(",")
    }

    private fun encodeRotation(rotation: PhoneRotationPayload?): String {
        if (rotation == null) {
            return listOf("na", "na", "na", "na").joinToString(",")
        }

        return listOf(
            formatFloat(rotation.qw, 6),
            formatFloat(rotation.qx, 6),
            formatFloat(rotation.qy, 6),
            formatFloat(rotation.qz, 6)
        ).joinToString(",")
    }

    private fun formatDouble(value: Double, decimals: Int): String =
        String.format(Locale.US, "%.${decimals}f", value)

    private fun formatFloat(value: Float, decimals: Int): String =
        String.format(Locale.US, "%.${decimals}f", value)
}
