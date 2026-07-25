package com.trailhud.app.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable

/**
 * TrailHUD draws its entire UI from the fixed palette in Color.kt rather
 * than Material3's color scheme (no Composable in the app reads
 * MaterialTheme.colorScheme), so this only needs to supply typography -
 * not a light/dark/dynamic scheme that would otherwise go unused.
 */
@Composable
fun TrailHUDTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        typography = Typography,
        content = content
    )
}
