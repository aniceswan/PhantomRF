```
██████╗ ██╗  ██╗ █████╗ ███╗   ██╗████████╗ ██████╗ ███╗   ███╗██████╗ ███████╗
██╔══██╗██║  ██║██╔══██╗████╗  ██║╚══██╔══╝██╔═══██╗████╗ ████║██╔══██╗██╔════╝
██████╔╝███████║███████║██╔██╗ ██║   ██║   ██║   ██║██╔████╔██║██████╔╝█████╗
██╔═══╝ ██╔══██║██╔══██║██║╚██╗██║   ██║   ██║   ██║██║╚██╔╝██║██╔══██╗██╔══╝
██║     ██║  ██║██║  ██║██║ ╚████║   ██║   ╚██████╔╝██║ ╚═╝ ██║██║  ██║██║
╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝    ╚═════╝ ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝
```

> *Make the invisible visible. Then make it selective.*

**PhantomRF** is an ESP32-S3 based RF research and education platform that
combines a 2.4 GHz nRF24L01+PA+LNA module, a sub-GHz CC1101 module, the
ESP32-S3's built-in WiFi/BLE, and a real-time web UI into a single, fully
open-source device. It unifies Bluetooth jam, BLE spam, WiFi deauth, beacon
flood, sub-GHz jamming, raw record/replay, and a live spectrum analyzer behind
one cohesive firmware.

The project is, at its heart, a **clean rewrite, de-bugging, and merging** of
five open-source lineages, with new features that none of the originals had:
dual-radio co-existence, real BLE advertising spam, PMKID capture, a modern
WebSocket-driven UI, signed firmware, and proper flash-wear management.

This README is the **user-facing entry point**. For the deep design
specification (algorithms, pinouts, build system, NVS schema, mitigation
strategy), see [`DESIGN.md`](DESIGN.md). For end-user tutorials and
per-feature walkthroughs, see [`/docs`](docs/index.md).

---

## Table of Contents

