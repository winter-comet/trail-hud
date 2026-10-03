# Trail-HUD

A car-style touchscreen display built on an STM32 board and paired with your phone over Bluetooth.

Like the central screen in a modern car, it brings several readings together on one 4.3" display.
Your phone sends its GPS position and its orientation (how it is being held), and the board draws
both on the screen. A motion sensor fixed to the vehicle measures how far the vehicle is tilted, and
the board shows that angle live. If the vehicle tilts too far, the board makes your phone vibrate as
a warning.

> **Note:** The touch input on the STM32H750B-DK board's screen is not handled yet.

## What it does

- **Shows your phone's orientation.** A 3D wireframe box on the screen turns in real time as you
  move the phone.
- **Shows your GPS position.** Latitude and longitude come from the phone's own location service.
- **Shows the vehicle's tilt.** A motion sensor fixed to the vehicle measures its tilt.
  The screen shows the angle as a rotating line, like an artificial horizon, with a numeric readout.
- **Warns you through your phone.** If the tilt exceeds 20° in either direction, the phone vibrates
  once. Beyond 25° it vibrates continuously until the tilt drops back below 20°.
- **Shows that the link is alive.** Two LEDs on the board blink: the top green one when data arrives
  from the phone, and the bottom one when the phone answers a connection test (see
  [Serial debug log](#serial-debug-log)).

Everything runs over a single Bluetooth Low Energy connection, and no internet connection is needed.
The phone sends its position and orientation about twice a second. The board sends a vibration
command back only when the tilt crosses a warning threshold.

## What you need

### Hardware

| Item                          | Used for                                                                          |
| ----------------------------- | --------------------------------------------------------------------------------- |
| STM32H750B-DK discovery board | Running the firmware and showing everything on its built-in 4.3" screen.          |
| HM-10 BLE module              | The Bluetooth link to the phone.                                                  |
| MPU-6050 module               | Measuring the vehicle's tilt. It must be the Keyestudio GY-521 variant.           |
| Breadboard or protoboard      | Holding the modules and the shared ground connection.                             |
| Jumper wires                  | Connecting the modules to the board. You need 8 male-male wires.                  |
| Micro-USB cable               | Powering the board and flashing the firmware.                                     |
| Android phone                 | Running the Trail HUD app. It must run Android 7.0 or newer.                      |

### Software

- [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html): provides the compiler,
  CMake, Ninja and the flashing tool in one install.
- [Android Studio](https://developer.android.com/studio): only needed if you want to build or modify
  the phone app yourself instead of installing a prebuilt APK.

## Wiring

Wire the two modules to the board as listed below, with the board unpowered. Power both modules from
the board, tie their grounds together on a shared ground rail, then connect the signal lines.

**Shared ground:** connect GND on the board (CN3 pin 6) to the ground rail on your breadboard.

**HM-10 Bluetooth module**

| Module pin | Connect to                | Purpose                                   |
| ---------- | ------------------------- | ----------------------------------------- |
| VCC        | CN3 pin 5 (5V)            | Power                                     |
| GND        | Common ground rail        | Ground                                    |
| TXD        | PB15 (CN2, D11)           | Data from phone to board                  |
| RXD        | PB14 (STMod+ P1, pin 9)   | Data from board to phone                  |
| STATE      | PG3 (CN6, D2)             | Tells the board when a phone is connected |
| EN         | PE3                       | Keeps the module enabled                  |

**MPU-6050 motion sensor**

| Module pin | Connect to                  | Purpose                      |
| ---------- | --------------------------- | ---------------------------- |
| VCC        | CN3 pin 4 (**3V3**, not 5V) | Power                        |
| GND        | Common ground rail          | Ground                       |
| SCL        | PD12 (CN2, D15)             | Sensor clock                 |
| SDA        | PD13 (CN2, D14)             | Sensor data                  |
| AD0        | Common ground rail          | Selects the sensor's address |

The sensor's `INT`, `XDA` and `XCL` pins are not used. Leave them unconnected.

### Mounting the tilt sensor

Fix the sensor firmly to the vehicle, standing upright on its edge, with the module's Y axis
(usually marked on the board) pointing up when the vehicle is level. The board measures the rotation
around the sensor's Z axis, which points out of the face of the module. If the face points along the
direction of travel, you measure side-to-side tilt. If it points sideways, you measure nose-up and
nose-down tilt.

## How to set up the project

Here are the instructions for a complete and working project setup, from start to finish.

### 1. Wire up the modules

Connect everything as described in [Wiring](#wiring) with the board unpowered.

### 2. Flash the board

Connect the board to your computer through the **ST-LINK** USB port. Then run these three commands
from the repository root.

Configure the build:

```bash
cmake --preset Debug -S firmware
```

Build the firmware:

```bash
cmake --build firmware/build/Debug
```

Flash it to the board:

```bash
STM32_Programmer_CLI -c port=SWD -w firmware/build/Debug/firmware.elf -rst
```

When the board starts, the screen shows a loading bar with four steps: display, Bluetooth module,
motion sensor and LEDs. If the bar fills completely and the main screen appears, the board is
healthy.

### 3. Install the phone app

Either install a prebuilt APK from the project's Releases page, or build and install the app
yourself. For the second option, connect your phone by USB with USB debugging turned on, then run:

```bash
cd app && ./gradlew installDebug
```

### 4. Connect

1. Turn on **Bluetooth** and **Location** on the phone. Location is needed to read the GPS position,
   and Android 11 and older also require it for any Bluetooth scan. The app has no internet access,
   so your position only goes to your own board, over Bluetooth.
2. Open **Trail HUD** and tap the Bluetooth button at the bottom left. The first time, Android asks
   for several permissions. Allow all of them, because scanning only starts once every permission
   is granted.
3. After a few seconds, nearby Bluetooth devices appear in a list. Pick **TRAIL-HUD**. If your
   module still has its factory name, it appears as **BT05** instead.
4. Once connected, the Bluetooth button expands to read **DISCONNECT**, and the Connection panel
   shows the signal strength as a row of bars.

### 5. Confirm it works

Within a second or two you should see:

- The phone wireframe on the right of the screen **turning as you rotate your phone**.
- Latitude and longitude below it. If the phone has no position yet, they read zero until it gets a
  GPS fix.
- The top green LED blinking each time a phone's data packet is received by the board.

Now test the tilt alert. Tilt the MPU-6050 sensor (or the whole assembly, if the sensor is fixed to
it) by more than 20° to either side. The angle on the screen follows your movement, updating about
twice a second, and the phone vibrates once. Beyond 25° it vibrates continuously until you bring the
tilt back below 20°. Alerts are only sent while the phone is connected.

## Reading the display

**Left side: vehicle tilt.** A line rotates around a center pivot, like an artificial horizon, and
the tilt angle is printed below it. The sign tells you which way the vehicle is tilted. Two short
marks on either side of the dial show level. The reading comes from the MPU-6050 sensor wired to the
board, so it reflects the tilt of the vehicle, not of your phone.

**Right side: your phone.** A wireframe box shows how the phone is currently oriented, with its GPS
coordinates below. When no phone is connected, this area reads `WAITING`.

The coordinates are shown exactly as the phone's location service reports them, so they are only as
precise as that fix. They can be off by a few meters in the open and by much more in cities, indoors
or inside a car. The screen does not show how accurate they are. The phone's own estimate is printed
as `hacc` (in meters) in the `PHONE DATA` view of the [serial debug log](#serial-debug-log).

## Common troubleshooting

| Symptom                                          | Most likely cause                                                                                                  |
| ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------ |
| Loading bar stops partway, red message at bottom | Wiring problem with the module named in the message                                                                |
| App can't find the device                        | Bluetooth or Location is off, a permission was denied, or the module is connected to another phone                 |
| Top LED blinks but the wireframe never appears   | Data arrives but is not in the expected format (see the `PHONE DATA` view below)                                   |
| Phone never vibrates                             | The phone is not connected, vibration is switched off or blocked by Do Not Disturb, or the tilt has not passed 20° |

### Serial debug log

The board also prints a running log over its USB port. To read it, open a serial terminal on the
ST-Link Virtual COM Port at **9600 baud, 8N1, no flow control**. Press `h` to list the available
views:

| Key | View             | What it shows                                                                             |
| --- | ---------------- | ----------------------------------------------------------------------------------------- |
| `w` | `WAITING`        | No periodic output (default)                                                              |
| `d` | `PHONE DATA`     | Each packet received from the phone, decoded                                              |
| `i` | `GYROSCOPE DATA` | Accelerometer and gyroscope readings from the MPU-6050                                    |
| `p` | `PINGS`          | Sends four test pings to the phone and reports each reply (the bottom LED blinks on each) |
| `h` | `HELP`           | Prints this command list                                                                  |

These views tell you whether packets are arriving and what they contain.

## Repository layout

```text
firmware/   STM32 firmware: display, sensor and Bluetooth link
app/        Android app: sends position and orientation, receives tilt alerts
```

## License

BSD 3-Clause. See [LICENSE](LICENSE).
