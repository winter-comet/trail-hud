package com.trailhud.app.service

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.core.content.getSystemService
import com.trailhud.app.protocol.VibrationCommand

/**
 * Plays the tilt alert vibrations the STM32 asks for.
 *
 * The firmware sends edges rather than a level - one packet when the tilt
 * crosses a threshold and one when it falls back - so this class holds no
 * angle of its own and simply starts, repeats, or cancels.
 *
 * "Continuous" is a repeating waveform rather than a single very long buzz:
 * the platform gives no unbounded one-shot, and a repeat keeps the alert
 * noticeable instead of fading into the background the way a constant motor
 * does. It runs until [stop] is called.
 */
class TrailHudVibrator(context: Context) {

    private val vibrator: Vibrator? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        context.getSystemService<VibratorManager>()?.defaultVibrator
    } else {
        @Suppress("DEPRECATION")
        context.getSystemService<Vibrator>()
    }

    private val isSupported: Boolean = vibrator?.hasVibrator() == true

    fun handle(command: VibrationCommand) {
        when (command) {
            VibrationCommand.ONCE -> vibrateOnce()
            VibrationCommand.START -> vibrateContinuously()
            VibrationCommand.STOP -> stop()
        }
    }

    private fun vibrateOnce() {
        val vibrator = vibrator ?: return
        if (!isSupported) return

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(
                VibrationEffect.createOneShot(SINGLE_PULSE_MS, VibrationEffect.DEFAULT_AMPLITUDE)
            )
        } else {
            @Suppress("DEPRECATION")
            vibrator.vibrate(SINGLE_PULSE_MS)
        }
    }

    private fun vibrateContinuously() {
        val vibrator = vibrator ?: return
        if (!isSupported) return

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(
                VibrationEffect.createWaveform(CONTINUOUS_PATTERN_MS, CONTINUOUS_REPEAT_INDEX)
            )
        } else {
            @Suppress("DEPRECATION")
            vibrator.vibrate(CONTINUOUS_PATTERN_MS, CONTINUOUS_REPEAT_INDEX)
        }
    }

    /**
     * Cancels whatever is playing. Safe to call when nothing is - which is why
     * the service can call it unconditionally on disconnect and teardown
     * without tracking whether an alert was actually running.
     */
    fun stop() {
        vibrator?.cancel()
    }

    private companion object {
        const val SINGLE_PULSE_MS = 250L

        // Wait, buzz, pause - repeated from index 0 so the buzz keeps cycling.
        val CONTINUOUS_PATTERN_MS = longArrayOf(0L, 600L, 400L)
        const val CONTINUOUS_REPEAT_INDEX = 0
    }
}