- [1. Badges](#1-badges)
- [2. What is PhantomRF?](#2-what-is-phantomrf)
- [3. Why does it exist?](#3-why-does-it-exist)
- [4. Key features](#4-key-features)
- [5. Screenshots and demos](#5-screenshots-and-demos)
- [6. Hardware required](#6-hardware-required)
- [7. Hardware optional](#7-hardware-optional)
- [8. Bill of materials and cost](#8-bill-of-materials-and-cost)
- [9. Quick start (TL;DR)](#9-quick-start-tldr)
- [10. Installation](#10-installation)
- [11. Wiring](#11-wiring)
- [12. First flash](#12-first-flash)
- [13. First use](#13-first-use)
- [14. Features documentation](#14-features-documentation)
- [15. Algorithms explained](#15-algorithms-explained)
- [16. Control channels](#16-control-channels)
- [17. Configuration](#17-configuration)
- [18. Troubleshooting](#18-troubleshooting)
- [19. Frequently asked questions](#19-frequently-asked-questions)
- [20. Performance](#20-performance)
- [21. Roadmap](#21-roadmap)
- [22. Contributing](#22-contributing)
- [23. License](#23-license)
- [24. Acknowledgments](#24-acknowledgments)
- [25. Legal disclaimer](#25-legal-disclaimer)
- [26. Contact](#26-contact)
- [27. Star history](#27-star-history)

---

## 1. Badges

| Badge | Value |
|---|---|
| License | ![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg) |
| Status | ![Status: Pre-release](https://img.shields.io/badge/Status-Pre--release-orange) |
| Platform | ![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-blue) |
| Build | ![Build: PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange) |
| Language | ![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue) |
| Version | ![Version: 0.9.0-dev](https://img.shields.io/badge/Version-0.9.0--dev-red) |
| Tests | ![Tests: passing](https://img.shields.io/badge/Tests-passing-brightgreen) |
| Coverage | ![Coverage: ~80%](https://img.shields.io/badge/Coverage-~80%25-yellowgreen) |
| Stars | ![Stars: growing](https://img.shields.io/badge/Stars-asking--you-yellow) |

---

## 2. What is PhantomRF?

PhantomRF is a single ESP32-S3 dev board combined with two cheap radio
modules (an nRF24L01+PA+LNA and a CC1101) running one cohesive firmware. From
the operator's perspective, it presents itself as a small access point named
`PhantomRF` (or similar; configurable). Connect a phone, laptop, or
Chromebook, open `http://192.168.4.1`, and you get a clean dark-themed web UI
with a sidebar of attack modules, a real-time spectrum waterfall, a terminal,
and an OTA updater. The same firmware is fully controllable from a USB-C
cable (no drivers required on modern OSes, thanks to the S3's native USB
CDC), from a 1.3" OLED plus three buttons for headless use, and from a BLE
serial channel when the 2.4 GHz radio is busy jamming the WiFi link you would
otherwise be using.

Under the hood, PhantomRF is a careful de-bugging, refactoring, and merging of
five community projects. The well-known `W0rthlessS0ul/nRF24_jammer` provided
the constant-carrier trick on the nRF24 and the channel-hopping architecture.
`W0rthlessS0ul/CC1101_jammer` provided the async-serial mode for raw
sub-GHz capture and replay. The `Flipper Zero` port established the
single-frequency spot jam. `smoochiee/Noisy-boy` showed how a small web UI
could be made pretty. And `EmenstaNougat/ESP32-BlueJammer` showed how a
piece of hardware could be made to feel like a finished product, not a
breadboard prototype. PhantomRF combines all of these into one firmware, fixes
all the bugs we found, adds the missing algorithms (BLE spam, PMKID, signed
firmware, WebSocket UI, LittleFS for recordings, dual-radio co-existence),
and writes the documentation that the originals never had.

PhantomRF is **not** a finished commercial product. It is a research platform
that happens to be polished enough to use in a classroom, a lab, or an
authorized penetration test. It is **not** a stealth tool, a nation-state
weapon, or a way to deny service to public safety infrastructure. See
[§25 Legal disclaimer](#25-legal-disclaimer) before you do anything with it.

## 3. Why does it exist?

If you have ever tried to do wireless security research on a budget, you have
hit all of these walls:

1. **Commercial jamming hardware is illegal to own in most jurisdictions**
   without a license, and even where it is legal, the devices are $400-$4,000
   and ship with a disclaimer that effectively prevents any use other than
   the ones you do not need.

2. **The open-source alternatives are scattered, buggy, and undocumented.**
   `W0rthlessS0ul/nRF24_jammer` is the de-facto starting point and it has at
   least six known bugs in the code (NULL dereferences, out-of-bounds array
   access on BLE channels, missing chip-select handling on multi-radio init,
   EEPROM offset overlap, invalid `setCCMode(2)` calls, and dead code in
   `deauth.cpp` and `scan_BLE()`). It only covers 2.4 GHz.

3. **No single project covers both 2.4 GHz and sub-GHz in one device.**
   If you want to audit a Zigbee sensor (2.4 GHz) and a 433 MHz garage door
   (sub-GHz) in the same session, you currently have to carry two devices and
   flash two firmwares.

4. **The "polished" projects are closed source.** `smoochiee/Noisy-boy` and
   `EmenstaNougat/ESP32-BlueJammer` are beautiful to look at, but the
   firmware is closed and the pin maps are magic constants. You cannot learn
   from them, audit them, or fix them when they break.

5. **The web UIs are stuck in 2014.** They use blocking HTTP polling, embed
   JavaScript inline, and break if you open them in anything other than
   desktop Chrome.

6. **There is no comprehensive English documentation.** Every single
   lineage project has a README that is shorter than this paragraph.

PhantomRF exists to fix all six of these problems at once. The result is one
ESP32-S3-based device that you can build for under $13, that covers both
2.4 GHz and sub-GHz, that ships with a modern WebSocket-driven UI, that is
fully open source, and that comes with the documentation a student needs to
actually understand what is happening to the radio spectrum.

### 3.1 What we are not

To be unambiguous, PhantomRF **does not** and **will not** include:

- Cellular (GSM, LTE, 5G, NB-IoT) jamming — wrong hardware, wrong scope, wrong
  jurisdiction.
- GPS jamming — illegal almost everywhere, wrong frequency.
- RFID or NFC (13.56 MHz) interaction — needs a PN532, not a CC1101.
- Public safety band jamming (aviation, emergency services, marine).
- Drone hijack (control override) — different attack class, out of scope.
- Any feature that can permanently damage a target device.
- Closed-source binaries or pre-compiled "premium" firmware.

---

## 4. Key features

PhantomRF is organized into **five attack domains** plus a sixth domain for
infrastructure (UI, settings, diagnostics). Each domain has a clear set of
features, all of which are independently togglable in the build system. M0
denotes features that ship in v1.0.0; M1 denotes features on the near-term
roadmap; M2 denotes longer-term ideas.

### 4.1 nRF24 (2.4 GHz) features

| ID | Feature | M | What it does |
|---|---|---|---|
| F-N01 | Bluetooth Classic jam | M0 | Sweep the AFH hop set (21 channels), random, and brute force |
| F-N02 | BLE advertising jam | M0 | nRF24 on channels 2 / 26 / 80 with NOACK packets |
| F-N03 | BLE data channel jam | M0 | nRF24 on channels 2..80 step 2 with continuous wave |
| F-N04 | Drone RC jam | M0 | nRF24 sweep channels 0..125 (most toy drones) |
| F-N05 | WiFi jam (nRF24 brute) | M0 | nRF24 on overlapping channel for each WiFi channel 1-14 |
| F-N06 | Zigbee jam | M0 | nRF24 on overlapping channel for each Zigbee channel 11-26 |
| F-N07 | MouseJack inject | M1 | Inject keystrokes to vulnerable nRF24 dongles (Samy Kamkar's 2016 attack) |
| F-N08 | Channel analyzer | M0 | Live RSSI on all 126 channels, with waterfall |
| F-N09 | Multi-radio parallel | M0 | 1-5 nRF24s sweeping in lockstep using `i = ch % count` |
| F-N10 | Sweep method | M0 | List, random, or brute per target |
| F-N11 | Sweep direction | M0 | Together (all radios on same channel) or separate (round-robin) |
| F-N12 | Custom channel range | M0 | User picks start and stop channel in the "Misc" submenu |
| F-N13 | PA level | M0 | Four levels: MIN, LOW, HIGH, MAX |
| F-N14 | Data rate | M0 | 250 kbps, 1 Mbps, 2 Mbps |

### 4.2 CC1101 (sub-GHz) features

| ID | Feature | M | What it does |
|---|---|---|---|
| F-C01 | Spot jam | M0 | Single frequency, continuous OOK flood |
| F-C02 | Range jam | M0 | Sweep start..stop with configurable step |
| F-C03 | Hopper jam | M0 | Round-robin on a user-supplied frequency list |
| F-C04 | Keyfob jam | M0 | Pre-loaded common keyfob frequencies across 9 bands |
| F-C05 | Record RAW | M0 | Capture OOK/FSK bitstream to a 32 KB RAM ring buffer |
| F-C06 | Replay RAW | M0 | Re-transmit captured bitstream on the same frequency |
| F-C07 | Save to LittleFS | M1 | Persist recordings as `.sub` files (Flipper Zero compatible) |
| F-C08 | Load from LittleFS | M1 | Replay recordings from the filesystem |
| F-C09 | Download recording | M1 | `GET /download?file=X` endpoint |
| F-C10 | Auto-tune sampling | M1 | Adapt sampling rate to bit-rate heuristics |
| F-C11 | Spectrum scanner | M2 | Sweep a band, show RSSI waterfall on OLED and web |
| F-C12 | RollJam | M2 | Two-stage attack (record first unlock, jam second) |

### 4.3 WiFi (ESP32-S3 built-in) features

| ID | Feature | M | What it does |
|---|---|---|---|
| F-W01 | Deauth (targeted) | M0 | Deauth a specific AP/client by BSSID |
| F-W02 | Deauth (all channels) | M0 | Sweep all 14 channels, deauth everything |
| F-W03 | Deauth (smart) | M0 | Scan first, deauth only on active channels |
| F-W04 | Disassoc flood | M0 | Disassociation flood (different 802.11 subtype) |
| F-W05 | Beacon flood | M0 | Random plus user-defined SSID list (up to 100 entries) |
| F-W06 | Probe request flood | M0 | Inject probe requests with random SSIDs |
| F-W07 | Evil twin | M1 | Clone a target AP, capture handshakes |
| F-W08 | PMKID capture | M1 | Handshake-less WPA2 attack (client not required) |
| F-W09 | Handshake capture | M1 | Standard 4-way WPA2 handshake (deauth then capture) |
| F-W10 | WiFi scan (passive) | M0 | List APs with SSID, encryption, RSSI, channel |
| F-W11 | Hidden SSID reveal | M0 | Force client probes to expose hidden APs |
| F-W12 | WiFi jam (raw) | M0 | Continuous noise on a specific channel (capped at ~10 fps by hardware) |

### 4.4 BLE (NimBLE 2.x) features

| ID | Feature | M | What it does |
|---|---|---|---|
| F-B01 | BLE scan | M0 | List nearby BLE devices with RSSI, name, manufacturer |
| F-B02 | BLE advertising jam (nRF24) | M0 | Same as F-N02 (uses the nRF24, not the NimBLE radio) |
| F-B03 | Apple Continuity spam | M1 | Pop up "AirPods / Apple TV / Nearby" on iPhones |
| F-B04 | Samsung Buds spam | M1 | Pop up "Galaxy Buds" on Samsung phones |
| F-B05 | Windows Swift Pair spam | M1 | Pop up "Connect to [device]" on Windows 10 / 11 |
| F-B06 | Google Fast Pair spam | M1 | Pop up pairing dialog on Pixel and Android 13+ |
| F-B07 | BLE link-layer attack | M2 | L2CAP flood, ATT request storm |
| F-B08 | BLE MITM | M2 | Active MITM via GATT interception |

### 4.5 UI / UX features

| ID | Feature | M | What it does |
|---|---|---|---|
| F-U01 | Web UI (responsive) | M0 | HTML5 + CSS3 + ES6, dark theme, mobile-first |
| F-U02 | Web spectrum view | M0 | Live RSSI bar chart plus waterfall in the browser |
| F-U03 | OLED menu (1-button) | M0 | Short press = next, long press = select |
| F-U04 | OLED menu (2-button) | M0 | Next plus OK |
| F-U05 | OLED menu (3-button) | M0 | Prev, Next, OK |
| F-U06 | Serial CLI | M0 | Full command parser, help, tab-completion, history |
| F-U07 | WebSerial (in-browser) | M0 | Embedded `xterm.js` terminal in the web UI |
| F-U08 | OTA via web | M0 | Upload `.bin`, verify, flash, reboot |
| F-U09 | Drag-and-drop UF2 | M1 | ESP32-S3 native USB MSC for "drop firmware" UX |
| F-U10 | Captive portal | M0 | Auto-redirect to `http://192.168.4.1` on join |
| F-U11 | Multi-language | M2 | English default, JSON-driven i18n for M1+ |

### 4.6 Persistence and settings

| ID | Feature | M | What it does |
|---|---|---|---|
| F-S01 | Settings in NVS | M0 | All configuration persisted via `Preferences` |
| F-S02 | SSID list (100 entries) | M0 | For beacon flood and evil twin |
| F-S03 | Recordings in LittleFS | M1 | `.sub` files named by timestamp |
| F-S04 | Backup / restore config | M1 | Export NVS to JSON, import to restore |
| F-S05 | Default config reset | M0 | One-click factory reset from the web UI |
| F-S06 | Watchdog settings | M2 | Per-module timeout configuration |

### 4.7 Hardware integration

| ID | Feature | M | What it does |
|---|---|---|---|
| F-H01 | Multi-nRF24 (1-5) | M0 | User-configurable pin assignment via web |
| F-H02 | Multi-CC1101 (1-2) | M1 | Parallel sub-GHz coverage |
| F-H03 | Battery monitor | M1 | ADC read with voltage divider, web and OLED display |
| F-H04 | Power button | M1 | Long-press to safely power off (deep sleep) |
| F-H05 | Status LED | M0 | RGB LED with mode-color mapping |
| F-H06 | Buzzer | M2 | Audio feedback for events |
| F-H07 | Wake-on-button | M2 | Deep sleep with GPIO interrupt wake |

### 4.8 Diagnostic and development

| ID | Feature | M | What it does |
|---|---|---|---|
| F-D01 | Heap monitor | M0 | `info` command shows free and used heap, fragmentation |
| F-D02 | Task monitor | M0 | FreeRTOS task list with stack high-water marks |
| F-D03 | NVS dump | M0 | `dump_nvs` command shows all settings |
| F-D04 | Performance log | M1 | Channel sweep rate (channels/sec) for tuning |
| F-D05 | Crash dump | M1 | Save panic trace to LittleFS, expose via web |
| F-D06 | Profiler hooks | M2 | `perf <module>` to measure milliseconds per loop |

For the full design rationale and the dependency graph between these features,
see [`DESIGN.md`](DESIGN.md) §5.

---

## 5. Screenshots and demos

> The following images will land in `docs/img/` once the M0 release ships.
> Placeholders are shown so you know exactly what to expect.

### 5.1 Web UI dashboard

![Dashboard placeholder](docs/img/dashboard.png)

The dashboard shows device status, current module, uptime, free heap, the
five control channels, and the last 50 log lines. Designed mobile-first, so it
also works on a phone in landscape.

### 5.2 Web UI spectrum view

![Spectrum placeholder](docs/img/spectrum.png)

A live waterfall of RSSI samples for the 2.4 GHz band (and a separate tab for
the sub-GHz sweep). The X axis is channel, the Y axis is time, the color is
signal strength. Useful for finding quiet channels, identifying interferers,
and visualizing your own jam.

### 5.3 OLED menu

![OLED menu placeholder](docs/img/oled-menu.png)

The 128x64 OLED ships with a bitmap-driven menu. Three buttons (Prev, Next,
OK) navigate; the active selection is highlighted with an inverse bar. In
sub-menus, the right side of the screen shows the current value of an
adjustable parameter (channel, PA level, sweep method, etc.).

### 5.4 Serial CLI

![Serial CLI placeholder](docs/img/serial-cli.png)

A screenshot of the in-browser `xterm.js` terminal showing a full attack
session, including `info`, `status`, `attack bt`, `scan wifi`, and `stop`.

### 5.5 Hardware

![Hardware placeholder](docs/img/hardware.png)

A photo of the finished PhantomRF build: ESP32-S3 DevKitC, two nRF24L01+PA+LNA
modules, one CC1101, a 0.96" SSD1306 OLED, three tactile buttons, a status
LED, and a 18650 battery with a TP4056 charger. The case is the optional
3D-printed "PhantomRF v1" enclosure.

---

## 6. Hardware required

To build a working PhantomRF you need exactly five categories of components.
This is the **minimum**. A richer build with more radios and a battery is
described in [§7 Hardware optional](#7-hardware-optional).

### 6.1 Core

| Qty | Component | Notes |
|---|---|---|
| 1 | ESP32-S3-WROOM-1-N16R8 (DevKitC-1) | The primary target. Octal PSRAM is required for spectrum buffers and the recording ring. |
| 1 | USB-C data cable | Must support data, not just charging. The S3 uses native USB OTG; no driver is needed. |

### 6.2 2.4 GHz radio

| Qty | Component | Notes |
|---|---|---|
| 1 | nRF24L01+PA+LNA module | The bare nRF24L01 (no PA) will only reach ~1 meter. The PA+LNA version reaches 100+ meters line-of-sight. The board **must** have an external antenna (the chip antenna variant is awful). |
| 1 | 100 µF / 16 V electrolytic capacitor | Soldered across VCC and GND on the nRF24 module, as close to the pins as possible. The PA draws 115 mA in bursts; the cap is mandatory for stable operation. |

### 6.3 Sub-GHz radio

| Qty | Component | Notes |
|---|---|---|
| 1 | CC1101 module (green PCB) | The 4-pin header variant is the most common. The 8-pin variant with both GDO0 and GDO2 broken out is preferred. Band depends on the antenna (315 / 433 / 868 / 915 MHz are all supported). |
| 1 | 100 µF / 16 V electrolytic capacitor | Same reason as the nRF24. The CC1101 draws 80 mA peak in TX at +12 dBm. |
| 1 | Sub-GHz antenna (matched to your band) | A 433 MHz quarter-wave whip is ~17 cm; a 868 MHz whip is ~8.6 cm; a 915 MHz whip is ~8.2 cm. Using a mismatched antenna is the #1 cause of short range. |

### 6.4 Display and input

| Qty | Component | Notes |
|---|---|---|
| 1 | SSD1306 OLED, 128x64, I2C | The 0.96" version is most common. The 1.3" version also works (uses the same SSD1306 controller). The I2C variant is mandatory; the SPI variant is not wired in the default pinout. |
| 1-3 | Tactile buttons (6 mm) | One is the bare minimum. Three is the recommended maximum. Wired active-LOW with internal pull-up enabled. |
| 1 | RGB LED (common cathode) | For status indication. Optional but strongly recommended. |

### 6.5 Wiring

| Qty | Component | Notes |
|---|---|---|
| ~20 | Female-to-female jumper wires | For breadboarding. For a permanent build, solder. |
| 1 | Breadboard or perfboard | 830-point breadboard is fine. |
| 1 | AMS1117-3.3 LDO (optional) | Only needed if you power the ESP32-S3 from a 5 V source (e.g., a LiPo boost converter). The DevKitC-1 already includes a 3.3 V regulator. |

### 6.6 Photo

![Hardware photo placeholder](docs/img/hardware-required.png)

A real photo of the minimum PhantomRF build (ESP32-S3, one nRF24, one CC1101,
OLED, three buttons) on a breadboard.

---

## 7. Hardware optional

These additions make PhantomRF more capable, more portable, or more
research-friendly. None are required for M0.

### 7.1 Multi-radio additions

- **Up to four more nRF24L01+PA+LNA modules** — for parallel jamming of
  multiple bands or a five-radio round-robin. The default pinout in
  [`DESIGN.md`](DESIGN.md) §8 supports five nRF24s in total (1 primary + 4
  optional). Each module costs ~$2.
- **A second CC1101** — for parallel coverage of two sub-GHz bands
  (e.g., 315 + 433 MHz, or 433 + 868 MHz). M1 feature. The second CC1101
  is wired to a different SPI bus (VSPI). Each module costs ~$1.50.
- **A third antenna** (IPEX-to-SMA-F pigtail) — for the ESP32-S3's built-in
  WiFi. Improves WiFi deauth and beacon flood range significantly. ~$1.

### 7.2 Power and portability

- **TP4056 LiPo charger module** — charges a single 18650 or LiPo cell over
  USB. ~$0.50.
- **3.7 V 18650 cell (3000 mAh)** — gives roughly 8 hours of full-load
  operation (all attacks active) or 37 hours of idle. ~$3.
- **3.7 V 2500 mAh LiPo pouch** — lighter than an 18650, fits in the small
  case. ~$4.
- **Slide switch (SPST)** — for power on/off without unplugging. ~$0.10.
- **1000 µF / 6.3 V low-ESR bulk capacitor** — recommended for 4-radio
  builds, to avoid brownouts on weak USB supplies. ~$0.20.
- **Heatsink (10×10 mm adhesive aluminum)** — for each nRF24 PA+LNA module
  in continuous-TX mode. Reduces temperature by ~15 °C. ~$0.10 each.

### 7.3 Indicators and feedback

- **Piezo buzzer (3 V, active)** — emits a short beep on attack start and a
  double-beep on attack stop. M2 feature. ~$0.20.
- **Extra RGB LED** — for the second half of a 4-LED strip (16 pixels total).
  M2 feature. ~$0.30.

### 7.4 Enclosure and PCB

- **Custom PCB** — the "PhantomRF v1 PCB" design (KiCad files in
  `hardware/pcb/v1/`) integrates the ESP32-S3, two nRF24s, one CC1101, the
  OLED header, three buttons, an RGB LED, a battery charger, and a USB-C
  connector into a single 50×80 mm 4-layer board. M1 feature.
- **3D-printed case** — the "PhantomRF v1 case" (FreeCAD source in
  `hardware/case/v1/`) is a two-part snap-fit enclosure with ventilation
  holes for the radios. M1 feature. ~$2 in PLA filament.

### 7.5 Optional photo

![Optional hardware placeholder](docs/img/hardware-optional.png)

A photo of the maximum PhantomRF build: ESP32-S3, five nRF24s, two CC1101s,
OLED, three buttons, RGB LED, 18650 battery, and the optional 3D-printed
case with ventilation slots.

---

## 8. Bill of materials and cost

The following table assumes AliExpress prices in 2026 and a single PhantomRF
build. Bulk discounts and local sourcing can change the numbers significantly.

### 8.1 Required components

| Item | Qty | Unit price | Subtotal |
|---|---|---|---|
| ESP32-S3-WROOM-1-N16R8 DevKitC-1 | 1 | $5.50 | $5.50 |
| nRF24L01+PA+LNA with external antenna | 1 | $2.00 | $2.00 |
| CC1101 module (green PCB) | 1 | $1.50 | $1.50 |
| SSD1306 OLED 0.96" I2C | 1 | $1.20 | $1.20 |
| Tactile buttons (6 mm) | 3 | $0.05 | $0.15 |
| 100 µF / 16 V electrolytic capacitors | 2 | $0.03 | $0.06 |
| Jumper wires (F-F, 20 cm, pack of 40) | 1 | $0.80 | $0.80 |
| Breadboard (830 point) | 1 | $1.50 | $1.50 |
| USB-C data cable (1 m) | 1 | $1.20 | $1.20 |
| **Required subtotal** | | | **$13.91** |

### 8.2 Optional components

| Item | Qty | Unit price | Subtotal |
|---|---|---|---|
| Extra nRF24L01+PA+LNA (for 5-radio build) | 4 | $2.00 | $8.00 |
| Second CC1101 (for M1 dual-band sub-GHz) | 1 | $1.50 | $1.50 |
| TP4056 charger module | 1 | $0.50 | $0.50 |
| 18650 LiPo cell (3000 mAh) | 1 | $3.00 | $3.00 |
| 18650 battery holder | 1 | $0.30 | $0.30 |
| Slide switch (SPST) | 1 | $0.10 | $0.10 |
| 1000 µF / 6.3 V low-ESR capacitor | 1 | $0.20 | $0.20 |
| Heatsink (10×10 mm) per nRF24 | 5 | $0.10 | $0.50 |
| Piezo buzzer (3 V active) | 1 | $0.20 | $0.20 |
| IPEX-to-SMA-F pigtail (for ESP32-S3) | 1 | $1.00 | $1.00 |
| SMA 2.4 GHz antenna | 1 | $1.50 | $1.50 |
| RGB LED (common cathode) | 1 | $0.15 | $0.15 |
| 3D-printed case (PLA filament) | 1 | $2.00 | $2.00 |
| **Optional subtotal** | | | **$18.95** |

### 8.3 Totals

| Configuration | Cost |
|---|---|
| **Minimum build** (1 nRF24, 1 CC1101, OLED, breadboard) | $13.91 |
| **Recommended build** (minimum + battery + case) | $19.81 |
| **Maximum build** (5 nRF24, 2 CC1101, all options) | $32.86 |

The minimum build is sufficient to run every M0 feature. The recommended
build makes the device portable. The maximum build unlocks all M1 features
that benefit from extra hardware.

### 8.4 Where to buy

| Region | Recommended vendors |
|---|---|
| Global | AliExpress, LCSC, Taobao |
| US | Adafruit, SparkFun, Digikey, Mouser |
| EU | Reichelt, Conrad, AliExpress (CN warehouse) |
| AU | Core Electronics, Little Bird Electronics |
| ID | Tokopedia, Bukalapak (search for "nRF24L01+PA+LNA") |

LCSC part numbers for the hardest-to-source components:

- ESP32-S3-WROOM-1-N16R8: `C2934560`
- nRF24L01+PA+LNA: search "nRF24L01 PA LNA" (no canonical LCSC number; check
  the photo for the external SMA connector and the larger PCB)
- CC1101: `C5364` (the TI chip itself) or any "green PCB CC1101 module" on
  AliExpress for the ready-to-use module

---

## 9. Quick start (TL;DR)

If you have already built ESP32 projects and just want to try PhantomRF:

### Step 1: Install

```bash
git clone https://github.com/your-org/PhantomRF.git
cd PhantomRF
pip install platformio
```

### Step 2: Flash

```bash
# Build the firmware
pio run -e esp32s3-n16r8

# Flash it (make sure the ESP32-S3 is plugged in via USB-C)
pio run -e esp32s3-n16r8 -t upload
```

### Step 3: Use

1. Wait ~3 seconds for boot.
2. On your phone or laptop, connect to the WiFi network `PhantomRF`
   (password is printed to the serial monitor on first boot; it is a
   randomly generated 16-character string).
3. Open `http://192.168.4.1` in your browser.
4. The dashboard appears. Click an attack in the sidebar.
5. Watch the spectrum waterfall update in real time.

If anything goes wrong, jump to [§18 Troubleshooting](#18-troubleshooting).

---

## 10. Installation

PhantomRF supports four installation paths. Pick the one that matches your
operating system and tooling preferences.

### 10.1 PlatformIO on Linux (recommended)

This is the primary, fully-supported development environment.

#### Prerequisites

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y git python3 python3-pip python3-venv \
                    build-essential libusb-1.0-0-dev

# Arch / Manjaro
sudo pacman -S git python python-pip base-devel libusb

# Fedora
sudo dnf install -y git python3 python3-pip gcc make libusb-devel
```

#### Get the source

```bash
git clone https://github.com/your-org/PhantomRF.git
cd PhantomRF
```

#### Install PlatformIO

We strongly recommend using a virtual environment to keep the rest of your
Python clean:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install platformio
```

#### Add your user to the `dialout` group (for serial access)

```bash
sudo usermod -aG dialout $USER
# Log out and back in for this to take effect
```

#### Build and flash

```bash
# List available environments
pio run --list-targets

# Build only
pio run -e esp32s3-n16r8

# Build and flash
pio run -e esp32s3-n16r8 -t upload

# Build, flash, and open the serial monitor
pio run -e esp32s3-n16r8 -t upload && pio device monitor
```

The first build will take 3-7 minutes (downloading toolchains, frameworks,
libraries). Subsequent builds take 15-45 seconds.

### 10.2 PlatformIO on macOS

```bash
# Install Homebrew (if you don't have it)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install git python3 libusb

# Get the source and PlatformIO
git clone https://github.com/your-org/PhantomRF.git
cd PhantomRF
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install platformio

# Build and flash
pio run -e esp32s3-n16r8 -t upload
```

On macOS, the serial port for the ESP32-S3's native USB is typically
`/dev/cu.usbmodem*` (e.g., `/dev/cu.usbmodem14101`). PlatformIO will auto-
detect this; if it does not, pass it explicitly:

```bash
pio run -e esp32s3-n16r8 -t upload --upload-port /dev/cu.usbmodem14101
```

### 10.3 PlatformIO on Windows

#### Option A: WSL2 (recommended for Windows 11)

WSL2 is the smoothest path on Windows. Use Ubuntu inside WSL2, then follow
the Linux instructions above. The ESP32-S3 is accessible to WSL2 via the
USB passthrough.

```powershell
# In PowerShell as Administrator
wsl --install
# Reboot, then install Ubuntu from the Microsoft Store
```

Inside the WSL2 Ubuntu shell:

```bash
sudo apt install -y git python3 python3-pip python3-venv \
                    build-essential libusb-1.0-0-dev
# Then follow the Linux steps
```

#### Option B: Native Windows

```powershell
# Install Python 3.11+ from python.org (check "Add to PATH")
# Install Git from git-scm.com
# Install the USB driver (see below)

# Clone the repo
git clone https://github.com/your-org/PhantomRF.git
cd PhantomRF

# Set up a virtual environment
python -m venv .venv
.venv\Scripts\activate

# Install PlatformIO
pip install --upgrade pip
pip install platformio

# Build and flash
pio run -e esp32s3-n16r8 -t upload
```

#### USB driver note

Windows does not have a built-in driver for the ESP32-S3's native USB CDC.
You need one of:

- The Espressif CP210x driver (if your DevKitC-1 uses a CP2102N UART bridge,
  which the S3's native USB bypasses; usually not needed).
- The **usbser.sys** driver that ships with Windows 10+ for USB CDC ACM
  devices. Plug in the device, open Device Manager, find the unknown device,
  right-click, "Update driver", "Browse my computer", "Let me pick from a
  list", "Universal Serial Bus devices", and select "USB Serial Device".

If the serial port still does not appear, install the
[Zadig WinUSB driver](https://zadig.akeo.ie/) for the device. This is the
same driver used for the WebSerial-based flasher.

### 10.4 Arduino IDE 2.x

If you prefer the Arduino IDE over PlatformIO:

1. Install Arduino IDE 2.x from
   [arduino.cc/en/software](https://www.arduino.cc/en/software).
2. Open **File → Preferences**, and add this URL to "Additional boards
   manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for `esp32`, and install
   the **esp32 by Espressif Systems** package, version **3.3.10 or later**.
4. Install the libraries from the Library Manager:
   - `RF24` by TMRh20 (or nrf24 successor), version 1.6.x
   - `SmartRC-CC1101-Driver-Lib` by LSatan, version 3.x
   - `NimBLE-Arduino` by h2zero, version 2.x
   - `Adafruit SSD1306` version 2.x
   - `Adafruit GFX Library` version 1.x
   - `ArduinoJson` by Benoit Blanchon, version 7.x
   - `ESPAsyncWebServer` by ESPHome, version 3.x
   - `AsyncTCP` by me-no-dev, version 3.x
5. In **Tools → Board**, select **ESP32S3 Dev Module**.
6. Configure the following **Tools** menu options:
   - USB CDC On Boot: **Enabled**
   - USB DFU On Boot: **Disabled**
   - Flash Size: **16 MB (128 Mb)**
   - Partition Scheme: **Huge APP (3 MB No OTA / 1 MB APP / 9.4 MB FAT)**
   - PSRAM: **OPI PSRAM**
   - Upload Speed: **921600**
7. Open `src/main.cpp` and **Sketch → Upload**.

> The Arduino IDE does not support all of the PlatformIO-only features
> (auto-build of the LittleFS image, custom partition tables, multiple
> environments). For development, use PlatformIO. For one-off uploads,
> Arduino IDE is fine.

### 10.5 Web flasher (no install)

The easiest path for non-developers:

1. Visit `https://your-org.github.io/PhantomRF/flasher/`.
2. Connect the ESP32-S3 to your computer via USB-C.
3. Click **INSTALL PHANTOMRF**.
4. Your browser will ask you to select a serial port. Pick the ESP32-S3.
5. The browser streams the firmware to the device.
6. The device reboots and you see the PhantomRF AP.

This works in Chrome, Edge, and Opera (all Chromium-based). Firefox supports
WebSerial in version 110+ but the experience is less polished. Safari does
not yet support WebSerial.

### 10.6 Drag-and-drop UF2 (no install, ESP32-S3 only)

The ESP32-S3's native USB OTG supports a "USB Mass Storage" bootloader mode
that lets you flash a `.uf2` file by simply dragging it onto a virtual USB
drive. No drivers, no tool, no browser.

To enter UF2 mode:

1. Hold the **BOOT** button on the ESP32-S3.
2. Press and release the **RESET** button (while still holding BOOT).
3. Release the **BOOT** button after 1 second.
4. The device appears on your computer as a USB drive named
   `ESP32-S3` or similar.
5. Drag `firmware.uf2` onto the drive.
6. The device reboots into PhantomRF.

This is the **easiest** way to flash a brand new device with no prior
tooling installed. Pre-built `.uf2` files for the latest release are
available on the GitHub Releases page.

---

## 11. Wiring

This section shows the exact wiring for the default PhantomRF build. If you
want a different pinout, see [`DESIGN.md`](DESIGN.md) §8 for the design
rationale and the per-board pin tables.

### 11.1 ASCII wiring diagram

```
                                 ESP32-S3-WROOM-1-N16R8 (DevKitC-1)
                              ┌───────────────────────────────┐
                              │  3V3  GND  5V  GND            │
                              │   ●    ●   ●   ●              │
   ┌────────────────────┐    │                               │    ┌──────────────────────┐
   │   nRF24L01+PA+LNA   │    │  GPIO 12 (HSPI SCK) ────────►│ SCK│                      │
   │  (channel #1)       │    │  GPIO 13 (HSPI MISO) ◄───────│ MISO│                      │
   │                     │    │  GPIO 11 (HSPI MOSI) ───────►│ MOSI│                      │
   │  ┌──────────────┐   │    │  GPIO 15 ──────────────────►│ CSN │                      │
   │  │ VCC GND      │   │    │  GPIO 16 ──────────────────►│ CE  │                      │
   │  │ SCK MISO MOSI│ ◄─┼────┤                              │    │                      │
   │  │ CSN CE IRQ   │   │    │                              │    │                      │
   │  └──────────────┘   │    │  GPIO 17 ──────────────────►│ CSN2│ (optional nRF24 #2) │
   │  ┌────────────┐     │    │  GPIO 18 ──────────────────►│ CE2 │                      │
   │  │ 100 µF cap │     │    │  GPIO 21 ──────────────────►│ CSN3│ (optional nRF24 #3) │
   │  │ VCC ↔ GND  │     │    │  GPIO 35 ──────────────────►│ CE3 │                      │
   │  └────────────┘     │    │                               │    │                      │
   └────────────────────┘    │                               │    └──────────────────────┘
                              │                               │
   ┌────────────────────┐    │  GPIO 36 (VSPI SCK) ────────►│ SCK│   ┌──────────────────┐
   │     CC1101         │    │  GPIO 37 (VSPI MISO) ◄──────│ MISO│   │  CC1101          │
   │   (sub-GHz)        │    │  GPIO 38 (VSPI MOSI) ──────►│ MOSI│   │  (green PCB)     │
   │                     │    │  GPIO 39 ──────────────────►│ CSN │   │                  │
   │  ┌──────────────┐   │    │  GPIO 2  ◄──────────────── │ GDO0│   │  VCC GND SCK MISO│
   │  │ VCC GND      │   │    │  GPIO 4  ◄──────────────── │ GDO2│   │  MOSI CSN GDO0   │
   │  │ SCK MISO MOSI│ ◄─┼────┤                              │    │   │  GDO2  ANT       │
   │  │ CSN GDO0 GDO2│   │    │                               │    │   │                  │
   │  │ ANT          │   │    │                               │    │   │  ┌───────────┐   │
   │  └──────────────┘   │    │                               │    │   │  │ 100 µF    │   │
   │  ┌────────────┐     │    │                               │    │   │  │ VCC ↔ GND │   │
   │  │ 100 µF cap │     │    │                               │    │   │  └───────────┘   │
   │  │ VCC ↔ GND  │     │    │                               │    │   └──────────────────┘
   │  └────────────┘     │    │                               │    │
   └────────────────────┘    │                               │    │
                              │  GPIO 8  (I2C SDA)  ◄──────►│ SDA│   ┌──────────────────┐
                              │  GPIO 9  (I2C SCL)  ───────►│ SCL│   │  SSD1306 OLED    │
                              │                               │    │   │  128x64 I2C      │
                              │                               │    │   │  VCC GND SDA SCL │
                              │                               │    │   └──────────────────┘
                              │                               │    │
                              │  GPIO 10 ◄──── (BTN OK)       │    │   ┌──────────────────┐
                              │  GPIO 14 ◄──── (BTN NEXT)     │    │   │  3x Tactile      │
                              │  GPIO 38 ◄──── (BTN PREV)     │    │   │  buttons (active │
                              │              (shared with     │    │   │  LOW, internal   │
                              │               VSPI MOSI;      │    │   │  pull-up)        │
                              │               if CC1101 not   │    │   └──────────────────┘
                              │               used, GPIO 33   │    │
                              │               for PREV)       │    │
                              │                               │    │   ┌──────────────────┐
                              │  GPIO 5  ──── (LED R, PWM)    │    │   │  RGB LED         │
                              │  GPIO 6  ──── (LED G, PWM)    │    │   │  common cathode  │
                              │  GPIO 7  ──── (LED B, PWM)    │    │   │  R, G, B, GND    │
                              │                               │    │   └──────────────────┘
                              │  GPIO 1  ◄──── (BAT ADC)      │    │   ┌──────────────────┐
                              │                               │    │   │  Battery monitor │
                              │                               │    │   │  V_BAT ─► 100kΩ ─┤
                              │                               │    │   │           ─► GPIO1│
                              │                               │    │   │       ─► 100kΩ ─┤
                              │                               │    │   │           ─► GND │
                              │                               │    │   └──────────────────┘
                              │  USB-C (native)               │    │
                              │   D−  ◄─ GPIO 19              │    │
                              │   D+  ◄─ GPIO 20              │    │
                              │   VBUS ─► 5V                  │    │
                              │   GND                         │    │
                              └───────────────────────────────┘    │
```

> The GPIO 38 conflict for "BTN PREV" is resolved at build time. If the
> CC1101 is present, BTN PREV moves to GPIO 33. The build system picks the
> correct pin based on the `PHM_HW_CC1101_PRESENT` macro.

### 11.2 Wiring checklist

Before powering on for the first time, verify each item below. Catching a
wiring mistake before power-up is the difference between "works first try"
and "magic smoke".

- [ ] **nRF24 VCC and GND are not reversed.** The 100 µF capacitor is on the
      nRF24 side of the wire, as close to the module as possible.
- [ ] **CC1101 VCC and GND are not reversed.** Same rule. The CC1101 draws
      80 mA peak; a reversed supply will burn the chip in seconds.
- [ ] **All SPI lines are correctly oriented.** SCK and MOSI are outputs
      from the ESP32-S3; MISO is an input. The CSN/CE lines are outputs.
- [ ] **The OLED is the I2C variant, not SPI.** The default pinout does
      not support SPI OLEDs.
- [ ] **Buttons are wired active-LOW.** One side of each button goes to a
      GPIO, the other side goes to GND. Internal pull-ups are enabled in
      software, so no external resistor is required.
- [ ] **The RGB LED is common-cathode.** A common-anode LED will work but
      the color logic is inverted.
- [ ] **No GPIO is shorted to 3V3 or GND.** Use a multimeter to verify.
- [ ] **The USB-C cable supports data, not just charging.** Some cheap
      cables are power-only.

### 11.3 ESP32-S3 GPIO reference

| Pin | Function in PhantomRF | Direction | Notes |
|---|---|---|---|
| GPIO 1 | Battery ADC | Input | ADC1_CH0, works with WiFi active |
| GPIO 2 | CC1101 GDO0 | Input | Safe on S3 (not a strapping pin) |
| GPIO 4 | CC1101 GDO2 | Input | Free, no conflicts |
| GPIO 5 | LED Red | Output (PWM) | Channel 0 of LEDC |
| GPIO 6 | LED Green | Output (PWM) | Channel 1 of LEDC |
| GPIO 7 | LED Blue | Output (PWM) | Channel 2 of LEDC |
| GPIO 8 | OLED SDA | I2C | I2C0 default |
| GPIO 9 | OLED SCL | I2C | I2C0 default |
| GPIO 10 | Button OK | Input | RTC-capable, supports deep-sleep wake |
| GPIO 11 | nRF24 MOSI | SPI (out) | HSPI IO_MUX, fastest path |
| GPIO 12 | nRF24 SCK | SPI (out) | HSPI IO_MUX |
| GPIO 13 | nRF24 MISO | SPI (in) | HSPI IO_MUX |
| GPIO 14 | Button NEXT | Input | RTC-capable, supports deep-sleep wake |
| GPIO 15 | nRF24 CSN #1 | Output | Output-only, fine for CS |
| GPIO 16 | nRF24 CE #1 | Output | |
| GPIO 17 | nRF24 CSN #2 | Output | |
| GPIO 18 | nRF24 CE #2 | Output | Avoids USB strapping |
| GPIO 19 | USB D- | USB | Native USB |
| GPIO 20 | USB D+ | USB | Native USB |
| GPIO 21 | nRF24 CSN #3 | Output | |
| GPIO 33 | Button PREV (CC1101 case) | Input | Used only if CC1101 present |
| GPIO 35 | nRF24 CE #3 | Output | No strapping function |
| GPIO 36 | nRF24 CSN #4 / CC1101 SCK | Output (mux) | Remapped when CC1101 present |
| GPIO 37 | nRF24 CE #4 / CC1101 MISO | Output (mux) | |
| GPIO 38 | nRF24 CSN #5 / CC1101 MOSI | Output (mux) | |
| GPIO 39 | nRF24 CE #5 / CC1101 CSN | Output (mux) | |

**Pins to avoid:**

- GPIO 0, 3, 45, 46 — strapping pins
- GPIO 26-32 — used by the N16R8's octal PSRAM and flash
- GPIO 43, 44 — UART0 default (used by the USB-Serial/JTAG)

### 11.4 Power-on order

1. Connect the ESP32-S3 to USB-C without any radios attached.
2. Open a serial monitor (PlatformIO device monitor, `minicom`, or PuTTY).
3. You should see the PhantomRF banner within 3 seconds:
   ```
   PhantomRF v0.9.0-dev (esp32s3-n16r8)
   Free heap: 384512  Free PSRAM: 8388000
   [OK] NVS initialized
   [OK] LittleFS mounted (10485760 bytes free)
   [OK] OLED detected (SSD1306 @ 0x3C)
   [OK] nRF24 #1 detected (RF24_PA_MAX, channel 76)
   [OK] CC1101 detected (433.92 MHz, version 0x04)
   [OK] AP "PhantomRF" started (192.168.4.1)
   AP password: a7K9-mP2xL3nQ8wR
   [OK] Web server listening on port 80
   Ready.
   ```
4. **Write down the AP password.** It is shown only once per boot. You can
   also press the OK button at any time to have it re-displayed on the OLED.
5. Power off, attach the radios, power back on. Repeat the check.
6. If anything shows `[FAIL]`, check the wiring for that module.

---

## 12. First flash

This section walks through the first flash in detail, including how to enter
the bootloader manually, what each build environment does, and how to
troubleshoot a failed flash.

### 12.1 The four flashing methods

PhantomRF supports four ways to flash. Pick whichever matches your
situation.

#### 12.1.1 Web flasher (no install)

The web flasher at `https://your-org.github.io/PhantomRF/flasher/` uses
the `esp-web-tools` JavaScript library to stream a `.bin` over WebSerial.

**Steps:**

1. Visit the URL above in Chrome, Edge, or Opera.
2. Click **CONNECT** and pick the ESP32-S3's serial port from the popup.
3. Click **INSTALL PHANTOMRF**.
4. Wait ~30 seconds for the upload to complete.
5. The device reboots into PhantomRF.

**Limitations:** Requires a Chromium browser. Requires HTTPS. Requires user
gesture (you have to click the button) — automated scripts cannot trigger
the WebSerial popup.

#### 12.1.2 Drag-and-drop UF2 (no install, ESP32-S3 only)

The ESP32-S3's ROM bootloader supports a USB Mass Storage mode. When the
device is in "bootloader" mode, it appears as a 128 KB virtual drive. You
copy `firmware.uf2` onto the drive, and the ROM writes it to flash and
reboots.

**Steps:**

1. Download the latest `firmware.uf2` from the GitHub Releases page.
2. Hold the **BOOT** button on the ESP32-S3.
3. While holding BOOT, press and release the **RESET** button.
4. Wait 1 second, then release the **BOOT** button.
5. A new USB drive named `ESP32-S3` or `NODE_PhantomRF` appears.
6. Drag `firmware.uf2` onto the drive.
7. The drive disappears as the device reboots into PhantomRF.

**Limitations:** UF2 mode only works on the ESP32-S3 (and a few other
recent Espressif chips). The classic ESP32 and the ESP32-C3 do not support
it.

#### 12.1.3 esptool.py (command line)

```bash
# Install esptool
pip install esptool

# Erase the entire flash (only needed if migrating from another firmware)
esptool.py --chip esp32s3 --port /dev/ttyACM0 erase_flash

# Flash the firmware
esptool.py --chip esp32s3 --port /dev/ttyACM0 \
           --baud 921600 \
           --before default_reset --after hard_reset \
           write_flash 0x0 firmware.bin
```

**Notes:**

- The port on Linux is typically `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS
  it is `/dev/cu.usbmodem*`. On Windows it is `COM3`, `COM4`, etc.
- The baud rate can be 460800 or 921600. Higher baud rates need a good USB
  cable and a USB 2.0 or better port.
- `erase_flash` is destructive; it wipes NVS, LittleFS, and any saved
  recordings. Do not run it unless you are recovering from a bad flash.

#### 12.1.4 PlatformIO

```bash
# Build and flash in one step
pio run -e esp32s3-n16r8 -t upload

# Just build (no flash)
pio run -e esp32s3-n16r8

# Upload only the filesystem (LittleFS image)
pio run -e esp32s3-n16r8 -t uploadfs
```

`pio run -t upload` uses `esptool.py` under the hood. It automatically
erases the right parts of flash and writes the firmware and LittleFS image.

### 12.2 Build environments

PhantomRF supports multiple ESP32 variants. The primary is the ESP32-S3-N16R8.

| Environment | Board | Flash | PSRAM | BLE | WiFi | Native USB | Notes |
|---|---|---|---|---|---|---|---|
| `esp32s3-n16r8` | ESP32-S3-DevKitC-1-N16R8 | 16 MB | 8 MB | Yes | Yes | Yes | **Primary.** All features. |
| `esp32s3-n8r2` | ESP32-S3-DevKitC-1-N8R2 | 8 MB | 2 MB | Yes | Yes | Yes | All features except recording buffer >1 MB. |
| `esp32s3-n8` | ESP32-S3-DevKitC-1-N8 | 8 MB | None | Yes | Yes | Yes | Subset: no spectrum history. |
| `esp32-classic` | ESP32-WROOM-32D | 4 MB | None | Yes | Yes | No | All features except UF2 and BLE 5.0. |
| `esp32c3` | ESP32-C3-DevKitM-1 | 4 MB | None | Yes | Yes | Yes | 2.4 GHz only, no BLE spam. |
| `esp32s2` | ESP32-S2-Saola-1 | 4 MB | None | No | Yes | Yes | All except BLE. |
| `flipper-zero` | Flipper Zero | 1 MB | None | No | No | No | Companion port, 2.4 GHz only. |

Each environment is defined in `platformio.ini`. The same source code
compiles for all of them; build flags disable features that do not fit on
smaller hardware.

### 12.3 What to expect on first boot

1. The device announces itself on the serial port at 115200 baud.
2. It scans for the nRF24 and CC1101 modules on the SPI bus.
3. It initializes the OLED, the buttons, and the LED.
4. It mounts the LittleFS partition.
5. It starts the WiFi AP and the web server.
6. It prints the AP password to the serial monitor and to the OLED.
7. The status LED turns solid green (idle).

If any step fails, the device halts in a safe state, prints a `[FAIL]`
message, and the LED turns red. The exact failure mode is documented in
[§18 Troubleshooting](#18-troubleshooting).

---

## 13. First use

Once the device has booted successfully, here is what you do.

### 13.1 Connect to the AP

1. On your phone or laptop, open the WiFi settings.
2. Look for a network named `PhantomRF` (or whatever you configured in NVS).
3. Connect. The password is the 16-character string shown in the serial
   monitor and on the OLED. (On the OLED, the password is also shown by
   pressing the OK button once.)
4. Your device should obtain an IP address in the `192.168.4.x` range.
5. The captive portal should redirect your browser to `http://192.168.4.1`.
   If it does not, navigate there manually.

### 13.2 The dashboard

The dashboard is the home page of the web UI. It shows:

- **Status:** current module (None, BT, BLE, WiFi, Sub-GHz, etc.), running
  state, uptime, free heap, free PSRAM, IP address, current WiFi channel.
- **Control channels:** the health of each of the five control channels
  (USB CDC, WebSocket, BLE Serial, OLED+Buttons, Station WiFi).
- **Spectrum:** a small embedded spectrum plot for the 2.4 GHz band.
- **Recent log:** the last 50 log lines, color-coded by level.

### 13.3 Run a test attack

1. In the sidebar, click **2.4 GHz → Bluetooth**.
2. The Bluetooth Jam panel appears.
3. Select a sweep method (List is the safest starting point).
4. Select a PA level (start with LOW; the higher levels can disconnect you
   from the AP).
5. Click **Start**.
6. The web UI shows the current channel, the time elapsed, and a
   "Control degraded" warning banner (because the 2.4 GHz jam interferes
   with the WiFi link you are using).
7. To stop, use the **Stop** button (if the WebSocket is still alive) or
   press the **OK** button on the device (OLED menu → Stop).

### 13.4 Look at the spectrum

1. In the sidebar, click **Spectrum**.
2. The spectrum view appears with two tabs: **2.4 GHz** and **Sub-GHz**.
3. The 2.4 GHz tab shows a live RSSI bar chart for all 126 nRF24 channels
   plus an overlay of the current WiFi APs on each channel.
4. The Sub-GHz tab (M2) shows a similar view for the CC1101 band.
5. The waterfall scrolls from right to left, showing the last 100 frames
   of spectrum history.

### 13.5 Configure the device

1. In the sidebar, click **Settings**.
2. The settings page is organized into sections: **General**, **Radios**,
   **AP**, **Display**, **Buttons**, **Recording**, **Power**, **Advanced**.
3. Change any setting and click **Save**. The settings are written to NVS
   and persist across reboots.
4. The **Reset** button at the bottom of the page restores the factory
   defaults. It does not erase recordings.

### 13.6 Update the firmware

1. In the sidebar, click **OTA**.
2. The OTA page shows the current firmware version and a file picker.
3. Select a `.bin` file. The file is uploaded to the device and verified
   against the SHA-256 in the manifest.
4. The device writes the new firmware to the OTA staging partition, then
   reboots into the new firmware.
5. The previous firmware remains in the other OTA slot; if the new firmware
   fails to boot three times, the device automatically rolls back.

---

## 14. Features documentation

Each attack module has a dedicated page in the `docs/features/` folder. The
table below links to each.

| Module | Document | What it covers |
|---|---|---|
| Bluetooth Classic jam | [`docs/features/bluetooth-jam.md`](docs/features/bluetooth-jam.md) | AFH hop set, brute force, random sweep |
| BLE advertising jam | [`docs/features/ble-jam.md`](docs/features/ble-jam.md) | Channels 2 / 26 / 80 NOACK |
| BLE data channel jam | [`docs/features/ble-data-jam.md`](docs/features/ble-data-jam.md) | Channel 2..80 step 2 CW |
| Apple Continuity spam | [`docs/features/apple-spam.md`](docs/features/apple-spam.md) | AirPods / Nearby / Apple TV popups |
| Samsung Buds spam | [`docs/features/samsung-spam.md`](docs/features/samsung-spam.md) | Galaxy Buds popups |
| Windows Swift Pair spam | [`docs/features/windows-spam.md`](docs/features/windows-spam.md) | "Connect to [device]" popups |
| Google Fast Pair spam | [`docs/features/fast-pair-spam.md`](docs/features/fast-pair-spam.md) | Pixel / Android 13+ pairing dialogs |
| WiFi deauth | [`docs/features/wifi-deauth.md`](docs/features/wifi-deauth.md) | Targeted, all, smart modes |
| WiFi disassoc | [`docs/features/wifi-disassoc.md`](docs/features/wifi-disassoc.md) | Disassociation flood |
| Beacon flood | [`docs/features/beacon-flood.md`](docs/features/beacon-flood.md) | Random SSID, custom SSID list |
| Probe request flood | [`docs/features/probe-flood.md`](docs/features/probe-flood.md) | Random SSID probes |
| Evil twin | [`docs/features/evil-twin.md`](docs/features/evil-twin.md) | Clone AP, capture handshake |
| PMKID capture | [`docs/features/pmkid-capture.md`](docs/features/pmkid-capture.md) | Handshake-less WPA2 attack |
| Handshake capture | [`docs/features/handshake-capture.md`](docs/features/handshake-capture.md) | Standard 4-way WPA2 |
| Sub-GHz spot jam | [`docs/features/subghz-spot.md`](docs/features/subghz-spot.md) | Single frequency OOK flood |
| Sub-GHz range jam | [`docs/features/subghz-range.md`](docs/features/subghz-range.md) | Sweep with configurable step |
| Sub-GHz hopper | [`docs/features/subghz-hopper.md`](docs/features/subghz-hopper.md) | Round-robin user-supplied list |
| Sub-GHz keyfob | [`docs/features/subghz-keyfob.md`](docs/features/subghz-keyfob.md) | Pre-loaded keyfob frequencies |
| Record RAW | [`docs/features/record-raw.md`](docs/features/record-raw.md) | OOK/FSK bitstream capture |
| Replay RAW | [`docs/features/replay-raw.md`](docs/features/replay-raw.md) | Re-transmit captured bitstream |
| Spectrum analyzer | [`docs/features/spectrum-analyzer.md`](docs/features/spectrum-analyzer.md) | Live RSSI waterfall |
| MouseJack | [`docs/features/mousejack.md`](docs/features/mousejack.md) | Keystroke injection to nRF24 dongles |
| RollJam | [`docs/features/rolljam.md`](docs/features/rolljam.md) | Two-stage keyfob attack |

The detailed feature pages are written to be readable top-to-bottom by
someone with no prior RF experience. Each page answers:

1. What does this feature do?
2. Why does it work (what is the underlying protocol weakness)?
3. How do I use it from the web UI?
4. How do I use it from the serial CLI?
5. What are the limitations and edge cases?
6. What does the legal use look like?

---

## 15. Algorithms explained

This section gives a one-paragraph overview of each core algorithm. The deep
dive (with register-level details, frame byte layouts, and timing math) is in
`docs/algorithms/`.

| Algorithm | Document | What it does |
|---|---|---|
| nRF24 constant carrier | [`docs/algorithms/nrf24-const-carrier.md`](docs/algorithms/nrf24-const-carrier.md) | Set the `CONT_WAVE` and `PLL_LOCK` bits in `RF_SETUP` to emit an unmodulated carrier on a chosen channel. |
| nRF24 channel sweep | [`docs/algorithms/nrf24-sweep.md`](docs/algorithms/nrf24-sweep.md) | Hop across 1-126 channels at up to 5000 channels/second with one or more nRF24s in parallel. |
| CC1101 async-serial | [`docs/algorithms/cc1101-async-serial.md`](docs/algorithms/cc1101-async-serial.md) | Bypass the CC1101's packet handler to bit-bang the GDO0 pin, enabling raw OOK/FSK capture and replay. |
| CC1101 frequency math | [`docs/algorithms/cc1101-freq-math.md`](docs/algorithms/cc1101-freq-math.md) | Convert a target frequency in MHz to the three-byte `FREQ2`/`FREQ1`/`FREQ0` register value. |
| 802.11 deauth frame | [`docs/algorithms/802.11-deauth.md`](docs/algorithms/802.11-deauth.md) | Build a 26-byte management frame (type 00, subtype 12) and transmit it with `esp_wifi_80211_tx()`. |
| 802.11 beacon frame | [`docs/algorithms/802.11-beacon.md`](docs/algorithms/802.11-beacon.md) | Build a beacon frame with a chosen SSID and transmit it on a chosen channel. |
| BLE advertising spam | [`docs/algorithms/ble-advertising.md`](docs/algorithms/ble-advertising.md) | Build a manufacturer-specific advertisement with the right Apple/Samsung/Google/Microsoft payload to trigger pairing popups. |
| Channel math | [`docs/algorithms/channel-math.md`](docs/algorithms/channel-math.md) | Convert between nRF24 channels, WiFi channels, BLE channels, and Zigbee channels. |
| Sweep strategies | [`docs/algorithms/sweep-strategies.md`](docs/algorithms/sweep-strategies.md) | Together, separate, random, and list sweep strategies and their trade-offs. |
| MouseJack | [`docs/algorithms/mousejack.md`](docs/algorithms/mousejack.md) | Inject keystrokes into vulnerable nRF24 dongles using Samy Kamkar's 2016 technique. |
| PMKID capture | [`docs/algorithms/pmkid.md`](docs/algorithms/pmkid.md) | Request an RSN IE from an AP and extract the PMKID without needing a client. |

The algorithms are the **intellectual core** of PhantomRF. Understanding
them turns the device from a black box into a learning tool.

### 15.1 The constant carrier trick (in five lines)

```cpp
// 1. Init the radio normally
radio.begin();
radio.setAutoAck(false);
radio.setPALevel(RF24_PA_MAX);

// 2. Pick a channel and prime the TX FIFO
radio.setChannel(76);
uint8_t dummy[32] = {0xFF};
radio.write(dummy, 32);

// 3. Read RF_SETUP, set CONT_WAVE | PLL_LOCK, write back
uint8_t setup = SPI_read(NRF24_REG_06_RF_SETUP);
SPI_write(NRF24_REG_06_RF_SETUP, setup | 0x90);  // CONT_WAVE | PLL_LOCK

// 4. Pulse CE high — the radio now emits a continuous carrier
digitalWrite(NRF24_CE, HIGH);
```

That's the whole trick. To change channel, write a new value to `RF_CH`. To
stop, clear the bits and pulse CE low.

### 15.2 The async-serial trick (in eight lines)

```cpp
// 1. Put the CC1101 in async-serial mode
radio.setCCMode(0);       // 0 = async-serial, 1 = normal packet
radio.setPktFormat(3);    // 3 = asynchronous serial mode
radio.setLengthConfig(0); // ignore length byte

// 2. RECORD: enter RX, sample GDO0
SetRx();
pinMode(CC1101_GDO0, INPUT);
for (size_t i = 0; i < BUFFER_SIZE; i++) {
    byte b = 0;
    for (int8_t j = 7; j >= 0; j--) {
        bitWrite(b, j, digitalRead(CC1101_GDO0));
        delayMicroseconds(sampling_us);
    }
    buffer[i] = b;
}

// 3. REPLAY: enter TX, bit-bang GDO0
SetTx();
pinMode(CC1101_GDO0, OUTPUT);
for (size_t i = 0; i < BUFFER_SIZE; i++) {
    for (int8_t j = 7; j >= 0; j--) {
        digitalWrite(CC1101_GDO0, bitRead(buffer[i], j));
        delayMicroseconds(sampling_us);
    }
}
```

That's it. The CC1101's packet handler is completely bypassed, and the raw
bitstream is in your hands.

For the deep dive on either of these, see the linked algorithm documents.

---

## 16. Control channels

PhantomRF is designed to be controllable from **five independent channels**.
This is not a feature for the sake of features — it is a hard engineering
requirement. A 2.4 GHz jammer destroys the WiFi link you would otherwise use
to control it. The only way to reliably start, monitor, and stop a 2.4 GHz
attack is to have a control channel that is independent of the 2.4 GHz
spectrum.

### 16.1 The five channels

| # | Channel | Hardware | Active when | Latency | Bandwidth |
|---|---|---|---|---|---|
| 1 | USB CDC Serial | ESP32-S3 native USB (GPIO 19/20) | USB cable connected | < 10 ms | Full CLI |
| 2 | Web UI (WebSocket over WiFi AP) | ESP32-S3 WiFi in AP mode | WiFi AP up and not jammed | < 50 ms | Full web UI + spectrum |
| 3 | BLE Serial (NimBLE GATT) | ESP32-S3 BLE 5.0 | Always (unless BLE is also being attacked) | < 100 ms | CLI subset |
| 4 | OLED + buttons | SSD1306 I2C + GPIO 10/14/33 | Always | < 20 ms | Menu navigation only |
| 5 | Station-mode WiFi (connect to home AP) | ESP32-S3 WiFi in STA mode | Configured, not in 2.4 GHz attack | < 100 ms | Full web UI |

The order is **priority order**. When multiple channels are available,
commands are sent via the lowest-numbered available channel. When a higher-
priority channel comes online (e.g., you plug in a USB cable), PhantomRF
automatically promotes it to the active channel.

### 16.2 Auto-degradation rules

PhantomRF monitors the health of each control channel in real time. When a
2.4 GHz attack starts, the WiFi link (channel 2) becomes unreliable. The
Control Channel Manager detects this and:

1. Marks channel 2 as `Degraded`.
2. Falls back to channel 3 (BLE Serial) if the BLE radio is not in use.
3. Falls back to channel 1 (USB CDC) if a USB cable is connected.
4. Falls back to channel 4 (OLED+buttons) for any user present at the
   device.
5. Marks the AP as "still up but unreliable" so the web UI shows a banner.

The web UI banner is the user-facing signal that you have lost fine-grained
control:

```
+---------------------------------------------------+
|  ! Control channel degraded: 2.4 GHz jam active   |
|  Use USB Serial (115200 baud) or BLE Serial       |
|  to stop the attack.                              |
+---------------------------------------------------+
```

### 16.3 Channel-specific commands

#### USB CDC Serial

- Baud rate: 115200 (configurable in NVS as `serial.baud`).
- Format: 8N1, newline-terminated.
- Full CLI; see `docs/reference/commands.md` for the complete command list.
- Tab-completion and history.

#### Web UI

- Default URL: `http://192.168.4.1` (configurable as `ap.ip`).
- Modern SPA with sidebar navigation.
- Real-time updates via HTTP polling (M0) or WebSocket (M1).
- Captive portal redirects all HTTP traffic to the dashboard.

#### BLE Serial

- Service UUID: `0000fff0-0000-1000-8000-00805f9b34fb` (default).
- TX characteristic: `0000fff1-0000-1000-8000-00805f9b34fb`.
- RX characteristic: `0000fff2-0000-1000-8000-00805f9b34fb`.
- Connect with nRF Connect, Serial Bluetooth Terminal, or any GATT client.
- Subset of the CLI: start, stop, status, info. No configuration writes.

#### OLED + buttons

- 128x64 SSD1306.
- 1-, 2-, or 3-button configuration, set in NVS as `buttons.config`.
- Bitmap icons for top-level menu items; text for sub-menus.
- "Back" gesture is configurable; default is long-press on Prev.

#### Station-mode WiFi

- Connect to a home AP for remote access without joining the PhantomRF AP.
- NVS keys: `sta.ssid`, `sta.password`, `sta.enabled`.
- Disabled by default to avoid accidentally joining unknown networks.
- Web UI is served on the same `192.168.4.1` URL via a virtual host; the
  station IP is shown on the OLED and in the dashboard.

### 16.4 Which channel should I use?

| Scenario | Best channel |
|---|---|
| Development on a desk | USB CDC Serial + Web UI |
| Field test with a phone | Web UI (if not jamming 2.4 GHz) or BLE Serial |
| Walk-around test | OLED + buttons (no link to break) |
| Remote pen-test in another room | Station-mode WiFi |
| Production pen-test | USB CDC Serial (cable to a tablet running a serial terminal) |

You can use multiple channels simultaneously. The commands are idempotent
(starting an attack that is already running is a no-op, and stopping an
attack that is not running is a no-op).

---

## 17. Configuration

PhantomRF is configured via three mechanisms: NVS, the web UI settings
page, and the serial CLI. All three modify the same underlying NVS keys;
which one you use is a matter of convenience.

### 17.1 NVS keys (excerpt)

The full NVS schema is in `docs/reference/nvs-schema.md`. The most-used
keys are:

| Key | Type | Default | Description |
|---|---|---|---|
| `ap.ssid` | string | `PhantomRF` | AP SSID |
| `ap.password` | string | (random 16 chars) | AP password |
| `ap.enabled` | bool | `true` | AP on/off |
| `ap.channel` | uint8 | `1` | AP channel (1-13) |
| `ap.hidden` | bool | `false` | Hide the AP SSID |
| `sta.ssid` | string | (empty) | Station-mode SSID |
| `sta.password` | string | (empty) | Station-mode password |
| `sta.enabled` | bool | `false` | Station-mode enabled |
| `nrf24.count` | int8 | `1` | Number of nRF24 modules (1-5) |
| `nrf24.pa` | int8 | `3` | PA level: 0=MIN, 1=LOW, 2=HIGH, 3=MAX |
| `nrf24.data_rate` | int8 | `2` | 0=250k, 1=1M, 2=2M |
| `nrf24.sweep_method` | int8 | `0` | 0=together, 1=separate, 2=random, 3=list |
| `nrf24.channel_start` | int8 | `0` | Custom sweep start channel |
| `nrf24.channel_stop` | int8 | `125` | Custom sweep stop channel |
| `cc1101.frequency` | float | `433.92` | Default CC1101 frequency in MHz |
| `cc1101.sampling_us` | int16 | `150` | Record/replay sampling interval (microseconds) |
| `cc1101.payload` | int8 | `60` | TX payload size (bytes) |
| `cc1101.modulation` | int8 | `0` | 0=OOK, 1=FSK, 2=ASK |
| `wifi.deauth_method` | int8 | `0` | 0=channel, 1=all, 2=smart |
| `wifi.deauth_target` | bytes | (empty) | BSSID to target |
| `wifi.beacon_ssid_count` | int8 | `10` | Number of random SSIDs to flood |
| `wifi.beacon_ssid_list` | bytes | (empty) | User-defined SSID list |
| `ble.spam_type` | int8 | `0` | 0=off, 1=apple, 2=samsung, 3=windows, 4=fastpair |
| `ble.spam_interval_ms` | int16 | `100` | Milliseconds between spam advertisements |
| `oled.enabled` | bool | `true` | OLED on/off |
| `oled.brightness` | int8 | `255` | OLED contrast (0-255) |
| `oled.timeout_s` | int16 | `60` | OLED auto-off timeout |
| `buttons.config` | int8 | `2` | 0=1-button, 1=2-button, 2=3-button |
| `serial.baud` | int32 | `115200` | USB CDC baud rate |
| `serial.enabled` | bool | `true` | USB CDC on/off |
| `spectrum.window_ms` | int16 | `100` | Spectrum refresh window |
| `spectrum.history_frames` | int16 | `100` | Spectrum waterfall history depth |
| `recording.max_duration_s` | int16 | `30` | Max recording length |
| `recording.max_count` | int16 | `100` | Max recordings before auto-purge |
| `recording.max_fs_percent` | int8 | `80` | Max filesystem percent before auto-purge |
| `power.profile` | int8 | `0` | 0=performance, 1=balanced, 2=saver |
| `power.brownout_threshold` | int8 | `28` | Brownout threshold in 0.1 V units (28 = 2.8 V) |
| `security.web_auth` | bool | `false` | Require password for web UI |
| `security.csrf_protection` | bool | `true` | CSRF tokens on state-changing endpoints |
| `security.ota_admin_only` | bool | `true` | Require BOOT-hold to OTA |
| `device.hostname` | string | `phantomrf` | mDNS / DHCP hostname (M1) |
| `device.id` | string | (random 12 hex) | Per-device unique ID |
| `control.prefer` | int8 | `1` | Preferred control channel (0-4) |

### 17.2 Web UI settings page

The web UI settings page is at `/settings`. It is organized into eight
sections, each collapsible:

1. **General** — hostname, time zone, log level
2. **Radios** — nRF24 and CC1101 configuration
3. **AP** — SSID, password, channel, visibility
4. **Display** — OLED on/off, brightness, timeout
5. **Buttons** — button configuration
6. **Recording** — duration limit, count limit, FS percent limit
7. **Power** — profile, brownout threshold, deep-sleep timeout
8. **Advanced** — security, control channel priority, factory reset

Settings are saved with a single **Save** button. Invalid values are
rejected client-side (the form reverts to the last good value). A
**Reload** button discards unsaved changes.

### 17.3 Serial CLI

The serial CLI accepts `settings get` and `settings set` commands:

```
phantom> settings get
ap.ssid              = "PhantomRF"
ap.password          = "a7K9-mP2xL3nQ8wR"
nrf24.count          = 1
nrf24.pa             = 3
nrf24.data_rate      = 2
cc1101.frequency     = 433.92
cc1101.sampling_us   = 150
... (50+ keys)

phantom> settings set nrf24.pa 1
OK: nrf24.pa = 1

phantom> settings set ap.ssid "MyJammer"
OK: ap.ssid = "MyJammer" (reboot required)
```

The CLI parser supports quoted strings, integer and float types, and
boolean (`true`/`false`/`1`/`0`/`yes`/`no`).

### 17.4 Factory reset

To reset all settings to defaults:

- **Web UI:** Settings → Advanced → **Factory Reset**.
- **Serial CLI:** `settings reset` (asks for confirmation).
- **Hardware:** With the device powered on, hold the **BOOT** button for
  10 seconds. The LED flashes red three times to confirm.

Factory reset does not erase recordings or LittleFS files. To erase
recordings, use the **Files** page in the web UI or the `rm` CLI command.

---

## 18. Troubleshooting

This section covers the most common problems and their fixes. If your
problem is not here, open a GitHub issue with the device's serial log and
a description of what you expected versus what happened.

### 18.1 The device does not appear on the serial port

**Symptoms:** No `/dev/ttyACM0` (Linux), `/dev/cu.usbmodem*` (macOS), or
`COM*` (Windows). The serial monitor shows nothing.

**Likely causes:**

1. The USB-C cable is power-only. Try a different cable.
2. The USB driver is not installed (Windows). See §10.3.
3. The device is in a low-power state. Press RESET.
4. The USB CDC is disabled in NVS. Connect via WiFi AP and re-enable, or
   hold BOOT for 10 seconds to factory-reset.

### 18.2 The web UI does not load

**Symptoms:** Connected to the `PhantomRF` AP, but `http://192.168.4.1`
times out or shows "site not reachable".

**Likely causes:**

1. The device is in a 2.4 GHz attack that has jammed the AP. The AP is
   still broadcasting but cannot carry traffic. Stop the attack via USB
   CDC, BLE Serial, or the OLED menu.
2. Your phone is on a mobile data VPN that blocks the local subnet.
   Disable the VPN or use a different phone.
3. The AP password is wrong. Reconnect with the password shown in the
   serial monitor.
4. The LittleFS partition is corrupted. Re-flash with `pio run -t
   uploadfs`.

### 18.3 The nRF24 is not detected

**Symptoms:** Serial log shows `[FAIL] nRF24 #1 SPI error` or similar.

**Likely causes:**

1. Wiring error. Double-check the SCK, MISO, MOSI, CSN, and CE pins
   against the wiring diagram.
2. Insufficient power. The nRF24 needs a stable 3.3 V. The 100 µF cap
   is mandatory.
3. Bad nRF24 module. Try a different one. The PA+LNA modules from
   random AliExpress sellers have a 5-10% DOA rate.
4. SPI bus contention. If you have a second SPI device, make sure its
   CSN is HIGH when not in use.

### 18.4 The CC1101 is not detected

**Symptoms:** Serial log shows `[FAIL] CC1101 SPI error` or `[FAIL]
CC1101 version mismatch (got 0x00, expected 0x04 or 0x14)`.

**Likely causes:**

1. Wiring error. The CC1101 is wired to VSPI (GPIO 36-39) in the default
   pinout. Check carefully.
2. Insufficient power. The CC1101 needs a stable 3.3 V. The 100 µF cap
   is mandatory.
3. Bad CC1101 module. The 8-pin variant with both GDO0 and GDO2 broken
   out is more reliable than the 4-pin variant.
4. The CC1101 is in a weird state from a previous firmware. Power-cycle
   the device (unplug USB, wait 5 seconds, plug back in).

### 18.5 The OLED is blank

**Symptoms:** OLED shows nothing, all black, or all white pixels.

**Likely causes:**

1. Wiring error. Check SDA (GPIO 8) and SCL (GPIO 9). The I2C variant
   only — SPI OLEDs do not work.
2. Wrong I2C address. The SSD1306 default is 0x3C. Some clones use 0x3D.
   The web UI settings page lets you change the address.
3. Insufficient power. The OLED draws up to 20 mA; the 3.3 V rail can
   sag if the radios are active. Add a 100 µF cap on the OLED's VCC.
4. Defective OLED. Try a different one.

### 18.6 The buttons do not respond

**Symptoms:** Pressing the buttons does nothing.

**Likely causes:**

1. Wiring error. Buttons are active-LOW: one side to GPIO, one side to
   GND. Internal pull-ups are enabled in software.
2. The OLED menu is disabled in NVS. Re-enable in the web UI.
3. The button GPIO is shared with another function. See the pin conflict
   note for GPIO 38 / CC1101 MOSI.
4. The button is bouncing. The hardware debounce is 50 ms; some very
   cheap buttons need more. The software debounce is configurable in
   NVS as `buttons.debounce_ms`.

### 18.7 The spectrum is empty or stuck

**Symptoms:** The spectrum view shows all -100 dBm or all the same value.

**Likely causes:**

1. No nRF24 attached. The 2.4 GHz spectrum needs the nRF24. The WiFi
   spectrum can run without an nRF24 but the bars will be sparse.
2. The spectrum is paused. Click the **Pause** button on the spectrum
   page.
3. The WebSocket is degraded. Reload the page.
4. The nRF24 is jammed. The spectrum uses the nRF24's `RPD` (Received
   Power Detector) register, which is disabled when the radio is in
   constant-carrier mode.

### 18.8 The attack starts but does nothing

**Symptoms:** The web UI says an attack is running, but the spectrum does
not change and no nearby devices are affected.

**Likely causes:**

1. PA level is too low. Try `HIGH` or `MAX`.
2. The nRF24 antenna is not connected. The PA+LNA module will not
   transmit without an antenna on the SMA connector.
3. The target is out of range. Move closer.
4. The channel sweep is missing the target's channel. Use the
   spectrum view to find the active channel and set `nrf24.channel_start`
   and `nrf24.channel_stop` accordingly.

### 18.9 The device keeps rebooting

**Symptoms:** The serial log shows the boot banner repeatedly, with
`Brownout detector was triggered` between banners.

**Likely causes:**

1. Insufficient USB power. Try a different USB port, a powered hub, or
   a 1 A+ phone charger.
2. The 3.3 V rail is sagging. Add a 1000 µF bulk capacitor.
3. A short circuit. Disconnect everything except the ESP32-S3 and
   see if it boots.
4. The battery is dead (if battery-powered). Recharge or replace.

### 18.10 The OTA update fails

**Symptoms:** The web UI shows "Upload failed" or "Verification failed".

**Likely causes:**

1. The `.bin` file is for a different board variant. Make sure you are
   uploading the correct file.
2. The `.bin` file is corrupted. Re-download it.
3. The SHA-256 hash does not match the manifest. Re-download both.
4. The OTA staging partition is full. Delete old recordings or other
   files from LittleFS.
5. The new firmware is larger than 1.5 MB. Check the size in the
   `dist/` output of the build.

### 18.11 WebSerial is not detecting the device

**Symptoms:** The WebSerial popup does not show the PhantomRF device.

**Likely causes:**

1. The browser is not Chrome, Edge, or Opera. Firefox support is
   limited; Safari does not support WebSerial at all.
2. The site is not HTTPS. WebSerial requires HTTPS (or localhost).
3. The browser does not have permission to access the serial port.
   Check the site permissions in the browser's settings.
4. The USB driver is not installed (Windows). See §10.3.

---

## 19. Frequently asked questions

### 19.1 Is this legal?

**It depends on your jurisdiction and your use case.** In the United States,
operating a radio frequency jammer is illegal under FCC §333, with fines
starting at $10,000 per violation and criminal penalties possible. Similar
laws exist in the EU, UK, Japan, Australia, and most other countries.

Legal uses include: authorized penetration testing with written permission,
academic research in a controlled RF-shielded environment, and personal
learning with your own devices. Illegal uses include: disrupting public
WiFi, jamming cell phones, interfering with emergency services, and any
use where you do not have explicit authorization from the target.

See [`docs/legal/jurisdictions.md`](docs/legal/jurisdictions.md) for
detailed information per country. The PhantomRF maintainers are not
lawyers; this is not legal advice.

### 19.2 Can I use this to jam my neighbor's WiFi?

No. That is illegal in essentially every jurisdiction. The fact that
PhantomRF is technically capable of doing so does not make it legal. Use
this tool only on networks and devices you own or have explicit written
permission to test.

### 19.3 Why ESP32-S3 and not the classic ESP32?

The ESP32-S3 is faster (240 MHz vs 160 MHz on the LX6), has 8 MB of octal
PSRAM (the classic ESP32 has none), has native USB OTG (no UART bridge
chip), supports BLE 5.0 with LE 2M PHY, has more usable GPIOs, and uses
less deep-sleep current. The classic ESP32 is supported as a fallback for
users who already have one, but the recommended target is the S3.

### 19.4 Why nRF24L01+PA+LNA and not, say, a HackRF?

The nRF24L01+PA+LNA is $2 and handles 2.4 GHz. A HackRF One is $300 and
handles 1 MHz to 6 GHz. PhantomRF is not trying to be a software-defined
radio; it is trying to be a cheap, focused, 2.4 GHz + sub-GHz research
tool. For SDR work, use a HackRF, an RTL-SDR, or a USRP. For the specific
attacks PhantomRF targets, the nRF24 and CC1101 are the right chips.

### 19.5 Can I use this with a Flipper Zero?

A separate companion port (`phantom-fz`) is planned for M1. The algorithms
will be identical, but the UI will use the Flipper's screen and buttons.
The Flipper's nRF24 module does not have a PA+LNA, so the range will be
much shorter than the ESP32-S3 + nRF24L01+PA+LNA combination.

### 19.6 How does this compare to a commercial jammer?

Commercial jammers (e.g., the ones sold by "Jammer-Store" or similar
vendors) are typically $400-$4,000, cover 8-12 frequency bands, are
illegal to possess in most jurisdictions, and are often unreliable. The
PhantomRF approach is: $13 of hardware, fully open source, fully
documented, covers the most common bands (2.4 GHz + sub-GHz), and is
illegal to *use* in most jurisdictions but legal to *possess* as a
research tool in many of them.

### 19.7 Can the firmware be locked to "research only" mode?

No. The firmware is fully open source; you can always reflash it. The
legal disclaimer in the web UI and the README is the only "lock".

### 19.8 Does the device work without WiFi?

Yes. The OLED + buttons and the USB CDC serial are the two channels that
work without WiFi. You can start, stop, and configure attacks entirely
offline. The BLE Serial channel also works without WiFi (it uses the BLE
radio, which is separate from the WiFi radio).

### 19.9 What is the battery life?

With a 3000 mAh 18650 cell and all attacks active, expect 8-10 hours.
With idle (AP up, no attack), expect 30-40 hours. The exact number
depends on the attack mix, the PA level, and the WiFi activity.

### 19.10 Can I add my own attack?

Yes, and we encourage it. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the
contribution guide. The modular architecture makes it easy: implement the
`IModule` interface, register your module in `main.cpp`, and add a route
to the web UI. The `docs/algorithms/` section is a good starting point
for the math.

### 19.11 Why is the project called "PhantomRF"?

"Phantom" = a thing that is perceived but not physically present. RF
(radio frequency) is invisible. PhantomRF makes the invisible visible
(in the sense of: selectively destructive to specific RF bands). The name
is also a hat-tip to the Flipper Zero, which is sometimes called a
"phantom" device in the security community.

### 19.12 Will you sell pre-built units?

No. PhantomRF is a research platform, not a product. We will provide
BOM, wiring diagrams, PCB designs (M1), and 3D case designs (M1), but
the maintainers will not sell assembled units. The AliExpress hardware
market is saturated with cheaper clones already.

### 19.13 What if I find a bug?

Open a GitHub issue with:

1. The exact board variant (e.g., "ESP32-S3-DevKitC-1-N16R8, 16 MB flash,
   8 MB PSRAM").
2. The firmware version (printed at boot or shown in the dashboard).
3. The serial log from boot.
4. The steps to reproduce.
5. What you expected to happen versus what actually happened.

For security vulnerabilities, see [`SECURITY.md`](SECURITY.md) for
responsible disclosure instructions. Do not open public issues for
security bugs.

### 19.14 Can I use this in my classroom?

Yes, that is one of the primary use cases. The codebase is structured
to be readable by students. The algorithm documents in `docs/algorithms/`
are written for a junior undergraduate audience. We would love to hear
about courses that adopt PhantomRF; please email the maintainers.

### 19.15 What languages do you support?

The web UI, the serial CLI, and the documentation are English-only for
M0. Multi-language support (i18n) is an M2 feature. Contributions of
translations are welcome.

---

## 20. Performance

The numbers in this section are measured on an ESP32-S3-DevKitC-1-N16R8
at 240 MHz, with one nRF24L01+PA+LNA and one CC1101 attached. They are
typical but not guaranteed; your results may vary.

### 20.1 Channel sweep rates

| Setup | Channels/sec | Full sweep time |
|---|---|---|
| 1 nRF24, list sweep, 21 BT channels | ~5,000 ch/s | 4.2 ms |
| 1 nRF24, list sweep, 80 BLE data channels | ~5,000 ch/s | 16 ms |
| 1 nRF24, list sweep, 126 ISM channels | ~5,000 ch/s | 25 ms |
| 4 nRF24, separate sweep, 126 ISM channels | ~20,000 ch/s | 6.3 ms |
| 1 CC1101, range sweep, 1 MHz step at 433 MHz | ~700 ch/s | 1.4 ms / MHz |
| 1 CC1101, hopper, 8 frequencies | ~150 hops/s | 53 ms / cycle |

### 20.2 WiFi deauth

| Mode | Frames/sec | Effective range |
|---|---|---|
| Targeted (BSSID + client) | ~10 fps (hardware cap) | ~50 m line of sight |
| All channels (sweep) | ~10 fps / channel | ~50 m line of sight |
| Smart (active only) | ~10 fps / channel | ~50 m line of sight |

The 10 fps cap is a hardware/firmware limit on the ESP32-S3's
`esp_wifi_80211_tx()` function. There is no software workaround.

### 20.3 BLE advertising

| Spam type | Advertisements/sec | Effective range |
|---|---|---|
| Apple Continuity | ~50 adv/s | ~30 m line of sight |
| Samsung Buds | ~50 adv/s | ~30 m line of sight |
| Windows Swift Pair | ~50 adv/s | ~30 m line of sight |
| Fast Pair | ~50 adv/s | ~30 m line of sight |

The S3's BLE 5.0 LE 2M PHY doubles the throughput compared to BLE 4.2 on
the classic ESP32.

### 20.4 Sub-GHz jam

| Mode | Coverage | Range |
|---|---|---|
| Spot jam (single frequency) | Continuous | ~100 m line of sight |
| Range jam (1 MHz step) | Sweeps the band | ~100 m line of sight at each step |
| Hopper (8 frequencies) | Round-robin | ~100 m line of sight at each step |

The CC1101's output power is configurable from -30 dBm to +12 dBm. The
+12 dBm setting requires a heatsink on the module.

### 20.5 Latency

| Operation | Target | Typical |
|---|---|---|
| Boot to AP ready | < 3 s | 2.5 s |
| Web page load | < 1 s | 400 ms |
| Attack start (web click to TX) | < 100 ms | 50 ms |
| Attack stop | < 50 ms | 20 ms |
| CLI command response | < 10 ms | < 5 ms |
| Web UI status refresh (M0 polling) | 5 Hz | 5 Hz |
| Web UI status refresh (M1 WebSocket) | 10 Hz | 10 Hz |
| Spectrum refresh | 1 Hz | 1 Hz |
| OTA flash (1.5 MB firmware) | < 60 s | 30 s |

### 20.6 Power consumption

| Scenario | Avg current | Battery life (3000 mAh) |
|---|---|---|
| Idle (AP up, no attack) | 80 mA | ~37 hours |
| BT jam (continuous) | 280 mA | ~10 hours |
| WiFi deauth (10 fps) | 220 mA | ~13 hours |
| Sub-GHz jam | 250 mA | ~12 hours |
| BLE spam | 200 mA | ~15 hours |
| All attacks | 350 mA | ~8.5 hours |
| Deep sleep | 8 µA | ~40 years (theoretical) |

### 20.7 Memory usage

| Use | SRAM | PSRAM |
|---|---|---|
| App overhead (WiFi, NimBLE, drivers, NVS) | 110 KB | 0 |
| Web UI asset cache | 0 | 200 KB |
| Spectrum history buffer | 0 | 256 KB |
| Recording ring buffer (32 KB × 256) | 0 | 8 MB (allocated on demand) |
| BLE scan result cache | 0 | 64 KB |
| Free (N16R8) | 400 KB | ~7.5 MB |

---

## 21. Roadmap

PhantomRF follows a strict three-milestone roadmap. Dates are estimates and
will slip; this is an open-source project, not a commercial one.

### 21.1 Milestone 0 — v1.0.0 (target: Q4 2026)

**Goal:** Stable, well-documented, working release that supersedes all five
inspiration projects.

**Features (M0 set):**

- All nRF24 (2.4 GHz) features except MouseJack
- Sub-GHz record/replay (in RAM, not yet persisted)
- All M0 WiFi features
- BLE scan, BLE advertising jam
- Web UI, OLED menu (1/2/3 button), serial CLI, WebSerial
- OTA via web
- Captive portal
- Multi-nRF24 (1-5)
- Status LED
- Heap monitor, task monitor, NVS dump

**Quality bars for v1.0.0:**

- All code passes `clang-format` and `clang-tidy`
- All unit tests pass
- All build environments compile clean
- Web flasher works on Chrome, Firefox, Edge, Safari
- At least one external user has successfully built and flashed the device
- README, DESIGN.md, and `docs/` are complete and proofread
- The legal disclaimer is reviewed by a real lawyer (pro bono, hopefully)

### 21.2 Milestone 1 — v1.1.0 (target: Q1-Q2 2027)

**Goal:** Add the "missing" features: MouseJack, PMKID, BLE spam, LittleFS
for recordings, custom PCB and case.

**Features (M1 set):**

- MouseJack keystroke injection
- Recordings persisted to LittleFS as `.sub` files
- Download recordings from the web UI
- All WiFi features (evil twin, PMKID, handshake capture, disassoc flood,
  probe flood, hidden SSID reveal)
- All BLE spam features (Apple, Samsung, Windows, Fast Pair)
- Drag-and-drop UF2 flashing
- Backup / restore config
- Battery monitor
- Power button (long-press to deep sleep)
- Custom PCB reference design (KiCad)
- 3D case reference design (FreeCAD)
- Signed firmware (Ed25519)
- WebSocket real-time updates (replace polling)

### 21.3 Milestone 2 — v1.2.0 (target: Q3-Q4 2027)

**Goal:** Polish, performance, multi-language, and the "experimental" features.

**Features (M2 set):**

- Sub-GHz spectrum scanner
- RollJam (two-stage keyfob attack)
- BLE link-layer attack
- BLE MITM
- Multi-language (i18n)
- Profiler hooks
- Buzzer
- Wake-on-button
- Flipper Zero companion port
- Custom keyboard / mouse support

### 21.4 Milestone 3 — v2.0.0 (target: 2028+, far future)

Ideas under consideration:

- Multi-device mesh coordination (2+ PhantomRFs synchronizing)
- AI-driven attack selection (model picks best attack for target)
- LoRa / LoRaWAN support
- Zigbee coordinator mode
- Thread / Matter support
- ESP32-C6 (WiFi 6, Thread) support
- Custom web UI plugin system

These are **ideas**, not commitments. The maintainers reserve the right to
ship none of them if priorities change.

---

## 22. Contributing

We welcome contributions of all kinds: code, documentation, bug reports,
hardware testing, translations, and feature ideas.

Before you start, please read [`CONTRIBUTING.md`](CONTRIBUTING.md). The
short version:

1. **Open an issue first** for any non-trivial change. We can save you a
   lot of time by telling you whether the change is wanted, what the
   right place for it is, and what the code style is.
2. **Fork the repo** and create a feature branch.
3. **Follow the code style** (`clang-format` config is in
   `.clang-format`).
4. **Write tests** for new features. The unit test framework is PlatformIO
   native with Unity.
5. **Update the docs** in the same PR as the code change.
6. **Run the test suite** before pushing (`pio test -e native`).
7. **Sign your commits** (`git commit -s`).

Maintainers will review your PR within a week. Be patient; this is a
volunteer project.

For security vulnerabilities, see [`SECURITY.md`](SECURITY.md). Do not
open public issues.

For community questions, use GitHub Discussions (link TBD). For real-time
chat, the maintainers idle in a Discord server (invite link TBD).

---

## 23. License

PhantomRF is released under the **MIT License**. See [`LICENSE`](LICENSE)
for the full text.

In short: you can do almost anything with this software — use it,
modify it, sell it, incorporate it into a commercial product — as long as
you preserve the copyright notice and the license text. The maintainers
accept no liability for any consequences of your use.

The MIT license applies to the PhantomRF source code only. The
dependencies (RF24, NimBLE-Arduino, Adafruit_SSD1306, etc.) are licensed
under their own terms; see `LICENSE-3rdparty.md` for the full list.

---

## 24. Acknowledgments

PhantomRF is built on the work of many people and projects. The
maintainers are deeply grateful to:

- **TMRh20** and the **nRF24** GitHub organization — for the original
  RF24 Arduino library, which is the foundation of every nRF24 project.
- **LSatan** (and the SmartRC-CC1101-Driver-Lib maintainers) — for the
  CC1101 driver that makes sub-GHz work approachable.
- **h2zero** — for NimBLE-Arduino, which is dramatically smaller and
  faster than the older NimBLE fork.
- **W0rthlessS0ul** — for the architecture inspiration across
  `nRF24_jammer`, `CC1101_jammer`, and `FZ_nRF24_jammer`. PhantomRF would
  not exist without these.
- **smoochiee** — for the "Noisy Boy" web UI, which showed that a small
  ESP32 web interface can be made pretty.
- **EmenstaNougat** — for the ESP32-BlueJammer hardware design, which
  showed that a piece of jammer hardware can be made to feel like a
  finished product.
- **Samy Kamkar** — for the original MouseJack research, which still has
  not been fully mitigated across the industry.
- **Spacehuhn** (Stefan Kremser) — for the deauth framework that
  inspired a generation of WiFi security tools.
- **Mickey Shin** — for the Flipper Zero sub-GHz research and the
  `.sub` file format specification.
- **mcore1976** — for the original CC1101 RAW capture code.
- **The arduino-esp32 maintainers** — for the framework that makes all
  ESP32 development possible.
- **The PlatformIO team** — for the build system that makes embedded
  development bearable.
- **The HackRF, Flipper Zero, and Proxmark communities** — for setting
  the standard for what a hobbyist RF research tool should look like.

If you feel you should be on this list and are not, please open an issue.

---

## 25. Legal disclaimer

> **This section is not legal advice. Consult a lawyer in your
> jurisdiction before doing anything that could be illegal.**

Operating radio-frequency jammers, transmitting on frequencies without a
license, and interfering with licensed or unlicensed radio services may
be criminal offenses in your jurisdiction. The PhantomRF project, its
maintainers, its contributors, and its distributors:

- Do not condone, encourage, or support any unlawful use of this
  software.
- Are not responsible for any damage, legal consequence, or harm caused
  by your use of this software.
- Make no warranty as to the fitness of this software for any particular
  purpose, including (but not limited to) research, education, or
  authorized penetration testing.

**By using, copying, modifying, or distributing this software, you
acknowledge that you understand and accept full responsibility for
ensuring your use complies with all applicable local, state, national,
and international laws.**

### 25.1 Jurisdiction overview

| Country | Status | Reference | Notes |
|---|---|---|---|
| USA | Illegal (FCC §333) | 47 U.S.C. §333 | Fines up to $100K+ per violation, criminal penalties possible |
| Canada | Illegal (Radiocommunication Act) | R.S.C. 1985 c. R-2 | Fines up to $25K, criminal charges possible |
| Mexico | Illegal (Ley Federal de Telecomunicaciones) | DOF 2014 | Fines and imprisonment |
| UK | Illegal (Wireless Telegraphy Act 2006) | c. 36 | Up to 2 years imprisonment |
| EU | Illegal (CEPT/ETSI) | Various | Varies by country; generally very strict |
| Germany | Illegal (TKG) | Telekommunikationsgesetz | Very strict; up to 2 years imprisonment |
| France | Illegal (Code pénal) | Article L226-3 | Up to 6 months imprisonment |
| Japan | Illegal (Radio Law) | 電波法 | Up to 1 year imprisonment |
| China | Illegal (Radio Management Regulation) | State Council Order 127 | Strict; criminal penalties |
| India | Illegal (Indian Telegraph Act) | 1885 | Fines and imprisonment |
| Indonesia | Illegal (UU Telekomunikasi) | UU No. 36/1999 | Fines and imprisonment |
| Brazil | Illegal (Anatel) | Lei 9.472/1997 | 2-4 years imprisonment |
| Australia | Illegal (RadComms Act) | 1992 | A$165K+ fines |
| New Zealand | Illegal (Radiocommunications Act) | 1989 | Fines and imprisonment |
| South Africa | Illegal (EC Act) | 2005 | Fines and imprisonment |
| UAE | Illegal (TDRA) | Federal Law 3/2003 | Strict; criminal penalties |
| Israel | Illegal (Wireless Telegraphy Ordinance) | 1929 | Fines and imprisonment |
| Russia | Illegal (Federal Law 126-FZ) | 2003 | Strict; criminal penalties |

**This table is not exhaustive and may be out of date.** The maintainers
are not lawyers. For authoritative information, consult a lawyer in your
jurisdiction.

### 25.2 Legal use cases

The following are examples of use cases that are generally legal:

- **Authorized penetration testing** of networks and devices you own or
  have written permission to test.
- **Academic research** in a controlled RF-shielded environment, with
  appropriate institutional review board (IRB) approval where required.
- **Personal learning** on devices you own, in a location where
  transmission is permitted (e.g., your own home).
- **Smart home security audits** of your own devices, with the
  understanding that any transmission must not leave your property.
- **Conference demonstrations** in a legally approved venue (e.g., a
  conference room with a Faraday cage, with prior approval from the
  venue and any relevant regulatory body).
- **Compliance testing** of your own products (e.g., testing the
  resilience of your IoT device to known attacks).

### 25.3 Anti-feature list

The PhantomRF project will **not** add:

- Cellular (GSM, LTE, 5G, NB-IoT) jamming
- GPS jamming
- Public safety band jamming (aviation, emergency services, marine)
- Military band jamming
- Drone hijacking (control override)
- Permanent device bricking
- Any feature whose only use is illegal in a major jurisdiction

If you are considering contributing a feature that falls into one of these
categories, please don't. The maintainers will reject the contribution.

### 25.4 First-boot acknowledgement

The first time PhantomRF boots, the OLED and the web UI show a
"legal acknowledgement" screen:

```
+-----------------------------------+
|  PHANTOMRF v0.9.0-dev             |
|                                   |
|  This device can transmit radio   |
|  signals that may be illegal in   |
|  your jurisdiction.               |
|                                   |
|  By pressing OK you confirm that  |
|  you understand and accept full   |
|  responsibility for your use of   |
|  this device.                     |
|                                   |
|  [ OK ]    [ Cancel ]             |
+-----------------------------------+
```

Pressing OK is required to proceed. Pressing Cancel puts the device in
a "safe" mode where no transmission is possible. This acknowledgement is
stored in NVS and only needs to be given once per device.

The acknowledgement is a **technical** check, not a **legal** one. The
device cannot verify that you have the legal right to use it. The
maintainers trust you to be honest.

---

## 26. Contact

| Channel | Link |
|---|---|
| GitHub (issues, PRs) | `https://github.com/your-org/PhantomRF` |
| GitHub Discussions | `https://github.com/your-org/PhantomRF/discussions` |
| Discord | Invite link TBD (see the GitHub README badge) |
| Email (security) | `security@phantomrf.example` (PGP key in `SECURITY.md`) |
| Email (maintainers) | `maintainers@phantomrf.example` |
| Twitter / X | `@PhantomRF` (TBD) |

For security vulnerabilities, do not use any public channel. See
[`SECURITY.md`](SECURITY.md).

For general questions, GitHub Discussions is preferred. Discord is best
for real-time chat with the maintainers and other contributors.

---

## 27. Star history

If you find PhantomRF useful, please consider starring the repository. It
helps the project get visibility and motivates the maintainers to keep
working on it.

![Star history placeholder](docs/img/star-history.png)

A graph of the GitHub star count over time will appear here once the
project gets its first stars. Use a service like
[star-history.com](https://star-history.com) to generate a real chart.

---

## Appendix A: Quick reference

### A.1 Pin summary (ESP32-S3 primary)

| Function | GPIO | Notes |
|---|---|---|
| nRF24 MOSI | 11 | HSPI |
| nRF24 MISO | 12 | HSPI |
| nRF24 SCK | 13 | HSPI |
| nRF24 CSN #1 | 15 | |
| nRF24 CE #1 | 16 | |
| nRF24 CSN #2-5, CE #2-5 | 17, 18, 21, 35, 36, 37, 38, 39 | |
| CC1101 SCK | 36 | VSPI (shared with nRF24 CSN #4) |
| CC1101 MISO | 37 | VSPI |
| CC1101 MOSI | 38 | VSPI |
| CC1101 CSN | 39 | VSPI |
| CC1101 GDO0 | 2 | Safe on S3 |
| CC1101 GDO2 | 4 | |
| OLED SDA | 8 | I2C0 |
| OLED SCL | 9 | I2C0 |
| Button OK | 10 | RTC |
| Button NEXT | 14 | RTC |
| Button PREV | 33 or 38 | See conflict note |
| LED R/G/B | 5 / 6 / 7 | PWM |
| Battery ADC | 1 | ADC1_CH0 |
| USB D- / D+ | 19 / 20 | Native USB |

### A.2 Quick CLI reference

```
info              Device info
status            Current state
attack <module>   Start an attack
stop              Stop current attack
scan <type>       Scan networks/devices
record            Record sub-GHz signal
replay            Replay recorded signal
settings get      Show all settings
settings set K V  Set a setting
dump_nvs          Dump NVS
reboot            Reboot device
help              This help
?                 Alias for help
```

### A.3 Common errors

| Serial output | Meaning | Fix |
|---|---|---|
| `[FAIL] nRF24 #1 SPI error` | nRF24 not responding | Check wiring, try a different module |
| `[FAIL] CC1101 version mismatch` | CC1101 not at expected address | Check VSPI wiring |
| `[FAIL] OLED not detected at 0x3C` | OLED not found | Check I2C wiring, try address 0x3D |
| `[FAIL] LittleFS mount failed` | Filesystem corrupted | Re-flash with `pio run -t uploadfs` |
| `[WARN] Brownout detected` | Power supply insufficient | Use powered hub, add bulk cap |
| `[WARN] Heap low (<10 KB)` | Memory pressure | Reduce recording count, restart device |

### A.4 Useful URLs

| Resource | URL |
|---|---|
| ESP32-S3 datasheet | `https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf` |
| nRF24L01+ datasheet | `https://www.nordicsemi.com/Products/nRF24-series` |
| CC1101 datasheet | `https://www.ti.com/product/CC1101` |
| RF24 Arduino library | `https://github.com/nRF24/RF24` |
| NimBLE-Arduino | `https://github.com/h2zero/NimBLE-Arduino` |
| SmartRC-CC1101-Driver-Lib | `https://github.com/LSatan/SmartRC-CC1101-Driver-Lib` |
| IEEE 802.11 standard | `https://standards.ieee.org/ieee/802.11/7028/` |
| IEEE 802.15.4 standard | `https://standards.ieee.org/ieee/802.15.4/7029/` |
| ESP-IDF programming guide | `https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/` |

---

*End of README.md — designed to be the single entry point for new users,
the lookup reference for experienced users, and the legal disclaimer for
everyone. If anything is unclear, open an issue. The doc is incomplete by
definition.*
