# ESP32-S3

> The primary microcontroller. All PhantomRF builds target some
> variant of the Espressif ESP32-S3 family.

The ESP32-S3 is the third-generation Tensilica Xtensa LX7 chip from
Espressif. It adds the **native USB OTG** that makes PhantomRF's
drag-and-drop UF2 flashing possible, plus **BLE 5.0 LE 2M PHY** and
**octal PSRAM** support. The older ESP32 (Xtensa LX6) and the
ESP32-C3 (RISC-V) are also supported but with reduced feature sets.

---

## Table of contents

- [Variants](#variants)
- [Pin map](#pin-map)
- [Strapping pins](#strapping-pins)
- [GPIO safety notes](#gpio-safety-notes)
- [PSRAM](#psram)
- [USB CDC](#usb-cdc)
- [Power and brownouts](#power-and-brownouts)
- [Boot modes](#boot-modes)
- [Next steps](#next-steps)

---

## Variants

| Module | Flash | PSRAM | Native USB | BLE 5 | Notes |
|---|---|---|---|---|---|
| ESP32-S3-WROOM-1-N16R8 | 16 MB | 8 MB octal | :white_check_mark: | :white_check_mark: | Primary target. 2.4 GHz antenna on-module. |
| ESP32-S3-WROOM-1-N8R2  | 8 MB  | 2 MB quad  | :white_check_mark: | :white_check_mark: | Cheaper. Spectrum buffer is half-size. |
| ESP32-S3-WROOM-1-N8    | 8 MB  | —          | :white_check_mark: | :white_check_mark: | No PSRAM. Recording buffer is 4× smaller. |
| ESP32-S3-WROOM-1-U16   | 16 MB | 8 MB octal | :white_check_mark: | :white_check_mark: | With U.FL antenna connector (better RF). |
| ESP32-S3-WROOM-1-U4    | 4 MB  | —          | :white_check_mark: | :white_check_mark: | Tight memory; no room for OTA. |

For M0 we **strongly recommend** the N16R8. The 8 MB octal PSRAM
holds the spectrum waterfall, the recording ring buffer, and the
JSON log buffer. The N8 (no PSRAM) variant works but is
constrained.

> :information_source: **The chip on the module is the same in all
> variants.** The differences are in the on-module flash, PSRAM, and
> antenna. The internal SRAM is 512 KB regardless.

## Pin map

This is the pin map for the **N16R8** (primary target). The other
modules in the family use the same GPIOs.

```
                        ┌──────────────────┐
                        │     ESP32-S3     │
                        │      WROOM-1     │
                        │                  │
   GPIO  0 ─────────────┤  (strapping)     ├───── 3V3
   GPIO  1 ─────────────┤  VBAT ADC        ├───── EN
   GPIO  2 ─────────────┤  CC1101 GDO0     ├───── GPIO 15 (CSN0)
   GPIO  3 ─────────────┤  (strapping)     ├───── GPIO 16 (CE0)
   GPIO  4 ─────────────┤  CC1101 GDO2     ├───── GPIO 17 (CSN1)
   GPIO  5 ─────────────┤  LED-R           ├───── GPIO 18 (CE1)
   GPIO  6 ─────────────┤  LED-G           ├───── GPIO 19 (USB-D+)
   GPIO  7 ─────────────┤  LED-B           ├───── GPIO 20 (USB-D-)
   GPIO  8 ─────────────┤  OLED SDA        ├───── GPIO 21 (CSN2 / Prev)
   GPIO  9 ─────────────┤  OLED SCL        ├───── GPIO 35 (CE2)
   GPIO 10 ─────────────┤  OK button       ├───── GPIO 36 (CSN3)
   GPIO 11 ─────────────┤  MOSI (nRF24)    ├───── GPIO 37 (CE3)
   GPIO 12 ─────────────┤  SCK (nRF24)     ├───── GPIO 38 (CSN4)
   GPIO 13 ─────────────┤  MISO (nRF24)    ├───── GPIO 39 (CE4)
   GPIO 14 ─────────────┤  Next button     ├───── GPIO 40 (UART0 TX)
   GND     ─────────────┤                  ├───── GPIO 41 (UART0 RX)
                        │                  │
                        └──────────────────┘
```

For the per-board pin assignment of every role (Nrf24, CC1101, OLED,
buttons, LED, etc.), see [Pinout per board](../reference/pinout.md).

## Strapping pins

The ESP32-S3 has five **strapping pins** that affect the boot
behaviour and must be in a known state at reset. PhantomRF avoids
using these as much as possible, but the OLED SDA pin (GPIO 8) is
sometimes one of them on certain dev boards — check your module's
datasheet.

| GPIO | Strapping function | PhantomRF use | OK to use? |
|---|---|---|---|
| GPIO 0 | Boot mode (1 = SPI boot, 0 = download) | n/a (input-only) | No — leave floating with pull-up |
| GPIO 3 | JTAG signal source | n/a (input-only) | No — leave floating |
| GPIO 45 | VDD_SPI voltage (1 = 3.3 V, 0 = 1.8 V) | n/a (input-only) | No — must match flash voltage |
| GPIO 46 | Boot mode (ROM messages) | n/a (input-only) | No |

> :warning: **GPIO 0 must be HIGH on reset for normal boot.** Most
> dev boards include a pull-up + a BOOT button that pulls it low
> when pressed (entering download mode). If your custom wiring
> accidentally ties GPIO 0 to ground, the board will silently fail
> to boot.

## GPIO safety notes

| GPIO | Concern | Mitigation |
|---|---|---|
| GPIO 19/20 | USB D+/D- | Don't put anything on these. The internal PHY expects a 90 Ω differential pair. |
| GPIO 26-32 | Used for octal PSRAM on the N16R8 | **Do not use these GPIOs.** The PSRAM needs them. The firmware refuses to configure them. |
| GPIO 33-37 | Used for octal flash on the N16R8 | Same as above — keep them free. |
| Any GPIO | 3.3 V only | The ESP32-S3 is **not 5 V tolerant**. Driving 5 V into any GPIO will damage it. |

## PSRAM

The octal PSRAM on the N16R8 gives us 8 MB of additional heap. The
firmware uses it for:

- The spectrum waterfall (8 KB per frame × 10 fps = 80 KB)
- The recording ring buffer (32 KB × 256 recordings = 8 MB)
- The JSON document pool for the WebSocket (4 KB × 4 documents = 16 KB)
- The web UI asset cache (1.5 MB for the static HTML/JS/CSS)

In builds **without PSRAM** (the N8 variant), the firmware falls
back to internal SRAM. The recording ring shrinks to 32 KB × 4
recordings, and the spectrum waterfall is disabled in the web UI
(only the in-page mock-up is shown). Everything else works.

To check whether PSRAM is detected, look at the boot log:

```
[heap] PSRAM: 8388608 bytes
```

If you see `PSRAM: 0 bytes`, the chip detected the module but the
PSRAM init failed. This is usually a power-supply issue — the
octal PSRAM is sensitive to the 3V3 rail and will silently disable
itself if the voltage sags.

## USB CDC

The ESP32-S3's native USB OTG peripheral exposes a CDC (serial)
device on the host. No driver is required on any modern OS:

- **Linux 3.x and newer:** the `cdc_acm` driver is built in.
- **macOS 10.9 and newer:** Apple's CDC driver is built in.
- **Windows 10 1709 and newer:** the USB CDC class driver is built
  in. For older Windows, the `usbser.sys` driver works but needs a
  one-time `dpinst.exe` install.

The USB serial port appears as:

- Linux: `/dev/ttyACM0` (or `ttyACM1`, `ttyACM2`, ...)
- macOS: `/dev/tty.usbmodem*`
- Windows: `COM3`, `COM4`, etc. (check Device Manager → Ports)

The same USB port also handles the **UF2 drag-and-drop bootloader**.
The board appears as a 8 MB USB mass storage device named
`PHANTOMRF` (or `ESP32-S3`). Drag a `.uf2` file onto it and the
firmware reflashes in ≈ 2 s, with no reset button required.

## Power and brownouts

The ESP32-S3 peaks at **500 mA** during WiFi TX at maximum power.
The on-board 3V3 regulator on most dev boards is rated for 600 mA
or so, which is enough for the chip alone but leaves little
headroom for radios.

A **brownout** happens when the 3V3 rail sags below the chip's
minimum. The chip then resets itself and prints
`Brownout detector was triggered` to the serial monitor. Common
causes:

- A weak USB cable (some cables have 28 AWG power wires)
- A USB hub that doesn't supply enough current
- The on-board LDO is undersized for the load

**Solutions:**

1. Use a 22 AWG USB-C cable (or shorter).
2. Plug directly into the computer, not a hub.
3. Add an external 3V3 LDO with ≥ 1 A peak (e.g. AP2112K-3.3).
4. Power from a bench supply during development.

The firmware has a brownout detector enabled in the bootloader.
The threshold is set to 3.0 V; below that, the chip resets rather
than running on marginal power.

## Boot modes

The ESP32-S3 has three boot modes, selected by GPIO 0 at reset:

| GPIO 0 | Mode | What it does |
|---|---|---|
| HIGH (or floating) | SPI Boot | Boots from on-module flash. Normal operation. |
| LOW (held at reset) | Download | Stays in the ROM bootloader. Accepts new firmware over UART. |
| LOW (after a reset) | — | Same as above, but for one boot only. |

The "BOOT" button on most dev boards pulls GPIO 0 low when held.
To enter download mode: hold BOOT, press RST, release BOOT.

PhantomRF never touches GPIO 0 from the firmware, so the boot
behaviour is unaffected by which attack is running.

## Next steps

- The wiring is the next thing to check. See
  [Wiring guide](../../getting-started/wiring.md).
- For the full pin assignment, see
  [Pinout per board](../reference/pinout.md).
- For the power tree and worst-case current, see
  [Power budget](power-budget.md).
