package com.trailhud.app.ble

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

/**
 * Shared by [TrailHudBleClient] and [com.trailhud.app.service.TrailHudService],
 * both of which need to check BLUETOOTH_CONNECT before touching a GATT
 * connection but only hold a plain [Context], not an Activity.
 */
fun Context.hasBluetoothConnectPermission(): Boolean {
    return Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT
            ) == PackageManager.PERMISSION_GRANTED
}
