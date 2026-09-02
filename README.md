# Trail-HUD

A handlebar-mounted heads-up display that pairs an STM32 board with your phone over Bluetooth.

Your phone sends its position and how it is being held. The board draws both on a 4.3" screen and
watches its own tilt with an onboard motion sensor — and when you lean too far, it buzzes your
phone to tell you.

```
        ┌─────────────────────────┬────────────────────────────┐
        │  TRAIL-HUD              │        DEVICE RENDER       │
        │  ─────────              │   ┌────────────────────┐   │
        │   ┌─────────────────┐   │   │                    │   │
        │   │                 │   │   │      ┌────────┐    │   │
        │   │   ╲             │   │   │     ╱        ╱│    │   │
        │  ─┤     ───●───     ├── │   │    └────────┘ │    │   │
        │   │             ╲   │   │   │    │        │╱     │   │
        │   │                 │   │   │    └────────┘      │   │
        │   │     -12.4°      │   │   └────────────────────┘   │
        │   └─────────────────┘   │   ┌────────────────────┐   │
        │                         │   │ LAT: ...  LON: ... │   │
        └─────────────────────-───┴───┴────────────────────┴───┘
```

---

## What it does

- **Shows your phone's orientation** as a 3D wireframe that turns in real time as you move the phone.
- **Displays your GPS position** — latitude and longitude — read from the phone's own location service.
- **Measures the tilt of whatever it's mounted to** using a motion sensor on the board itself, shown
  as an artificial-horizon needle with a numeric readout.
- **Warns you through your phone.** Lean past 20° and the phone buzzes once. Past 25° it vibrates
  continuously until you come back under 20°.
- **Confirms the link is alive** — two LEDs blink for incoming data and for connection checks.

Everything runs over a single Bluetooth Low Energy connection. No internet connection is needed.

---

## What you need

### Hardware

| Item                          | Additional Notes                            |
| ----------------------------- | ------------------------------------------- |
| STM32H750B-DK discovery board | Includes the used built-in screen           |
| HM-10 BLE module              | None                                        |
| MPU-6050 module               | Required is the Keyestudio / GY-521 variant |
| Breadboard or protoboard      | None                                        |
| Jumper wires                  | Required are 8 male-male jumper wires       |
| Micro-USB cable               | Required only for power and flashing code   |
| Android phone                 | Required is Android 7.0 or newer            |

### Software

- [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) — provides the compiler, CMake, Ninja and the flashing tool in one install.
- [Android Studio](https://developer.android.com/studio) — only if you want to build or modify the phone app yourself rather than installing a prebuilt APK.

---

## Wiring

Eight wires. Power both modules from the board, share a common ground, then four signal lines.

**HM-10 Bluetooth module**

| Module pin | Connect to | Purpose |
|---|---|---|
| VCC | CN3 pin 5 (5V) | Power |
| GND | Common ground rail | Ground |
| TXD | PB15 (CN2, D11) | Data from phone → board |
| RXD | PB14 (STMod+ P1, pin 9) | Data from board → phone |
| STATE | PG3 (CN6, D2) | Tells the board when a phone is connected |
| EN | PE3 | Keeps the module enabled |

**MPU-6050 motion sensor**

| Module pin | Connect to                  | Purpose                      |
| ---------- | --------------------------- | ---------------------------- |
| VCC        | CN3 pin 4 (**3V3**, not 5V) | Power                        |
| GND        | Common ground rail          | Ground                       |
| SCL        | PD12 (CN2, D15)             | Sensor clock                 |
| SDA        | PD13 (CN2, D14)             | Sensor data                  |
| AD0        | Common ground rail          | Selects the sensor's address |

The sensor's `INT`, `XDA` and `XCL` pins are not used — leave them unconnected.

---

## Plug and play

Start to finish, about ten minutes.

### 1. Wire it up

Follow the tables above with the board unpowered.

### 2. Flash the board

Plug the board into your computer via the **ST-LINK** USB port, then from the repository root:

```bash
cmake --preset Debug -S firmware
```

```bash
cmake --build firmware/build/Debug
```

```bash
STM32_Programmer_CLI -c port=SWD -w firmware/build/Debug/firmware.elf -rst
```

The screen shows a loading bar with four stages. If it fills completely and the main layout appears,
the board is healthy. If it stops partway, a red message at the bottom of the screen names what
failed — most often a wiring mistake on the module for that stage.

### 3. Install the phone app

Either install a prebuilt APK from the project's Releases page, or build it yourself:

```bash
cd app && ./gradlew installDebug
```

### 4. Connect

1. Turn on **Bluetooth** and **Location** on the phone. Android requires location access for any
   BLE scan, even though nothing is sent anywhere.
2. Open **Trail HUD** and grant the permissions it asks for.
3. Tap the Bluetooth icon to scan.
4. Pick **TRAIL-HUD** from the list. If your module still has its factory name, it appears as
   **BT05** instead.

### 5. Confirm it works

Within a second or two you should see:

- The phone outline on the right of the screen **turning as you rotate your phone**.
- Latitude and longitude filling in underneath, once the phone gets a GPS fix.
- The green LED blinking each time data arrives.

Now tilt the board past 20°. Your phone should buzz once. Tilt past 25° and it buzzes continuously
until you bring it back level.

---

## Reading the display

**Left side — tilt.** A needle showing how far the board is leaning, with the angle printed below it.
Two short marks either side of centre show level. The reading comes from the board's own sensor, so
it reflects your bike, not your phone.

**Right side — your phone.** A wireframe box that matches how you're holding the phone, with your
coordinates below. When no phone is connected it reads `WAITING`.

---

## Common Troubleshooting

| Symptom                                    | Most likely cause                                                       |
| ------------------------------------------ | ----------------------------------------------------------------------- |
| Wireframe never appears but the LED blinks | Data is arriving but not in the expected format                         |
| App can't find the device                  | Location services are off, or the module is already connected elsewhere |
| Phone never buzzes                         | Vibration permission denied, or the phone is in Do Not Disturb          |

The board also prints a running log over its USB port. Open a serial terminal at **9600 baud, 8N1**
on the ST-Link Virtual COM Port and press `h` for the list of available views — it will tell you
whether packets are arriving and what they contain.

---

## Repository layout

```
firmware/   STM32 firmware — display, sensor, and Bluetooth link
app/        Android app    — sends position and orientation, receives alerts
```

---

## License

BSD 3-Clause. See [LICENSE](LICENSE).
