# Installation

> Bring-up checklist — what to buy, what to solder, and what to expect
> when you first power the board on.

This page is for the **hardware assembly** part of getting started. If you
just want to flash pre-built firmware onto an already-built board, skip
to [First flash](first-flash.md).

---

## Table of contents

- [Hardware required](#hardware-required)
- [Hardware optional](#hardware-optional)
- [Bill of materials and cost](#bill-of-materials-and-cost)
- [Board variants](#board-variants)
- [First power-on](#first-power-on)
- [Next steps](#next-steps)

---

## Hardware required

The minimum viable PhantomRF build consists of four parts. All of them
are off-the-shelf, no custom PCBs required.

| # | Part | Where to buy | Approx. price (USD) |
|---|---|---|---|
| 1 | ESP32-S3 dev board (N16R8, N8R2, or N8) | Mouser, DigiKey, AliExpress, Amazon | $5 – $15 |
| 2 | nRF24L01+PA+LNA module (with external antenna) | AliExpress, Amazon | $3 – $6 |
| 3 | CC1101 sub-GHz module (green PCB, 433 MHz or 868/915 MHz) | AliExpress, Amazon | $2 – $5 |
| 4 | SSD1306 OLED 128×64 I²C display (optional but recommended) | AliExpress, Amazon | $2 – $4 |

![Board with radios and OLED — placeholder](../../assets/board-assembled.jpg)

### What to look for in each part

**ESP32-S3 dev board.** Any variant that exposes at least 8 free GPIOs
and the native USB D+/D- pins will work. The recommended boards are
listed in [Board variants](#board-variants) below. Avoid ESP32-S3 boards
that integrate battery charging (e.g. the TinyS3) unless you also want
to use their charger — pin counts are tighter.

**nRF24L01+PA+LNA module.** Get the **PA+LNA** variant (the one with the
external antenna connector and a small shield can). The bare nRF24L01
(non-PA) has about 1/1000 the range and is too weak for any realistic
attack. The +PA+LNA version delivers +20 dBm on TX and has a low-noise
amplifier on RX, both of which are required for the
[constant-carrier trick](../algorithms/nrf24-const-carrier.md) to work
at useful ranges.

> :warning: **Beware of clones.** Many "nRF24L01+PA+LNA" modules on
> AliExpress are actually relabelled non-PA parts. Symptoms: noticeably
> shorter range, no heatsink on the PA chip, no antenna. Buy from a
> vendor with a 30-day return policy.

**CC1101 module.** Almost all modules sold are based on the TI CC1101
reference design. The frequency is fixed at order time — pick **433
MHz** for ITU Region 1 (Europe, most of Asia) or **868/915 MHz** for
ITU Region 2 (Americas). Some modules are dual-band and ship with a
spring antenna; those are fine.

> :information_source: **The 433 MHz band is by far the most
> crowded.** Most cheap keyfobs, weather stations, garage openers, and
> ISM devices use it. The 868/915 MHz band has more industrial and
> LoRa devices, fewer consumer products.

**OLED display.** The 128×64 I²C version with the SSD1306 controller is
the most common. Look for the "VCC-GND-SDA-SCL" pin order. The 128×32
variant also works (smaller font), and the SH1106 controller is
binary-compatible with a 1-pixel column offset — both are supported in
M1.

## Hardware optional

These parts aren't required to bring the firmware up, but they make the
device more useful or safer to use.

| Part | Why you'd want it |
|---|---|
| 3× tactile buttons (6×6 mm, through-hole) | Lets you navigate the OLED menu without a phone |
| 3.3 V regulator with ≥ 1 A peak (e.g. AMS1117-3.3) | Power from a 5 V USB supply without frying the ESP |
| 18650 battery + TP4056 charger module | True portable use; lasts 4–8 hours per cell |
| Logic level shifter (3.3 V ↔ 5 V) | Only needed if you're chaining an Arduino-based sniffer |
| Heatsink for the PA-LNA nRF24 | The chip gets hot at +20 dBm CW; a small aluminium tab keeps it at < 70 °C |
| N-type to SMA pigtail | If you want to use a real external antenna instead of the bundled rubber duck |

![Exploded view — placeholder](../../assets/exploded.jpg)

## Bill of materials and cost

A typical first build (ESP32-S3 N16R8 + 1 nRF24 + 1 CC1101 + OLED +
buttons, no battery):

| Item | Min | Typical | Max |
|---|---|---|---|
| ESP32-S3 N16R8 DevKitC-1 | $5 | $10 | $15 |
| nRF24L01+PA+LNA | $3 | $5 | $8 |
| CC1101 (433 MHz) | $2 | $4 | $6 |
| SSD1306 OLED 128×64 | $2 | $3 | $5 |
| 3× tactile buttons | $0.20 | $0.50 | $1 |
| Wires, header pins, perfboard | $1 | $3 | $8 |
| **Total** | **$13** | **$25** | **$43** |

> :information_source: **The biggest cost variable is the ESP32-S3
> board.** Genuine Espressif modules are $5–8 in single-unit
> quantities; "ESP32-S3" boards sold for $2 on AliExpress are usually
> ESP32-S2 (no BLE 5) or ESP32-C3 (RISC-V, single core). The pinout
> is the same so the firmware still works, but the feature set
> degrades.

## Board variants

PhantomRF supports six ESP32 variants, but only the first three are
recommended for a first build. See [hardware/esp32-s3.md](../hardware/esp32-s3.md)
for the full table.

| Variant | Chip | BLE 5 | PSRAM | Flash | Recommended? |
|---|---|---|---|---|---|
| ESP32-S3-WROOM-1-N16R8 | Xtensa LX7 dual | :white_check_mark: | 8 MB octal | 16 MB | :star: Primary |
| ESP32-S3-WROOM-1-N8R2 | Xtensa LX7 dual | :white_check_mark: | 2 MB quad | 8 MB | :star: Yes |
| ESP32-S3-WROOM-1-N8 | Xtensa LX7 dual | :white_check_mark: | — | 8 MB | :warning: Some features disabled |
| ESP32-WROOM-32 (classic) | Xtensa LX6 dual | — | — | 4 MB | :warning: No native USB, no BLE 5 |
| ESP32-C3-DevKitM-1 | RISC-V single | :white_check_mark: (LE) | — | 4 MB | :no_entry: Tight memory |
| ESP32-S2-Saola-1 | Xtensa LX7 single | :no_entry: | — | 4 MB | :no_entry: No Bluetooth |

> :information_source: **Don't get an ESP32 (classic) by accident.**
> The classic ESP32 is fine for 2.4 GHz work but lacks the native USB,
> the BLE 5 radio, and the octal PSRAM that make the S3 the primary
> target. If you see "ESP32" in the listing without "S3" or "C3", it's
> the classic.

## First power-on

1. Wire up at least the ESP32 + nRF24 (see [Wiring](wiring.md)). The
   OLED and CC1101 can come later.
2. Plug a USB-C cable into the ESP32-S3 and your computer.
3. Open a serial monitor at **115200 8N1**. Tools that work:
   - `picocom /dev/ttyACM0 -b 115200` on Linux
   - `minicom -D /dev/tty.usbmodem* -b 115200` on macOS
   - The Arduino IDE Serial Monitor
   - PlatformIO's "Monitor" task
4. Press the **RST** button (or power-cycle the board).
5. You should see the boot banner:

   ```
   === PhantomRF v0.1.0 ===
   Board: ESP32-S3-N16R8
   Initializing...
   ```

6. The banner will be followed by the AP password and a list of
   detected radios. If you see "nRF24 not responding" or "CC1101 not
   found", re-check the wiring — those messages are usually honest.

> :information_source: **No AP password? Re-flash with the web
> flasher.** If the device has never booted before, there is no
> password in NVS. The firmware generates a 16-character random
> password on first boot and prints it to the serial console exactly
> once. If you missed it, hold the BOOT button for 5 seconds to
> trigger a password reset (the LED will blink red three times).

## Next steps

- The radio modules won't be detected until you wire them up. See
  [Wiring](wiring.md) for the exact pins and a diagram.
- After the wiring is done, head to [First flash](first-flash.md) for
  three different ways to install the firmware.
- If something doesn't power up correctly, jump to
  [Troubleshooting](../reference/troubleshooting.md).
