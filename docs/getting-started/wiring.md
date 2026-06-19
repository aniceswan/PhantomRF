# Wiring guide

> Every connection you need to make, with ASCII diagrams for the three
> most common build configurations.

This page is the authoritative wiring reference. The pin numbers come
from `src/hal/Board.cpp`; the rationale (why this GPIO, not that one)
is in [DESIGN.md §8](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#8-pinout-specification).

---

## Table of contents

- [Picking a configuration](#picking-a-configuration)
- [Configuration A — Minimum: 1 nRF24](#configuration-a--minimum-1-nrf24)
- [Configuration B — Recommended: 1 nRF24 + 1 CC1101](#configuration-b--recommended-1-nrf24--1-cc1101)
- [Configuration C — Full: 5 nRF24 + 1 CC1101](#configuration-c--full-5-nrf24--1-cc1101)
- [OLED connection (any config)](#oled-connection-any-config)
- [Button connection (any config)](#button-connection-any-config)
- [Power tree](#power-tree)
- [Next steps](#next-steps)

---

## Picking a configuration

The PhantomRF firmware auto-detects which radios are present at boot by
reading the NVS key `nrf24.count` and the presence of the CC1101 SPI
device. You can have:

| Config | nRF24 | CC1101 | What you can do |
|---|---|---|---|
| **A** | 1 | 0 | 2.4 GHz only (BT, BLE, WiFi, nRF24 attacks) |
| **B** | 1 | 1 | 2.4 GHz **and** sub-GHz (recommended) |
| **C** | 5 | 1 | 2.4 GHz with 5-radio parallel sweep (M2 feature) |

We strongly recommend **Configuration B** for your first build. It
exercises every code path and is enough to demonstrate every
documented attack.

> :information_source: **M0 limitation.** The M0 firmware supports up
> to 2 nRF24 modules. Configuration C is for M2 (see
> [DESIGN.md §19](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#19-roadmap--milestones)).
> Building it now is fine — the extra radios will just be unused.

The pin numbers below are for the **ESP32-S3-WROOM-1-N16R8** (primary
target). For the other variants, see
[Pinout per board](../reference/pinout.md).

## Configuration A — Minimum: 1 nRF24

This is the smallest possible build. It exercises the OLED, buttons,
USB CDC, WiFi AP, and one nRF24 module. No sub-GHz support.

### Connection table

| ESP32-S3 GPIO | Connects to | nRF24 pin | Notes |
|---|---|---|---|
| 3V3 | VCC | VCC | Use the 3V3 rail, NOT 5V — the nRF24 is 3.3 V only |
| GND | GND | GND | Common ground |
| GPIO 16 | → | CE | Chip enable |
| GPIO 15 | → | CSN | Chip-select (active low) |
| GPIO 12 | → | SCK | SPI clock (shared bus) |
| GPIO 13 | → | MISO | SPI MISO |
| GPIO 11 | → | MOSI | SPI MOSI |
| GPIO 8  | ↔ | SDA | OLED (if installed) |
| GPIO 9  | ↔ | SCL | OLED (if installed) |
| GPIO 10 | ↔ | OK  | Centre button (active-low, internal pull-up) |
| GPIO 5  | → | LED-R | Status LED (red channel) |
| USB D+ | — | — | GPIO 19, native USB |
| USB D- | — | — | GPIO 20, native USB |

### ASCII diagram

```
                +-----------------------+
                |     ESP32-S3 DevKit   |
                |                       |
        3V3 ----+---> nRF24 VCC         |
        GND ----+---> nRF24 GND         |
                |                       |
       GPIO 16 ----> nRF24 CE           |
       GPIO 15 ----> nRF24 CSN          |
       GPIO 12 ----> nRF24 SCK          |
       GPIO 13 <---- nRF24 MISO         |
       GPIO 11 ----> nRF24 MOSI         |
                |                       |
        GPIO 8 <---> OLED SDA           |
        GPIO 9 <---> OLED SCL           |
                |                       |
       GPIO 10 ---+--> OK button --- GND|
                |                       |
       USB-C  ---+--> Native USB        |
                +-----------------------+
```

### Power notes

- The nRF24+PA+LNA peaks at **115 mA** during TX at +20 dBm. The
  ESP32-S3's onboard 3V3 regulator is rated for 600 mA, which is
  enough for both. If you add more radios or a 5 V power supply, add
  an external 3V3 regulator.
- Decoupling: 100 nF ceramic + 10 µF electrolytic across the
  nRF24's VCC/GND, as close to the module as possible. The PA-LNA
  has a switching supply on board that injects noise.

## Configuration B — Recommended: 1 nRF24 + 1 CC1101

This is the build we recommend. It includes the OLED, three buttons,
the RGB status LED, and the CC1101 sub-GHz module on a separate SPI
bus. It exercises every feature of the M0 firmware.

### Connection table

| ESP32-S3 GPIO | Connects to | Module pin | Notes |
|---|---|---|---|
| 3V3 | VCC | nRF24 VCC | — |
| GND | GND | nRF24 GND, CC1101 GND, OLED GND | Common ground |
| GPIO 16 | → | nRF24 CE | — |
| GPIO 15 | → | nRF24 CSN | — |
| GPIO 12 | → | nRF24 SCK | **Shared** with CC1101 SCK (different CSN) |
| GPIO 13 | ← | nRF24 MISO | **Shared** with CC1101 MISO |
| GPIO 11 | → | nRF24 MOSI | **Shared** with CC1101 MOSI |
| 3V3 | VCC | CC1101 VCC | — |
| GPIO 36 | → | CC1101 SCK | Actually unused — we use the shared SPI above |
| GPIO 37 | ← | CC1101 MISO | Actually unused — we use the shared SPI above |
| GPIO 38 | → | CC1101 MOSI | Actually unused — we use the shared SPI above |
| GPIO 39 | → | CC1101 CSN | — |
| GPIO 2  | ← | CC1101 GDO0 | Optional IRQ; pulled to status flag in M0 |
| GPIO 4  | ← | CC1101 GDO2 | Optional; tied to FIFO threshold in M1 |
| GPIO 8  | ↔ | OLED SDA | — |
| GPIO 9  | ↔ | OLED SCL | — |
| GPIO 10 | ← | OK button | Active-low, internal pull-up |
| GPIO 14 | ← | Next button | Active-low, internal pull-up |
| GPIO 21 | ← | Prev button | Active-low, internal pull-up |
| GPIO 5  | → | LED-R | — |
| GPIO 6  | → | LED-G | — |
| GPIO 7  | → | LED-B | — |
| GPIO 1  | ← | VBAT/2 | Through a 100 kΩ / 100 kΩ divider |

> :information_source: **Why share the SPI bus?** The nRF24 and
> CC1101 use the same SPI mode (mode 0, MSB-first) and a maximum
> clock of 10 MHz. The two CSN pins let us put both on the same bus
> and address them independently. This frees up three GPIOs and
> matches the original W0rthlessS0ul nRF24+CC1101 build.

### ASCII diagram

```
                 +----------------------+
                 |      ESP32-S3        |
                 |                      |
   3V3 ---+---->-+ nRF24 VCC            |
          +---->-+ CC1101 VCC           |
          +---->-+ OLED VCC             |
                 |                      |
  GPIO 16 ----+  |                      |
  GPIO 15 ----+  +---> nRF24 CE/CSN     |
  GPIO 12 ----+--------> SCK (shared)    |
  GPIO 13 <---+--------> MISO (shared)   |
  GPIO 11 ----+--------> MOSI (shared)   |
                 |                      |
  GPIO 39 ----+  +---> CC1101 CSN       |
  GPIO 2  <---+  +---> CC1101 GDO0      |
                 |                      |
  GPIO 8  <--->-+---> OLED SDA          |
  GPIO 9  <--->-+---> OLED SCL          |
                 |                      |
  GPIO 10 ---+   +---> OK button --- GND|
  GPIO 14 ---+   +---> Next btn  --- GND|
  GPIO 21 ---+   +---> Prev btn  --- GND|
                 |                      |
  GPIO 5  ---> LED-R                     |
  GPIO 6  ---> LED-G                     |
  GPIO 7  ---> LED-B                     |
                 |                      |
  GPIO 1  <---+ VBAT/2 (resistor divider)|
                 |                      |
  USB-C ----+   +---> Native USB        |
                 +----------------------+
```

### Power notes

- Peak system current: **160 mA** (nRF24 TX + CC1101 RX + ESP32 WiFi
  TX + OLED on).
- The ESP32-S3's onboard regulator can supply this comfortably. If
  you power the board from a weak USB charger, you may see brownouts
  when both radios transmit at once. See
  [Power budget](../hardware/power-budget.md) for measurements.

## Configuration C — Full: 5 nRF24 + 1 CC1101

This is the maximum-build configuration. It supports the M2 "5-radio
parallel sweep" feature and lets the device cover the entire 2.4 GHz
band simultaneously (one radio per quarter of the band).

> :warning: **5 radios will push the SPI bus hard.** All 5 nRF24s
> share the same SCK/MISO/MOSI and have a unique CE + CSN. The
> firmware uses SPI transactions to ensure the bus is in a known
> state before talking to each radio. M0 ships with single-radio
> support — this build is for early adopters.

### Connection table

| ESP32-S3 GPIO | Connects to | nRF24 pin |
|---|---|---|
| GPIO 16 | → | nRF24 #0 CE |
| GPIO 15 | → | nRF24 #0 CSN |
| GPIO 18 | → | nRF24 #1 CE |
| GPIO 17 | → | nRF24 #1 CSN |
| GPIO 35 | → | nRF24 #2 CE |
| GPIO 21 | → | nRF24 #2 CSN (also Prev button — see below) |
| GPIO 37 | → | nRF24 #3 CE |
| GPIO 36 | → | nRF24 #3 CSN |
| GPIO 39 | → | nRF24 #4 CE |
| GPIO 38 | → | nRF24 #4 CSN |
| GPIO 12 | → | nRF24 #N SCK (shared) |
| GPIO 13 | ← | nRF24 #N MISO (shared) |
| GPIO 11 | → | nRF24 #N MOSI (shared) |

> :warning: **GPIO 21 is shared between nRF24 #2 CSN and the Prev
> button.** The firmware disambiguates by direction: nRF24 CSN is
> output, Prev button is input. **The Prev button is unavailable in
> this configuration.** Either drop to a 2-button config or remove
> nRF24 #2.

### ASCII diagram (truncated to 2 radios for brevity)

```
              +----------------------+
              |      ESP32-S3        |
              |                      |
  GPIO 12 --+ +----> SCK  (to all)   |
  GPIO 13 <-+ +----> MISO (to all)   |
  GPIO 11 --+ +----> MOSI (to all)   |
              |                      |
  GPIO 16 ---> NRF24 #0 CE            |
  GPIO 15 ---> NRF24 #0 CSN           |
  GPIO 18 ---> NRF24 #1 CE            |
  GPIO 17 ---> NRF24 #1 CSN           |
  GPIO 35 ---> NRF24 #2 CE            |
  GPIO 21 ---> NRF24 #2 CSN (no Prev) |
  GPIO 37 ---> NRF24 #3 CE            |
  GPIO 36 ---> NRF24 #3 CSN           |
  GPIO 39 ---> NRF24 #4 CE            |
  GPIO 38 ---> NRF24 #4 CSN           |
              |                      |
  (CC1101, OLED, buttons as in Config B)
              |                      |
  USB-C -----+------> Native USB      |
              +----------------------+
```

## OLED connection (any config)

The SSD1306 OLED uses I²C. Only two data wires (SDA, SCL) and two
power wires (VCC, GND). Pull-ups are not required — most modules
include 10 kΩ pull-ups on the breakout board.

| OLED pin | ESP32-S3 GPIO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

The I²C address is **0x3C** for almost all SSD1306 modules. If your
display doesn't show anything, try **0x3D** — that's the alternative
address. You can change it in `src/ui/oled/OledMenu.cpp`.

## Button connection (any config)

Buttons are active-low with the ESP32's internal pull-ups enabled in
software. No external resistors required.

| Button | ESP32-S3 GPIO | Behaviour |
|---|---|---|
| OK   | GPIO 10 | Centre / select |
| Next | GPIO 14 | Move forward in menu |
| Prev | GPIO 21 | Move backward in menu |

Each button is wired between the GPIO and GND. The firmware enables
the internal pull-up in `Board::setupPins()`.

For the 1-button config, only OK is wired. For the 2-button config,
OK + Next. The 3-button config is the default.

## Power tree

```
  +5V (USB-C or external) ─── 3V3 LDO ─── +3V3 rail
                                       ├── ESP32-S3 VCC
                                       ├── nRF24 VCC
                                       ├── CC1101 VCC
                                       └── OLED VCC

  Battery (LiPo 3.7 V) ── TP4056 charger ── 3V3 LDO ── +3V3 rail
       (optional)
```

> :warning: **Do not power the nRF24 from 5 V.** The chip is
> absolute-max 3.6 V. The PA-LNA's onboard regulator can take up to
> 7 V on the "VIN" pin of some breakout boards, but the "VCC" pin is
> 3.3 V only. Always connect VCC to 3V3.

## Next steps

- Done wiring? Head to [First flash](first-flash.md) to install the
  firmware.
- Got a "radio not responding" error? See
  [Troubleshooting](../reference/troubleshooting.md#radio-not-detected).
- Need the per-board pinout? See
  [Pinout per board](../reference/pinout.md).
