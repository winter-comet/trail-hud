package com.trailhud.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.trailhud.app.ui.theme.TrailHUDTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Enable edge-to-edge and configure window for full screen
        enableEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)

        setContent {
            TrailHUDTheme {
                MainScreen()
            }
        }
    }
}

@Composable
fun MainScreen(modifier: Modifier = Modifier) {
    val lightOliveBackground = Color(0xFF90A481)
    val darkOliveBackground = Color(0xFF839474)
    val darkTextColor = Color(0xFF1A1A1A)
    val borderColor = Color(0xFF000000)

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(lightOliveBackground)
            .statusBarsPadding() // Padding for the status bar content
    ) {
        // Title row with menu icon
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(lightOliveBackground)
                .padding(20.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "TRAIL-HUD",
                fontSize = 28.sp,
                fontFamily = FontFamily.Monospace,
                color = darkTextColor,
                letterSpacing = 2.sp,
                modifier = Modifier.weight(1f)
            )

            IconButton(
                onClick = { /* Handle menu click */ },
                modifier = Modifier.size(32.dp)
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.menu),
                    contentDescription = "Menu",
                    tint = darkTextColor,
                    modifier = Modifier.size(48.dp)
                )
            }
        }

        // Spacer to push buttons to bottom
        Spacer(modifier = Modifier.weight(1f))

        // Stack of buttons
        Button(
            onClick = { /* Handle button 1 click */ },
            modifier = Modifier
                .fillMaxWidth()
                .height(60.dp)
                .padding(horizontal = 16.dp, vertical = 4.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = darkOliveBackground
            ),
            shape = RectangleShape
        ) {
            Text(
                "CONNECT",
                fontSize = 18.sp,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 1.sp,
                color = darkTextColor
            )
        }

        Button(
            onClick = { /* Handle button 2 click */ },
            modifier = Modifier
                .fillMaxWidth()
                .height(60.dp)
                .padding(horizontal = 16.dp, vertical = 4.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = darkOliveBackground
            ),
            shape = RectangleShape
        ) {
            Text(
                "IMPORT MODEL",
                fontSize = 18.sp,
                fontFamily = FontFamily.Monospace,
                letterSpacing = 1.sp,
                color = darkTextColor
            )
        }

        Spacer(modifier = Modifier
            .height(16.dp)
            .navigationBarsPadding() // Padding for the navigation bar
        )
    }
}

@Preview(showBackground = true)
@Composable
fun MainScreenPreview() {
    TrailHUDTheme {
        MainScreen()
    }
}