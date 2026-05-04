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
