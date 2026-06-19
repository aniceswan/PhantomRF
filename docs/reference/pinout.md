# Pinout per board

> Every GPIO assignment for every supported board variant. The
> authoritative reference; the wiring diagrams in
> [Wiring guide](../getting-started/wiring.md) are derived
> from this table.

PhantomRF supports six ESP32 variants. The pin tables live
in `src/hal/Board.cpp`; the per-board rationale is in
[DESIGN.md §8](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#8-pinout-specification).

---

## Table of contents

- [How the table is laid out](#how-the-table-is-laid-out)
- [ESP32-S3-WROOM-1-N16R8 (primary)](#esp32-s3-wroom-1-n16r8-primary)
- [ESP32-S3-WROOM-1-N8R2](#esp32-s3-wroom-1-n8r2)
- [ESP32-S3-WROOM-1-N8 (no PSRAM)](#esp32-s3-wroom-1-n8-no-psram)
- [ESP32 classic (Xtensa LX6)](#esp32-classic-xtensa-lx6)
- [ESP32-C3-DevKitM-1 (RISC-V)](#esp32-c3-devkitm-1-risc-v)
- [ESP32-S2-Saola-1](#esp32-s2-saola-1)
- [Pin-role ↔ GPIO quick reference](#pin-role--gpio-quick-reference)
- [Next steps](#next-steps)

---

## How the table is laid out

Each row is a `PinRole` — a logical name for what the pin
*does*, decoupled from the GPIO number. The actual GPIO
number is in the column for the board you're using. `-1`
means "not wired on this board".

If you want to use a different GPIO for a role (e.g. move
the OLED SDA to a free pin), the way to do it is to edit the
`kPins[]` table in `src/hal/Board.cpp` and add a build flag
for your custom board, *not* to hard-code GPIO numbers in
the rest of the code.

## ESP32-S3-WROOM-1-N16R8 (primary)

The recommended board for the M0 firmware. 16 MB flash, 8 MB
octal PSRAM, native USB OTG, BLE 5.

| PinRole | GPIO | Notes |
|---|---|---|
| Nrf24Ce0  | 16 | nRF24 #0 chip enable |
| Nrf24Csn0 | 15 | nRF24 #0 chip select |
| Nrf24Ce1  | 18 | nRF24 #1 chip enable |
| Nrf24Csn1 | 17 | nRF24 #1 chip select |
| Nrf24Ce2  | 35 | nRF24 #2 chip enable |
| Nrf24Csn2 | 21 | nRF24 #2 CSN (**shared with Prev button**) |
| Nrf24Ce3  | 37 | nRF24 #3 chip enable |
| Nrf24Csn3 | 36 | nRF24 #3 chip select |
| Nrf24Ce4  | 39 | nRF24 #4 chip enable |
| Nrf24Csn4 | 38 | nRF24 #4 chip select |
| Nrf24Sck  | 12 | SPI clock (shared) |
| Nrf24Miso | 13 | SPI MISO (shared) |
| Nrf24Mosi | 11 | SPI MOSI (shared) |
| Cc1101Sck  | 36 | SPI clock (shared with nRF24) |
| Cc1101Miso | 37 | SPI MISO (shared) |
| Cc1101Mosi | 38 | SPI MOSI (shared) |
| Cc1101Csn0 | 39 | CC1101 chip select |
| Cc1101Gdo0 | 2  | CC1101 GDO0 (data) |
| Cc1101Gdo2 | 4  | CC1101 GDO2 (FIFO threshold) |
| Cc1101Csn1 | -1 | Second CC1101 (M1) |
| OledSda   | 8  | OLED I²C data |
| OledScl   | 9  | OLED I²C clock |
| ButtonOk  | 10 | Centre / select |
| ButtonNext| 14 | Move forward in menu |
| ButtonPrev| 21 | Move backward (**shared with Nrf24Csn2**) |
| LedR      | 5  | RGB LED red channel |
| LedG      | 6  | RGB LED green channel |
| LedB      | 7  | RGB LED blue channel |
| VbatAdc   | 1  | Battery monitor (through 100k/100k divider) |
| UsbDp     | 19 | Native USB D+ |
| UsbDm     | 20 | Native USB D- |

> :warning: **GPIO 21 conflict.** In the 5-radio build,
> Nrf24Csn2 and ButtonPrev share GPIO 21. The 5-radio build
> is for M2; in M0 only Nrf24 #0 is used, so the Prev
> button is available. Don't wire both.

## ESP32-S3-WROOM-1-N8R2

Same pinout as the N16R8, just with 8 MB flash and 2 MB
quad PSRAM. All features work, but the spectrum waterfall
buffer is half the size.

| PinRole | GPIO | Notes |
|---|---|---|
| Nrf24Ce0..4, Csn0..4 | (same as N16R8) | |
| Cc1101* | (same as N16R8) | |
| OledSda, OledScl | 8, 9 | |
| ButtonOk, Next, Prev | 10, 14, 21 | |
| LedR, G, B | 5, 6, 7 | |
| VbatAdc | 1 | |
| UsbDp, Dm | 19, 20 | |

## ESP32-S3-WROOM-1-N8 (no PSRAM)

8 MB flash, no PSRAM. The firmware runs but with reduced
buffer sizes. The spectrum waterfall is disabled in the web
UI (only the in-page mock is shown).

| PinRole | GPIO | Notes |
|---|---|---|
| Nrf24Ce0..4, Csn0..4 | (same as N16R8) | |
| Cc1101* | (same as N16R8) | |
| OledSda, OledScl | 8, 9 | |
| ButtonOk, Next, Prev | 10, 14, 21 | |
| LedR, G, B | 5, 6, 7 | |
| VbatAdc | 1 | |
| UsbDp, Dm | 19, 20 | |

## ESP32 classic (Xtensa LX6)

For users who already have a classic ESP32-DevKit. No native
USB, no BLE 5. The 4 MB flash is enough but tight.

| PinRole | GPIO | Notes |
|---|---|---|
| Nrf24Ce0  | 16 | |
| Nrf24Csn0 | 15 | |
| Nrf24Ce1  | 18 | |
| Nrf24Csn1 | 17 | |
| Nrf24Ce2..4, Csn2..4 | -1 | Not enough free GPIOs |
| Nrf24Sck  | 14 | |
| Nrf24Miso | 12 | |
| Nrf24Mosi | 13 | |
| Cc1101Sck  | 18 | (shared with Nrf24Ce1) |
| Cc1101Miso | 19 | |
| Cc1101Mosi | 23 | |
| Cc1101Csn0 | 5  | |
| Cc1101Gdo0 | 27 | (shared with ButtonPrev) |
| Cc1101Gdo2 | 4  | |
| Cc1101Csn1 | -1 | |
| OledSda   | 21 | |
| OledScl   | 22 | |
| ButtonOk  | 25 | |
| ButtonNext| 26 | |
| ButtonPrev| 27 | (shared with Cc1101Gdo0) |
| LedR      | 2  | Classic has only a single LED (no G or B) |
| LedG, LedB | -1 | |
| VbatAdc   | 34 | Input-only pin, perfect for ADC |
| UsbDp, Dm | -1 | Classic needs a USB-UART bridge |

> :warning: **Strapping pins on the classic ESP32.** GPIO 2,
> 5, 12, 15 are strapping pins; the bootloader reads them at
> reset. PhantomRF uses GPIO 2 for the LED, GPIO 5 for the
> CC1101 CSN, and GPIO 12/15 for SPI. This is generally
> fine but be aware that the boot behaviour may be affected
> by the LED's state or the CC1101's pull-ups.

## ESP32-C3-DevKitM-1 (RISC-V)

RISC-V single core, no PSRAM, limited features. The M0
firmware runs but the nRF24 SPI is bit-banged (the C3 has
only one hardware SPI, which is used by the flash).

| PinRole | GPIO | Notes |
|---|---|---|
| Nrf24Ce0  | 10 | |
| Nrf24Csn0 | 7  | |
| Nrf24Ce1..4, Csn1..4 | -1 | Not supported on C3 |
| Nrf24Sck  | 4  | Bit-banged (HW SPI is for flash) |
| Nrf24Miso | 5  | Bit-banged |
| Nrf24Mosi | 6  | Bit-banged |
| Cc1101*  | -1 | Not supported (no free SPI) |
| OledSda   | 8  | |
| OledScl   | 9  | |
| ButtonOk  | 3  | |
| ButtonNext, Prev | -1 | 1-button only on C3 |
| LedR      | 2  | |
| LedG, LedB | -1 | |
| VbatAdc   | 0  | |
| UsbDp, Dm | -1 | C3 has native USB; pins 18/19 are used internally |

> :warning: **GPIO 8 and 9 are strapping pins on the C3.**
> The default boot mode reads them as 1 (SPI boot). The OLED
> SDA/SCL don't disturb this when the OLED is idle, but if
> you're using a different I²C device, be careful.

## ESP32-S2-Saola-1

USB OTG native, no Bluetooth. Useful for 2.4 GHz-only builds
where you want to use the cheap S2 modules.

| PinRole | GPIO | Notes |
|---|---|---|
| Nrf24Ce0..4, Csn0..4 | (same as S3) | |
| Cc1101* | (same as S3) | |
| OledSda, OledScl | 8, 9 | |
| ButtonOk, Next, Prev | 10, 14, 21 | |
| LedR, G, B | 5, 6, 7 | |
| VbatAdc | 1 | |
| UsbDp, Dm | 19, 20 | |

> :warning: **No Bluetooth on the S2.** The BLE Serial CLI
> (DESIGN §20.1) is unavailable. USB CDC and the OLED are
> the only control channels.

## Pin-role ↔ GPIO quick reference

For the primary board (N16R8), this is the cheat sheet:

```
  PinRole         GPIO    PinRole         GPIO
  ──────────      ────    ──────────      ────
  Nrf24Ce0         16     Nrf24Ce3         37
  Nrf24Csn0        15     Nrf24Csn3        36
  Nrf24Ce1         18     Nrf24Ce4         39
  Nrf24Csn1        17     Nrf24Csn4        38
  Nrf24Ce2         35     Nrf24Sck         12
  Nrf24Csn2        21     Nrf24Miso        13
                                       Nrf24Mosi        11
  Cc1101Csn0       39     Cc1101Gdo0        2
  Cc1101Gdo2        4     Cc1101Sck        36
  Cc1101Miso       37     Cc1101Mosi       38

  OledSda           8     OledScl           9

  ButtonOk         10     ButtonNext       14
  ButtonPrev       21

  LedR              5     LedG              6
  LedB              7

  VbatAdc           1

  UsbDp            19     UsbDm            20
```

## Next steps

- The wiring diagrams (with these pins shown in context)
  are in [Wiring guide](../getting-started/wiring.md).
- The per-board rationale (why GPIO 16 for CE0, etc.) is in
  [DESIGN.md §8](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#8-pinout-specification).
- The ESP32-S3 variants are detailed in
  [esp32-s3.md](../hardware/esp32-s3.md).
