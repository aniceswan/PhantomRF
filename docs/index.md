# PhantomRF

> *Make the invisible visible. Then make it selective.*

**PhantomRF** is an ESP32-S3 based RF research and education platform that
combines a 2.4 GHz nRF24L01+PA+LNA module, a sub-GHz CC1101 module, the
ESP32-S3's built-in WiFi/BLE, and a real-time web UI into a single, fully
open-source device.

It unifies Bluetooth classic jam, BLE spam, WiFi deauth, beacon flood,
sub-GHz jamming, raw record/replay, and a live spectrum analyzer behind
one cohesive firmware.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../LICENSE)
[![Status: Pre-release](https://img.shields.io/badge/Status-Pre--release-orange)]()
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-blue)]()
[![Build: PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange)]()
[![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue)]()
[![Version](https://img.shields.io/badge/Version-0.1.0-red)]()

---

## Quick links

| Link | Purpose |
|---|---|
| [GitHub repository](https://github.com/anomalyco/PhantomRF) | Source code, issues, releases |
| [Web Flasher](https://anomalyco.github.io/PhantomRF/flasher/) | Flash your board from a browser, no toolchain required |
| [Discord server](https://discord.gg/phantomrf) | Real-time help, project chat |
| [DESIGN.md](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md) | Deep design specification |
| [Changelog](../CHANGELOG.md) | What changed in each release |

---

## TL;DR

1. Buy the hardware — see [Hardware required](getting-started/installation.md#hardware-required).
2. Wire it up — see [Wiring guide](getting-started/wiring.md).
3. Flash the firmware — see [First flash](getting-started/first-flash.md).
4. Connect to the `PhantomRF-XXXX` WiFi AP and open <http://192.168.4.1>.
5. Default login is `admin` / `<password printed to USB serial>`.

> :warning: **READ THE [LEGAL DISCLAIMER](legal/disclaimer.md) BEFORE USING THIS
> DEVICE.** Jamming, deauthenticating, or otherwise interfering with radio
> communications is illegal in most jurisdictions without explicit
> authorisation from the spectrum regulator and the network operator.

---

## What can it do?

PhantomRF supports a growing list of attacks and analyses. Every one of
them is exposed through three control surfaces that share the same
underlying code (DESIGN §20.1):

- **Web UI** at <http://192.168.4.1/> — recommended for daily use.
- **USB CDC Serial** at 115200 baud — robust, always works.
- **OLED + 1/2/3 buttons** — for headless operation in the field.
- **BLE Serial** — when 2.4 GHz is jammed, fall back to Bluetooth control.

| Feature | Status | Docs |
|---|---|---|
| Bluetooth classic jam | M0 | [bluetooth-jam.md](features/bluetooth-jam.md) |
| WiFi deauth / beacon flood | M0 | [wifi-deauth.md](features/wifi-deauth.md) |
| 2.4 GHz constant-carrier jam | M0 | [nrf24-const-carrier.md](algorithms/nrf24-const-carrier.md) |
| Sub-GHz jam / record / replay | M0 | [cc1101-async-serial.md](algorithms/cc1101-async-serial.md) |
| Spectrum analyzer (both bands) | M0 | [spectrum-analyzer.md](features/spectrum-analyzer.md) |
| BLE spam (Apple / Samsung) | M1 | — |
| PMKID capture | M1 | — |
| 5-radio parallel nRF24 sweep | M2 | — |

---

## Table of contents

### Getting started

- [Installation](getting-started/installation.md) — what to buy, how to assemble.
- [Wiring](getting-started/wiring.md) — every connection, ASCII diagrams included.
- [First flash](getting-started/first-flash.md) — three ways to get firmware on the board.
- [First use](getting-started/first-use.md) — connect, log in, run a test attack.

### Hardware

- [ESP32-S3 board](hardware/esp32-s3.md) — variants, GPIOs, strapping pins.
- [nRF24L01 module](hardware/nrf24l01.md) — pinout, power, antenna choices.
- [CC1101 module](hardware/cc1101.md) — pinout, frequency bands, registers.
- [Power budget](hardware/power-budget.md) — worst-case current, battery life.

### Features

- [Bluetooth jam](features/bluetooth-jam.md)
- [WiFi deauth](features/wifi-deauth.md)
- [Sub-GHz jam](features/subghz-jam.md)
- [Spectrum analyzer](features/spectrum-analyzer.md)

### Algorithms (deep dives)

- [nRF24 constant carrier](algorithms/nrf24-const-carrier.md)
- [CC1101 async-serial](algorithms/cc1101-async-serial.md)
- [802.11 deauth frames](algorithms/802.11-deauth.md)
- [Channel math reference](algorithms/channel-math.md)

### Reference

- [Pinout per board](reference/pinout.md)
- [CLI commands](reference/commands.md)
- [NVS settings](reference/settings.md)
- [REST API](reference/api.md)
- [WebSocket protocol](reference/websocket-protocol.md)
- [Troubleshooting](reference/troubleshooting.md)

### Legal

- [Disclaimer](legal/disclaimer.md) — read this first.
- [Jurisdictions](legal/jurisdictions.md) — country-by-country notes.
- [Ethics](legal/ethics.md) — what we will and won't add.

---

## Next steps

- New user? Start with [Installation](getting-started/installation.md).
- Already have the hardware? Jump to [Wiring](getting-started/wiring.md).
- Want the technical story? Read [DESIGN.md](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md).
- Curious about a specific algorithm? The [Algorithms](algorithms/) section
  has dedicated pages for each.
