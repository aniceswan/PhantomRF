# Troubleshooting

> Common issues and their fixes. If your problem isn't here,
> open an issue on GitHub with the boot log attached.

This page is a "things that go wrong and how to fix them"
checklist. It's organised by symptom. Use Ctrl+F to search.

---

## Table of contents

- [Power and boot](#power-and-boot)
- [Serial and USB](#serial-and-usb)
- [WiFi and the web UI](#wifi-and-the-web-ui)
- [Radio not detected](#radio-not-detected)
- [OLED issues](#oled-issues)
- [Buttons not working](#buttons-not-working)
- [Attack issues](#attack-issues)
- [OTA issues](#ota-issues)
- [Performance issues](#performance-issues)
- [Factory reset](#factory-reset)
- [Next steps](#next-steps)

---

## Power and boot

### Symptom: nothing happens when I plug in the USB

**Likely cause:** bad USB cable, no power on the 5 V rail, or
the BOOT button is stuck.

**Fix:**

1. Try a different USB cable. Some cables are charge-only and
   have no data wires, but those still power the board.
2. Try a different USB port on your computer. Some ports
   are power-limited.
3. Check the BOOT button isn't stuck. Press it and release
   it a few times.
4. Try holding BOOT, pressing RST, releasing RST, then
   releasing BOOT. This is the "download mode" — if the
   firmware is corrupted, this lets you re-flash.

### Symptom: chip resets repeatedly

**Likely cause:** brownout detector tripping. The 3V3 rail is
sagging under load.

**Fix:**

1. Use a thicker USB cable (22 AWG power wires).
2. Plug directly into the computer, not through a hub.
3. Add an external 3V3 LDO rated for at least 1 A.
4. If you've added an nRF24 PA-LNA, make sure the 3V3 rail
   can supply 600+ mA peak.

See [Power budget](../hardware/power-budget.md) for
measurements.

### Symptom: `Brownout detector was triggered` in the log

**Likely cause:** the 3V3 rail dropped below the chip's
minimum (≈ 3.0 V) during a current spike.

**Fix:** same as above. The brownout detector is enabled by
default; you can lower the threshold (or disable it) in
`menuconfig` under `Component config → ESP System Settings`.

### Symptom: `Guru Meditation Error: Core 1 panic`

**Likely cause:** firmware bug, watchdog timeout, or stack
overflow. The chip will print a register dump and reboot.

**Fix:**

1. Capture the full panic output over the serial monitor.
2. Check the offending PC address — it's the start of the
   function that crashed.
3. If the PC is in a known library (e.g. `RF24::write`),
   check your wiring (most "library" panics are actually
   SPI bus errors).
4. File an issue with the panic dump.

## Serial and USB

### Symptom: no serial port appears

**Likely cause:** driver issue, or the ESP32-S3's USB is in
the wrong mode.

**Fix:**

- **Linux:** `ls /dev/ttyACM*`. If nothing, the kernel
  doesn't see the device. `dmesg` will show why.
- **macOS:** `ls /dev/tty.usbmodem*`. If nothing, check
  System Information → USB.
- **Windows:** Device Manager → Ports (COM & LPT). If
  nothing, you may need a driver.

On modern OSes, no driver install is required for the S3's
native USB. If the OS still doesn't see the device, try
a different USB cable (some are power-only).

### Symptom: serial port is busy / can't open

**Likely cause:** another process is holding the port (the
Arduino IDE's Serial Monitor, a leftover `minicom` process,
etc.)

**Fix:** close all serial monitors and IDEs. On Linux,
`fuser /dev/ttyACM0` will show what's holding the port.

### Symptom: garbage characters in the serial monitor

**Likely cause:** baud rate mismatch. The default is 115200.

**Fix:** set the baud rate to 115200 in your terminal. Other
common rates (9600, 38400) won't work — the firmware doesn't
autodetect.

## WiFi and the web UI

### Symptom: the AP doesn't show up in my WiFi list

**Likely cause:** the AP is disabled, or the firmware is
crashing during boot.

**Fix:**

1. Check the serial monitor for boot errors.
2. If the boot completes but the AP doesn't show, try
   `get ap.enabled` in the CLI. It should return `true`.
3. If the firmware has never been configured, the AP SSID
   is `PhantomRF-XXXX` where `XXXX` is the last 2 bytes of
   the AP MAC. Check the serial console for the exact
   name.

### Symptom: connected to the AP but can't open the web UI

**Likely cause:** captive portal is interfering, or the
device's IP has changed.

**Fix:**

1. Try `http://192.168.4.1/` directly. Don't rely on the
   auto-redirect.
2. If that fails, try `http://phantomrf.local/`. The
   firmware advertises itself via mDNS.
3. If neither works, check the serial console — the
   `g_state.apActive` field should be `true`.

### Symptom: HTTP Basic Auth dialog keeps popping up

**Likely cause:** wrong password, or rate limit lockout.

**Fix:**

1. The default password is on the serial console. If you
   changed it, use the new one.
2. After 5 failed attempts, the device locks out for 60
   seconds. Wait, then try again.
3. If you've forgotten the password, hold the BOOT
   button for 5 seconds to regenerate it. The LED will
   blink red three times.

### Symptom: web UI loads but stays blank

**Likely cause:** the WebSocket can't connect, usually
because the ESP32-S3's WiFi is overloaded.

**Fix:**

1. Refresh the page. The browser will reconnect.
2. Check the browser's developer console for errors
   (Ctrl+Shift+I in Chrome).
3. If the page is blank for more than 5 seconds, the
   WebSocket handshake is failing. Try `ws://192.168.4.1/ws`
   directly in a tool like `wscat`.

## Radio not detected

### Symptom: `nRF24 not responding` in the log

**Likely cause:** SPI wiring, or the chip is unpowered.

**Fix:**

1. Verify the 3V3 and GND wires to the nRF24. Use a
   multimeter to confirm 3.3 V across VCC-GND.
2. Verify SCK, MISO, MOSI, CE, CSN. The order matters.
3. Try a different nRF24 module. Some AliExpress modules
   are dead on arrival.
4. Check for solder bridges. A bridge between SCK and
   MOSI will lock the SPI bus.

### Symptom: `CC1101 not found` in the log

**Likely cause:** SPI wiring, or wrong SPI mode.

**Fix:**

1. Verify the 3V3 and GND wires to the CC1101.
2. Check SCK, MISO, MOSI, CSN. The CC1101 uses SPI mode 0.
3. Try a different CC1101 module. The cheap "green PCB"
   modules are occasionally DOA.
4. Some CC1101 modules have a 0.1 µF bypass cap missing.
   Add one across VCC-GND, close to the module.

### Symptom: radio detected but RSSI is always -100

**Likely cause:** antenna not connected, or wrong channel.

**Fix:**

1. Connect an antenna. The PA-LNA **will burn out** if
   you transmit without one.
2. Try a different channel. If the target is on channel 76
   and you're scanning channel 12, you'll see nothing.
3. Check that the CE pin is being driven HIGH during RX.

## OLED issues

### Symptom: OLED shows nothing

**Likely cause:** I²C address mismatch, or wiring issue.

**Fix:**

1. Run an I²C scanner sketch. The OLED should show up at
   `0x3C` (almost always) or `0x3D` (rare).
2. Verify SDA and SCL wires. The OLED is fragile — a
   short between SDA and SCL will lock the I²C bus.
3. Try a different OLED module. The cheap SSD1306 modules
   are occasionally DOA.
4. Check the OLED's `oled.enabled` NVS key:
   ```
   phantom> get oled.enabled
   oled.enabled = true
   ```
   If it's `false`, set it back:
   ```
   phantom> set oled.enabled true
   phantom> save
   ```

### Symptom: OLED shows garbled text

**Likely cause:** I²C bus errors, often from a loose wire.

**Fix:**

1. Reseat the OLED. The 4-pin header is easy to misalign.
2. Add a 4.7 kΩ pull-up on SDA and SCL (most OLED modules
   already have them, but a missing one causes intermittent
   errors).
3. Shorten the I²C wires. Long wires pick up noise.

## Buttons not working

### Symptom: button presses don't register

**Likely cause:** wrong button config, or wiring issue.

**Fix:**

1. Check the `buttons.config` NVS key. It should match
   your physical setup:
   ```
   phantom> get buttons.config
   buttons.config = 2
   ```
2. Verify the wiring. Each button should be between the
   GPIO and GND (active-low).
3. Try a different button. The cheap tactile switches
   are occasionally DOA.

### Symptom: button registers multiple presses

**Likely cause:** debounce settings too short, or the
button is mechanically bouncy.

**Fix:**

1. Increase the debounce time:
   ```
   phantom> set buttons.debounce_ms 100
   ```
2. Replace the button. Mechanical bounce is rare but
   possible.

## Attack issues

### Symptom: attack starts but has no effect

**Likely cause:** wrong target, wrong channel, or the
target device is out of range.

**Fix:**

1. Verify the target device is on the same channel you're
   jamming.
2. Move closer. The PA-LNA's range is 30 m indoors;
   beyond that, the jam is ineffective.
3. Use the spectrum analyzer to verify the jam is actually
   transmitting. A bright spot on the channel you're
   jamming means it's working.

### Symptom: `2.4 GHz jam disconnects me from the PhantomRF`

**Likely cause:** the PhantomRF AP is in the 2.4 GHz band,
so a 2.4 GHz jam will disrupt the AP link.

**Fix:**

1. Use USB CDC to stop the attack. The serial link
   survives a 2.4 GHz jam.
2. Use the BLE Serial CLI (M1) if 2.4 GHz is jammed.
3. For long-running tests, set up the device to join an
   existing WiFi network (M1) instead of running its own
   AP.

### Symptom: attack stops on its own after a few minutes

**Likely cause:** thermal throttling. The chip is overheating.

**Fix:**

1. Add a heatsink to the nRF24 PA chip.
2. Reduce the PA level:
   ```
   phantom> set nrf24.pa 1
   ```
3. Use forced airflow (a small fan).

## OTA issues

### Symptom: OTA upload fails with "Update.begin failed"

**Likely cause:** the partition table doesn't have room
for the new firmware, or the upload was interrupted.

**Fix:**

1. Check the partition table in `partitions/default_16mb.csv`.
   The OTA partition should be at least 1.5 MB.
2. Retry the upload. The ESP-IDF's `Update` class leaves
   the OTA partition in a half-written state after a
   failed upload; the next upload overwrites it cleanly.

### Symptom: OTA succeeds but the new firmware doesn't boot

**Likely cause:** the new firmware is for a different
board variant, or the partition table is wrong.

**Fix:**

1. Check that you uploaded the right `.bin` for your
   board. The N16R8 firmware won't boot on a classic
   ESP32.
2. Re-flash the bootloader and partition table. The OTA
   only updates the app partition; the bootloader is
   fixed. If the new firmware expects a different
   partition layout, you need a full reflash with
   `esptool.py`.

## Performance issues

### Symptom: spectrum waterfall is laggy

**Likely cause:** too many simultaneous WS clients, or
the browser is struggling with the canvas redraw.

**Fix:**

1. Close other browser tabs. Canvas redraw is CPU-heavy.
2. Lower the spectrum update rate:
   ```
   phantom> set spectrum.window_ms 500
   ```
3. The M0 firmware runs spectrum at 1 Hz. M1 will run
   it at 10 Hz with binary frames.

### Symptom: web UI is slow to respond

**Likely cause:** the ESP32-S3 is overloaded (e.g. a
5-radio sweep is running and starving the HTTP server).

**Fix:**

1. Stop the attack. The HTTP server runs on Core 0 at
   low priority; an attack running on Core 1 shouldn't
   affect it, but a busy attack can saturate the SPI
   bus and cause HTTP delays.
2. Reduce the number of active radios:
   ```
   phantom> set nrf24.count 1
   ```

## Factory reset

If nothing else works, factory-reset the device:

1. **From the web UI:** Settings → "Factory reset" button.
2. **From the CLI:**
   ```
   phantom> reset
   ```
3. **From the BOOT button:** hold BOOT for 10 seconds.
   The LED will blink red 5 times. The device reboots
   with default settings.

> :warning: **Factory reset is destructive.** All custom
> settings are lost. The default AP password (printed to
> the serial console) is regenerated.

## Next steps

- If your problem isn't covered here, open an issue on
  GitHub. Include the boot log (captured via the serial
  monitor at 115200 baud).
- The legal picture is in
  [Disclaimer](../legal/disclaimer.md).
- The CLI commands are in [commands.md](commands.md).
- The NVS settings are in [settings.md](settings.md).
