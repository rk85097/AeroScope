# AeroScope

AeroScope turns an ESP32-S3 round touch display into a small live aircraft radar. It shows nearby aircraft, a radar-style map, range controls, aircraft trails, airport markers, and a touch details page with flight/aircraft information.

The idea came from looking at existing aircraft-radar display projects: some were very simple and hobby-looking, while others were polished but expensive. This project aims for the middle point: good-looking, useful, and affordable.

## Hardware

Required:

- **ESP32-S3 LCD Driver Board RGB/SPI N8R8 40PIN/18PIN Connector WIFI Bluetooth LE For 2.8 Captive TouchScreen Display**
- AliExpress listing: [ESP32-S3 LCD Driver Board / 2.8 captive touchscreen](https://he.aliexpress.com/item/1005007354066395.html)

Choose the N8R8 2.8-inch round capacitive touch variant:

![Required AliExpress hardware variant](docs/assets/hardware-variant.png)

Optional:

- 3D printed table mount: `TODO: add model link`
- The current mount is based on an existing model that works, but it can probably be improved by contributors.

## Install Firmware

Download the latest firmware binary:

[`releases/firmware.bin`](releases/firmware.bin)

Then flash it with `esptool.py`.

1. Install Python 3.
2. Install esptool:

```bash
python -m pip install esptool
```

3. Connect the radar board to your computer with USB-C.
4. Put the board into boot mode:

- Hold **BOOT**.
- Tap **RESET** once.
- Release **BOOT**.

5. Find the serial port:

- Windows: check Device Manager, usually `COM15`, `COM7`, etc.
- macOS: usually `/dev/cu.usbmodem*` or `/dev/cu.usbserial*`.
- Linux: usually `/dev/ttyUSB0` or `/dev/ttyACM0`.

6. Flash the firmware:

```bash
python -m esptool --chip esp32s3 --port YOUR_PORT --baud 921600 write_flash 0x10000 firmware.bin
```

Examples:

```powershell
python -m esptool --chip esp32s3 --port COM15 --baud 921600 write_flash 0x10000 firmware.bin
```

```bash
python -m esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash 0x10000 firmware.bin
```

7. Press **RESET** after flashing.

The screen should boot into AeroScope.

## First Setup

1. Power on the device.
2. Connect your phone or computer to the same Wi-Fi network you want the radar to use.
3. Open the device web control panel in a browser.

If the device is not configured yet, connect to the setup Wi-Fi network:

```text
AeroScope-Setup
```

Then open:

```text
http://192.168.4.1
```

In the control panel, set at minimum:

- Wi-Fi network name and password.
- Home latitude and longitude.
- Preferred range and display options.

After saving, keep the radar and your phone/computer on the same local network to access the control panel again.

## Use

- Tap the range value at the bottom of the screen to increase range by **5 NM**.
- Long-press the range value at the bottom to decrease range by **5 NM**.
- Tap an aircraft to open its details page.
- Use the dismiss button/text on the details page to return to the radar.

## Notes

- Aircraft and route data depend on public/open data providers, Wi-Fi quality, and local ADS-B coverage.
- Some aircraft may not expose full route, airline, model, or callsign information.
- This is a personal/open hobby project, not certified aviation equipment.
- Works with user-entered coordinates worldwide where public aircraft data providers and map data are available. If dynamic map data is unavailable, the radar can still show aircraft over the radar display.

## How It Works

- The ESP32-S3 connects to Wi-Fi and polls public aircraft data providers for aircraft near the configured latitude/longitude and range.
- Provider mode can be automatic or manually selected. Current providers include ADSB.lol, ADSB.fi, Airplanes.live, OpenSky, and an optional local Dump1090/Tar1090-style receiver URL.
- Aircraft from multiple providers are merged by ICAO address, filtered by range and ground/airborne setting, then interpolated on screen so movement feels continuous between provider updates.
- The map layer uses OpenStreetMap/Overpass data for coastlines, borders, and airport markers, with a small built-in fallback for offline or failed map fetches.
- Tapping an aircraft opens a details page. Route and airline enrichment is fetched on demand from ADSBDB by callsign, so extra data is requested only when needed.
- The web control panel is served directly by the device and is used for Wi-Fi, location, range, provider, and styling settings.

## License

Personal use is allowed. Contributions are welcome.

Commercial use, resale, paid hosting, paid redistribution, or use in a commercial product/service is not allowed without written permission from the project owner.

## Third-Party Licenses

This project uses third-party open-source libraries, tools, and public data sources, including PlatformIO, Arduino ESP32, ArduinoJson, ESP Async WebServer, AsyncTCP, LittleFS, React/Vite for the web control panel, and public aviation/map data providers.

Each third-party dependency remains under its own license. Before redistributing firmware, binaries, or modified source, review the dependency manifests and source packages for their full license terms.
