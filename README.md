<div align="center">

  <img src="https://raw.githubusercontent.com/arslansadiq87/circuit-studio/main/techpanda%20logo.png" alt="TechPanda Logo" width="150">

  <h1>Tech Panda Labs</h1>

  <p>
    Create 3D electronics wiring diagrams, connect components,<br>
    and generate animated wiring tutorials.
  </p>

  <p>
    <a href="https://circuit.techpandalabs.com">
      <img src="https://img.shields.io/badge/Launch-Circuit_Studio-2563EB?style=for-the-badge" alt="Launch Circuit Studio">
    </a>
    <a href="https://techpandalabs.com">
      <img src="https://img.shields.io/badge/Explore-TechPanda_Labs-374151?style=for-the-badge" alt="Explore TechPanda Labs">
    </a>
  </p>

</div>

# ESP32-C3 Super Mini PlatformIO Project

This project targets an ESP32-C3 Super Mini using PlatformIO and the Arduino framework.

## Build

```powershell
pio run
```

## Upload

```powershell
pio run -t upload
```

If upload auto-detection fails, set `upload_port` in `platformio.ini`, for example `COM3`.

## Serial Monitor

```powershell
pio device monitor
```

The project enables USB CDC on boot with:

```ini
-D ARDUINO_USB_MODE=1
-D ARDUINO_USB_CDC_ON_BOOT=1
```

Many ESP32-C3 Super Mini boards use GPIO 8 for the onboard LED. If your LED is on a different pin, add this to `build_flags`:

```ini
-D LED_PIN=YOUR_GPIO
```

If the LED logic is inverted, add:

```ini
-D LED_ACTIVE_LOW=1
```
