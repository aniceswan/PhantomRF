# First flash

> Three different ways to get firmware onto the board. Pick the one
> that matches your toolchain.

The first flash is the hardest. After that, every subsequent update
is a one-click operation from the web UI (over-the-air, "OTA").

---

## Table of contents

- [Pick a method](#pick-a-method)
- [Method 1: Web flasher (easiest)](#method-1-web-flasher-easiest)
- [Method 2: ESP Web Tool (browser, no install)](#method-2-esp-web-tool-browser-no-install)
- [Method 3: PlatformIO (developer)](#method-3-platformio-developer)
- [Method 4: Arduino IDE](#method-4-arduino-ide)
- [Method 5: esptool.py (CI / scripting)](#method-5-esptoolpy-ci--scripting)
- [Verifying the flash](#verifying-the-flash)
- [Next steps](#next-steps)

---

## Pick a method

| Method | Tooling required | Best for |
|---|---|---|
| **Web flasher** | None — runs in Chrome / Edge | Most users, first time |
| **ESP Web Tool** | None — runs in any Chromium browser | Users who want a specific release |
| **PlatformIO** | `pio` CLI, Python | Developers, anyone who already uses it |
| **Arduino IDE** | Arduino IDE 2.x, ESP32 board package | Hobbyists who prefer the GUI |
| **esptool.py** | Python 3, `pip install esptool` | CI, scripting, advanced users |

We recommend **Web flasher** for a first build. The `.uf2` file is
self-contained and the drag-and-drop flow is hard to mess up.

## Method 1: Web flasher (easiest)

PhantomRF ships a custom [Web Flasher](https://anomalyco.github.io/PhantomRF/flasher/)
that wraps the Espressif `esp-web-tools` library. It runs entirely in
your browser — no software to install, no serial drivers to track down.

1. Open <https://anomalyco.github.io/PhantomRF/flasher/> in Chrome or Edge.
2. Plug your ESP32-S3 into a USB-C cable.
3. Click **Connect**, pick the serial port labelled `USB JTAG/serial`.
4. Select the **PhantomRF v0.1.0 (esp32s3-n16r8)** firmware file.
5. Click **Install**.
6. Wait for the green "Installation complete" banner (≈ 25 s).
7. Press the **RST** button on the board. The PhantomRF banner
   should appear in the Web Flasher's serial monitor pane.

> :information_source: **Chrome on macOS may need extra permissions.**
> If the serial port dropdown is empty, go to **Chrome → View →
> Developer Tools → Site Settings** and grant "USB" for the site.

![Web flasher screenshot — placeholder](../../assets/web-flasher.png)

### What if "Connect" doesn't see the port?

The ESP32-S3's native USB needs the host to know about the USB CDC
class. On most modern OSes this is built in, but on older systems you
need a one-time driver install:

- **Windows 10/11:** No driver needed for S3's native USB. For
  boards that use an external USB-UART bridge (CH340, CP2102), you
  need the manufacturer driver.
- **macOS:** No driver needed.
- **Linux:** No driver needed since 3.x kernels.

## Method 2: ESP Web Tool (browser, no install)

The [Espressif ESP Web Tool](https://esphome.github.io/esp-web-tools/)
is a generic firmware installer that works with any ESP32 firmware
that's structured as a `manifest.json` + `.bin` triplet.

1. Download the three files from the [latest release](https://github.com/anomalyco/PhantomRF/releases):
   - `phantomrf-esp32s3-n16r8-firmware.bin`
   - `phantomrf-esp32s3-n16r8-bootloader.bin`
   - `phantomrf-esp32s3-n16r8-partitions.bin`
2. Open <https://esphome.github.io/esp-web-tools/> in your browser.
3. Click **Connect**, choose your port.
4. Drag the three files into the drop zone, or pick them manually.
5. Click **Install**.

This is the same flow that the PhantomRF Web Flasher runs under the
hood. If the custom flasher is offline or stale, ESP Web Tool is a
reliable fallback.

## Method 3: PlatformIO (developer)

If you want to modify the source code, build the firmware, and
optionally flash it, PlatformIO is the way.

### One-time setup

```bash
# Install PlatformIO (if you don't have it)
pip install platformio

# Or use the standalone CLI
# (see https://platformio.org/install/cli)
```

### Clone and build

```bash
git clone https://github.com/anomalyco/PhantomRF.git
cd PhantomRF
pio run -e esp32s3-n16r8
```

The build emits a `.pio/build/esp32s3-n16r8/firmware.bin` (and
several related files). The `post_build.py` script also generates a
UF2 in `dist/`.

### Flash

```bash
pio run -e esp32s3-n16r8 -t upload
```

The first upload will take 15–25 s. Subsequent uploads only reflash
the app partition and are faster (≈ 5 s).

### Open the serial monitor

```bash
pio device monitor -e esp32s3-n16r8
# or, equivalent
pio run -e esp32s3-n16r8 -t monitor
```

Set the baud rate to **115200** in your terminal (the PlatformIO
`monitor_speed` is set to this in `platformio.ini`).

### Building for a different board

```bash
# ESP32-S3 with 8 MB flash + 2 MB PSRAM
pio run -e esp32s3-n8r2 -t upload

# ESP32-S3 with 8 MB flash, no PSRAM
pio run -e esp32s3-n8 -t upload

# Classic ESP32 (no native USB, no BLE 5)
pio run -e esp32-classic -t upload
```

## Method 4: Arduino IDE

The Arduino IDE works for the M0 firmware but doesn't have the
pre-build hooks that PlatformIO does (UF2 generation, manifest
emission, etc.). Use it only if you're already in that ecosystem.

1. **Add the ESP32 board package URL.** In Arduino IDE, go to
   **File → Preferences**, and add this URL to "Additional Board
   Manager URLs":
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Install the ESP32 package.** Tools → Board → Boards Manager,
   search "esp32", install.
3. **Install the libraries.** Tools → Manage Libraries, install:
   - RF24 by TMRh20
   - SmartRC-CC1101-Driver-Lib
   - NimBLE-Arduino
   - Adafruit SSD1306
   - Adafruit GFX Library
   - GyverButton
   - ArduinoJson
   - AsyncTCP
   - ESPAsyncWebServer
4. **Open the sketch.** The project doesn't ship an Arduino sketch
   directly — copy the contents of `src/` into a new Arduino project
   directory, or use `File → Open` to point at `src/main.cpp`.
5. **Select the board.** Tools → Board → ESP32S3 Dev Module.
6. **Set the partition scheme.** Tools → Partition Scheme → "16MB
   Flash (3MB APP/9.5MB FAT/1.5MB FFat/2MB OTA)" (or whichever
   matches your board).
7. **Click Upload.**

## Method 5: esptool.py (CI / scripting)

`esptool.py` is the lowest-level flash tool. It runs on every
platform with Python 3.

```bash
pip install esptool

# Erase the entire flash (factory reset)
esptool.py --chip esp32s3 --port /dev/ttyACM0 erase_flash

# Flash the bootloader, partition table, and app
esptool.py --chip esp32s3 --port /dev/ttyACM0 \
    --baud 921600 \
    --before default_reset --after hard_reset \
    write_flash \
    0x0    bootloader.bin \
    0x8000 partitions.bin \
    0x10000 firmware.bin
```

Replace `/dev/ttyACM0` with the actual port:
- Linux: `/dev/ttyACM0` or `/dev/ttyUSB0`
- macOS: `/dev/tty.usbmodem*`
- Windows: `COM3`, `COM4`, etc. (check Device Manager)

## Verifying the flash

After flashing, press **RST** and watch the serial output. You should
see something like:

```
=== PhantomRF v0.1.0 ===
Board: ESP32-S3-N16R8
Initializing...
[core] NVS namespace 'phm' opened
[storage] LittleFS mounted: 1100000 / 1585152 bytes free
[btn] buttons: OK=10 NEXT=14 PREV=21 (cfg=2)
[oled] OLED ready (3-button config)
[web] HTTP server up on http://192.168.4.1/
[web] WebSocket ready at ws://192.168.4.1/ws
[web] AP SSID: PhantomRF-A3F2 | password: <16 chars>
[cli] Console ready (USB CDC at 115200 8N1)

Ready. Connect to the AP and open the web UI.
phantom> 
```

If you see all of the `[core]`, `[storage]`, `[web]`, `[cli]`
log lines, the flash is good. If you see `[oled] SSD1306 not found`,
the OLED isn't wired correctly (or it's at a different I²C address).

## Next steps

- The serial monitor is your friend. Every module logs its progress
  to the same console, and most issues are diagnosable from there.
- Once the device boots, head to [First use](first-use.md) to
  connect via WiFi and start a test attack.
- Subsequent updates are over-the-air. See
  [Web UI → OTA](../../web/index.html#ota) for the drag-and-drop
  flow.
