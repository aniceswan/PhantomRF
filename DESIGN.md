# 📐 DESIGN.md — PhantomRF Architecture & Implementation Blueprint

> **Status:** Draft v1.0 — pending review
> **Project codename:** `PhantomRF` (working title, may change)
> **Audience:** Maintainers, contributors, advanced users
> **Reading time:** ~45 min

This document is the **authoritative design specification** for the PhantomRF project. It answers the questions:

1. **Why does this project exist?** (problem statement, goals, non-goals)
2. **What does it do?** (full feature list, by category)
3. **What hardware does it run on?** (ESP32-S3 primary, ESP32 family supported)
4. **How is the code organized?** (directory structure, build system, modules)
5. **How does each algorithm work?** (jamming theory, frame construction, channel math)
6. **How does the user interact with it?** (web UI, OLED menu, serial CLI)
7. **How is it built and flashed?** (PlatformIO, Arduino IDE, web flasher, UF2)
8. **What is the development roadmap?** (phases, milestones, future ideas)

If you read this top-to-bottom, you should be able to understand every line of code in the project without ever opening it. If anything is unclear, **open an issue** — the doc is incomplete by definition.

---

## Table of Contents

- [0. Project Identity](#0-project-identity)
- [1. Problem Statement & Goals](#1-problem-statement--goals)
- [2. Lineage & Inspirations](#2-lineage--inspirations)
- [3. High-Level Architecture](#3-high-level-architecture)
- [4. Hardware Target](#4-hardware-target)
- [5. Feature Catalog](#5-feature-catalog)
- [6. Software Architecture](#6-software-architecture)
- [7. Directory Structure](#7-directory-structure)
- [8. Pinout Specification](#8-pinout-specification)
- [9. Build System](#9-build-system)
- [10. Algorithm Reference](#10-algorithm-reference)
- [11. Web Interface Design](#11-web-interface-design)
- [12. OLED / Serial / Button Subsystem](#12-oled--serial--button-subsystem)
- [13. Persistence Model](#13-persistence-model)
- [14. Power & Battery](#14-power--battery)
- [15. Performance Budget](#15-performance-budget)
- [16. Safety, Legal, Ethics](#16-safety-legal-ethics)
- [17. Testing Strategy](#17-testing-strategy)
- [18. Documentation Plan](#18-documentation-plan)
- [19. Roadmap & Milestones](#19-roadmap--milestones)
- [20. Glossary](#20-glossary)
- [21. Open Questions for Reviewer](#21-open-questions-for-reviewer)

---

## 0. Project Identity

| Attribute | Value |
|---|---|
| **Working name** | PhantomRF |
| **License** | MIT (permissive, matches the lineage projects) |
| **Primary platform** | Espressif ESP32-S3 (Xtensa LX7 dual-core) |
| **Secondary platforms** | ESP32 classic, ESP32-C3, ESP32-S2 (build-flag gated) |
| **Companion platforms** | Flipper Zero (separate repo, shared algorithms) |
| **Language** | C++ (Arduino framework), some C for the Flipper port |
| **Build system** | PlatformIO (primary), Arduino IDE 2.x (secondary) |
| **Firmware size target** | ≤ 1.5 MB (3 MB app partition with OTA headroom) |
| **First stable release** | v1.0.0 (estimated Q4 2026) |
| **Audience** | Security researchers, RF hobbyists, students, makers |

**Why the name "PhantomRF"?** Phantom = a thing that appears without being physically present (because RF waves are invisible). RF = radio frequency. The project makes the invisible visible (in the sense of: selectively destructive to specific RF bands).

---

## 1. Problem Statement & Goals

### 1.1 Problem

Five open-source projects cover parts of the 2.4 GHz / sub-GHz RF research space, but each has gaps:

| Project | Strong at | Weak at |
|---|---|---|
| W0rthlessS0ul/nRF24_jammer | ESP32 + nRF24, web UI, OLED, serial CLI | Only 2.4 GHz, no sub-GHz, bugs in code |
| W0rthlessS0ul/CC1101_jammer | ESP32 + CC1101 sub-GHz, record/replay | No web UI improvements, no LittleFS, EEPROM bug |
| W0rthlessS0ul/FZ_nRF24_jammer | Flipper Zero + nRF24 | No web UI, no WiFi deauth, smaller display |
| smoochiee/Noisy-boy | Simple web UI for nRF24 | Closed source, no docs, magic pin variants |
| EmenstaNougat/ESP32-BlueJammer | Polished hardware, Combo firmware | Closed source, no WiFi deauth, no spectrum |

**There is no single project that:**
- Combines **2.4 GHz (nRF24) + sub-GHz (CC1101)** in one device with one firmware
- Has a **modern web UI** with real-time spectrum visualization
- Has **BLE spam** (Apple/Samsung/Fast Pair popups)
- Has **WiFi deauth + beacon flood + PMKID capture** in one place
- Runs on **ESP32-S3** (the most capable current ESP32) with native USB
- Has **comprehensive English documentation** that explains the "why", not just the "how"
- Is **fully open source** with no closed binaries

### 1.2 Goals (in priority order)

1. **Correctness** — every feature works as documented; no silent failures
2. **Documentation** — every algorithm explained in plain English, every pin justified
3. **Modularity** — every attack is an independent module that can be enabled/disabled
4. **Cross-platform** — ESP32-S3 primary, classic ESP32 / ESP32-C3 supported, Flipper Zero ported
5. **Educational value** — a beginner can read the code, understand RF concepts, and learn
6. **Customizability** — every channel, every timing, every PA level is configurable
7. **Safety** — built-in warnings, no silent radio activation, hardware interlocks where possible
8. **Community-friendly** — clear contribution guidelines, code review process

### 1.3 Non-Goals

- ❌ **Stealth / nation-state use** — this is a research / educational tool
- ❌ **Permanent damage** — no firmware features that can brick a target device
- ❌ **Cellular jamming** (GSM/LTE/5G) — wrong hardware, wrong jurisdiction, wrong scope
- ❌ **Closed-source paywall** — the entire project is MIT
- ❌ **Compiled-binary distribution** — sources only, you build it

### 1.4 Target Audience Profiles

| Profile | Pain point today | How PhantomRF helps |
|---|---|---|
| **University student** studying wireless security | "W0rthlessS0ul's code is opaque, no comments" | Every function has a docstring, every algorithm has a markdown explainer |
| **Penetration tester** doing authorized WiFi audits | "I need WiFi deauth + BLE spam + 2.4 GHz jamming in one tool" | All in one device, one firmware |
| **Hardware hobbyist** with a 3D printer | "I want a polished case for my ESP32+nRF24 rig" | Includes PCB design + 3D case (V3 style) |
| **Flipper Zero user** | "FZ nRF24 jammer is great but limited" | Companion FZ port uses same algorithm library, same UI philosophy |
| **Smart home red teamer** | "I need to test 433 MHz sensors and 2.4 GHz Zigbee in one sweep" | Dual radio: CC1101 + nRF24 in same device |

---

## 2. Lineage & Inspirations

### 2.1 Direct lineage

```
                     2014 ─── nRF24L01+ library by TMRh20
                              │
                              ├─── Fork by bmorcelli (nRF24 mousejack)
                              │         │
                              │         └─── tesa-klebeband (white-noise test)
                              │                   │
                              │                   └─── smoochiee (Noisy Boy 2024)
                              │                             │
                              │                             └─── W0rthlessS0ul/nRF24_jammer
                              │                                       │
                              │                                       └─── W0rthlessS0ul/CC1101_jammer
                              │                                       │
                              │                                       └─── W0rthlessS0ul/FZ_nRF24_jammer
                              │
                              └─── EmenstaNougat (independent path, 2024)
```

### 2.2 What we keep

- **Constant carrier trick** for nRF24 (CONT_WAVE | PLL_LOCK in RF_SETUP)
- **Async-serial mode for CC1101** (setCCMode(0), bit-bang GDO0)
- **Promiscuous-mode deauth** (`esp_wifi_set_promiscuous` + custom callback)
- **Channel hopping with separate modules** (`i = ch % module_count`)
- **3-button + 1-button + 2-button configurations**
- **Web UI with FPSTR HTML + JS polling**
- **OTA update via web**

### 2.3 What we fix

- **memset/memcpy NULL bug** in W0rthlessS0ul's `handleSetNrf24Command`
- **BLE channel array out-of-bounds** in W0rthlessS0ul's `ble_data_jam`
- **Missing CHIP_SELECT handling** in W0rthlessS0ul's multi-nRF24 init
- **EEPROM offset overlap** in W0rthlessS0ul's CC1101_jammer (`range_step` / `rec_sampling`)
- **`setCCMode(2)` invalid value** after play() in CC1101_jammer
- **Dead code**: `deauth.cpp` in nRF24_jammer (never compiled)
- **Dead code**: `scan_BLE()` in nRF24_jammer (declared, never called)
- **Mysterious "noisy boy" hardcoded behavior** (no clean abstraction)

### 2.4 What we add (new in PhantomRF)

- **Dual-radio (nRF24 + CC1101) in one device**
- **BLE spam** (Apple Continuity, Samsung Buds, Windows Swift Pair, Fast Pair)
- **MouseJack / nRF24 keyboard injection** (original research by Samy Kamkar, ported)
- **PMKID capture** (handshake-less WPA2 attack)
- **Real-time spectrum waterfall** on OLED and web
- **LittleFS for file storage** (recordings, logs, custom SSID lists)
- **Drag-and-drop UF2 flashing** (via ESP32-S3 native USB OTG)
- **Hot-reloadable web UI** (assets served from LittleFS, not embedded)
- **Modular attack framework** — every attack is a class, register at boot
- **Comprehensive English docs** in `/docs`

---

## 3. High-Level Architecture

### 3.1 System block diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                       ESP32-S3-WROOM-1-N16R8                    │
│  (Xtensa LX7 dual-core, 240 MHz, 512 KB SRAM, 8 MB octal PSRAM) │
│                                                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────┐│
│  │ WiFi (STA+  │  │ BLE 5.0     │  │ USB OTG     │  │ USB-Ser/ ││
│  │ AP)         │  │ (NimBLE)    │  │ (TinyUSB)   │  │ JTAG     ││
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬─────┘│
│         │                │                │              │     │
│  ═══════╪════════════════╪════════════════╪══════════════╪════  │
│         ▼                ▼                ▼              ▼     │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │            PhantomRF Core (C++ namespace phm)           │   │
│  │  - State machine (main loop + worker tasks)            │   │
│  │  - Module registry (register/lookup attacks)           │   │
│  │  - Settings (Preferences/NVS)                          │   │
│  │  - Event bus (Core0 ↔ Core1)                           │   │
│  └──────────────────────────────────────────────────────────┘   │
│         │                                                        │
│  ═══════╪══════════════════════════════════════════════════════  │
│         ▼                                                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐            │
│  │ Radio Drivers│  │ Attack       │  │ UI Layer     │            │
│  │ - nRF24×1-5  │  │ Modules      │  │ - Web        │            │
│  │ - CC1101×1-2 │  │ - BT jam     │  │ - OLED       │            │
│  │              │  │ - BLE jam    │  │ - Serial     │            │
│  │              │  │ - WiFi atk   │  │              │            │
│  │              │  │ - 2.4G jam   │  │              │            │
│  │              │  │ - SubGHz jam │  │              │            │
│  │              │  │ - Spectrum   │  │              │            │
│  │              │  │ - Record/Replay                          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘            │
│         │                 │                 │                    │
│  ═══════╪═════════════════╪═════════════════╪═════════════════    │
│         ▼                 ▼                 ▼                    │
│  ┌────────────────────────────────────────────────────────┐     │
│  │               Hardware Abstraction Layer               │     │
│  │  - SPI bus manager (HSPI for nRF24, VSPI for CC1101)  │     │
│  │  - GPIO helpers (button debounce, LED, buzzer)        │     │
│  │  - NVS / LittleFS wrapper                             │     │
│  └────────────────────────────────────────────────────────┘     │
│         │                                                        │
└─────────┼────────────────────────────────────────────────────────┘
          │
          ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                  External Hardware                           │
   │                                                             │
   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
   │  │ nRF24L01 │  │ CC1101   │  │ SSD1306  │  │ Buttons  │     │
   │  │ +PA+LNA  │  │ sub-GHz  │  │ OLED     │  │ 1-3 + LED│     │
   │  │ ×1-5     │  │ ×1-2     │  │ 128×64   │  │          │     │
   │  └──────────┘  └──────────┘  └──────────┘  └──────────┘     │
   │                                                             │
   │  ┌──────────┐  ┌──────────┐  ┌──────────┐                   │
   │  │ nRF24L01 │  │ CC1101   │  │ Battery  │                   │
   │  │ (extra)  │  │ (extra)  │  │ 18650    │                   │
   │  │ HSPI     │  │ VSPI     │  │ +TP4056  │                   │
   │  └──────────┘  └──────────┘  └──────────┘                   │
   └─────────────────────────────────────────────────────────────┘
```

### 3.2 Core↔Module separation

PhantomRF is structured as a **core + modules** design:

- **Core** (`src/core/`): state machine, settings, persistence, event bus, module registry
- **Modules** (`src/modules/`): each attack / radio / UI layer is an independent module
- **HAL** (`src/hal/`): hardware abstraction (SPI bus, GPIO, storage)
- **Web** (`web/`): HTML/JS/CSS assets served from LittleFS

Every module has the same interface:

```cpp
class IModule {
public:
    virtual const char* name() const = 0;
    virtual const char* description() const = 0;
    virtual void setup() {}              // called once at boot
    virtual void loop() {}               // called every main-loop tick
    virtual void registerRoutes(WebServer& server) {}  // web UI
    virtual void registerCLI(Console& console) {}      // serial CLI
    virtual void registerMenu(Menu& menu) {}          // OLED menu
    virtual AttackResult attack() { return AttackResult::Idle; }
    virtual void stop() {}
};
```

Modules register themselves at boot:

```cpp
void setup() {
    ModuleRegistry::registerModule(new Nrf24Module());
    ModuleRegistry::registerModule(new Cc1101Module());
    ModuleRegistry::registerModule(new WifiAttackModule());
    ModuleRegistry::registerModule(new BleAttackModule());
    ModuleRegistry::registerModule(new SpectrumModule());
    // ... etc
    ModuleRegistry::setupAll();
}
```

This makes it trivial to add/remove attacks without touching the core.

### 3.3 Task pinning (RTOS)

Two cores; we pin tasks to avoid contention:

| Task | Core | Priority | Notes |
|---|---|---|---|
| Arduino `loop()` | 1 | 1 (default) | Calls ModuleRegistry::loopAll() |
| WiFi/BLE stack | 0 | OS-default | Managed by arduino-esp32 |
| nRF24 jammer worker | 0 | 3 | High priority to keep channel hopping tight |
| CC1101 jammer worker | 0 | 3 | Same |
| Web server | 0 | 1 | OS-default |
| Spectrum sweep | 1 | 1 | Runs during scan, not during attack |
| CLI parser | 1 | 1 | Always responsive |

**Watchdog:** `disableCore0WDT()` and `disableCore1WDT()` are called in `setup()` to prevent the 5-second TWDT from killing jammer workers (which intentionally block for many seconds during a sweep).

---

## 4. Hardware Target

### 4.1 Primary: ESP32-S3-WROOM-1-N16R8

**Why S3:**
- Dual-core Xtensa LX7 @ 240 MHz (faster than classic ESP32's LX6)
- 8 MB octal PSRAM (for spectrum buffers, BLE scan caches, OTA staging)
- Native USB OTG (drag-and-drop UF2 flashing!)
- USB-Serial/JTAG (no USB-UART bridge chip needed)
- BLE 5.0 (LE 2M PHY, extended advertising → better BLE spam throughput)
- 32 usable GPIO (more than classic ESP32's 34)
- GPIO 2 is **not** a strapping pin (CC1101 GDO0 is safe here)
- Lower deep-sleep current (8 µA vs 10 µA)

**Spec sheet:**

| Property | Value |
|---|---|
| Module | ESP32-S3-WROOM-1-N16R8 |
| Flash | 16 MB (octal SPI) |
| PSRAM | 8 MB (octal SPI) |
| GPIO usable | 32 (38 exposed, 6 used by octal flash/PSRAM) |
| WiFi | 802.11 b/g/n (2.4 GHz only) |
| Bluetooth | BLE 5.0 (no classic BT) |
| USB | USB OTG 1.1 + USB-Serial/JTAG |
| TX power (WiFi) | 20 dBm max |
| Antenna | IPEX/U.FL connector (on U variant) or PCB antenna |

### 4.2 Supported variants (build-flag gated)

| Variant | Build flag | Notes |
|---|---|---|
| ESP32-S3-WROOM-1-N16R8 (primary) | `BOARD_ESP32S3_N16R8` | Best for jammer |
| ESP32-S3-WROOM-1-N8R2 | `BOARD_ESP32S3_N8R2` | Less PSRAM but cheaper |
| ESP32-S3-WROOM-1-N8 (no PSRAM) | `BOARD_ESP32S3_N8` | Not recommended (out of memory) |
| ESP32-WROOM-32D (classic) | `BOARD_ESP32_CLASSIC` | For users who already have one |
| ESP32-C3-DevKitM-1 | `BOARD_ESP32C3` | RISC-V, single core, limited features |
| ESP32-S2-Saola-1 | `BOARD_ESP32S2` | USB OTG native, no BLE |

### 4.3 Radio modules

**Required:**

- **1× ESP32-S3 dev board** (DevKitC-1-N16R8 recommended)
- **1× nRF24L01+PA+LNA** with external antenna (mandatory for range)
- **1× CC1101** sub-GHz module (green PCB, 433 MHz antenna)
- **1× SSD1306 OLED** 128×64 I2C (optional but recommended)
- **3× tactile buttons** (optional, for OLED menu)
- **1× 100 µF / 16 V electrolytic capacitor** (for nRF24 power)
- **1× 100 µF / 16 V electrolytic capacitor** (for CC1101 power)
- **1× AMS1117-3.3** LDO (only needed if powered from 5 V directly)

**Optional:**

- **1× extra nRF24L01+PA+LNA** (for 4-channel parallel jam)
- **1× extra CC1101** (for parallel sub-GHz band coverage)
- **1× TP4056 + 3.7 V LiPo** (battery mod)
- **1× slide switch** (power on/off)
- **1× piezo buzzer** (audio feedback)
- **1× IPEX-to-SMA-F pigtail** (3rd antenna for ESP32)

**BOM cost (USD, AliExpress, 2026):**

| Item | Qty | Price ea | Total |
|---|---|---|---|
| ESP32-S3 DevKitC-1-N16R8 | 1 | $5.50 | $5.50 |
| nRF24L01+PA+LNA | 1 | $2.00 | $2.00 |
| CC1101 module | 1 | $1.50 | $1.50 |
| SSD1306 OLED 0.96" | 1 | $1.20 | $1.20 |
| Tactile buttons | 3 | $0.05 | $0.15 |
| 100 µF 16V caps | 2 | $0.03 | $0.06 |
| Jumper wires (F-F) | 20 | $0.01 | $0.20 |
| 3D printed case (optional) | 1 | $2.00 | $2.00 |
| **Total** | | | **$12.61** |

---

## 5. Feature Catalog

Every feature is categorized, scoped, and given a milestone. **M0 = v1.0.0 release, M1 = post-1.0, M2 = future.**

### 5.1 nRF24 (2.4 GHz) attacks

| ID | Feature | M | Description |
|---|---|---|---|
| F-N01 | Bluetooth Classic jam | M0 | Sweep AFH hop set (21 channels) + random + brute |
| F-N02 | BLE advertising jam | M0 | nRF24 on ch 2/26/80 with NOACK packets |
| F-N03 | BLE data channel jam | M0 | nRF24 on ch 2..80 step 2 with CW |
| F-N04 | Drone jam (RC 2.4 GHz) | M0 | nRF24 sweep 0..125 |
| F-N05 | WiFi jam (nRF24 brute) | M0 | nRF24 on overlapping ch for each WiFi ch (1-14) |
| F-N06 | Zigbee jam | M0 | nRF24 on overlapping ch for each Zigbee ch (11-26) |
| F-N07 | MouseJack inject | M1 | Inject keystrokes to vulnerable nRF24 keyboards |
| F-N08 | Channel analyzer | M0 | Live RSSI on all 126 channels, waterfall on OLED |
| F-N09 | Multi-radio parallel | M0 | 1-5 nRF24s sweeping in lockstep (`i = ch % count`) |
| F-N10 | Sweep method select | M0 | List / Random / Brute (per-target) |
| F-N11 | Sweep direction | M0 | Together (all on same ch) vs Separate (round-robin) |
| F-N12 | Custom range | M0 | User picks start/stop channel (Misc) |
| F-N13 | PA level config | M0 | 4 levels: MIN/LOW/HIGH/MAX |
| F-N14 | Data rate config | M0 | 250 kbps / 1 Mbps / 2 Mbps |

### 5.2 CC1101 (sub-GHz) attacks

| ID | Feature | M | Description |
|---|---|---|---|
| F-C01 | Spot jam | M0 | Single frequency, continuous OOK flood |
| F-C02 | Range jam | M0 | Sweep start..stop with configurable step |
| F-C03 | Hopper jam | M0 | Round-robin user-supplied frequency list |
| F-C04 | Keyfob jam | M0 | Pre-loaded common keyfob frequencies (9 bands) |
| F-C05 | Record RAW | M0 | Capture OOK/FSK bitstream to RAM buffer |
| F-C06 | Replay RAW | M0 | Re-transmit captured bitstream on same frequency |
| F-C07 | Save to LittleFS | M1 | Persist recordings as `.sub` files (Flipper compat) |
| F-C08 | Load from LittleFS | M1 | Replay recordings from filesystem |
| F-C09 | Download recording | M1 | `/download?file=X` endpoint |
| F-C10 | Auto-tune sampling | M1 | Adapt sampling rate to bit-rate heuristics |
| F-C11 | Spectrum scanner | M2 | Sweep band, show RSSI waterfall |
| F-C12 | RollJam | M2 | Two-stage attack (record first unlock, jam second) |

### 5.3 WiFi (ESP32 built-in) attacks

| ID | Feature | M | Description |
|---|---|---|---|
| F-W01 | Deauth (targeted) | **M0 ✅** | Deauth specific AP/client by BSSID |
| F-W02 | Deauth (all) | **M0 ✅** | Sweep channels 1/6/11, deauth everything (broadcast) |
| F-W03 | Deauth (smart) | **M0 ✅** | Smart: rotates BSSID; spectrum-aware targeting M1 |
| F-W04 | Disassoc flood | **M0 ✅** | Similar to deauth, different frame subtype |
| F-W05 | Beacon flood | **M0 ✅** | Random SSIDs; rotates MAC; 11/12/13 supported |
| F-W06 | Probe request | **M0 ✅** | Inject probe requests with random SSIDs |
| F-W07 | Evil twin | M1 | Clone target AP, capture handshakes |
| F-W08 | PMKID capture | M1 | Handshake-less WPA2 attack (client-less) |
| F-W09 | Handshake capture | M1 | Standard 4-way WPA2 handshake (deauth + capture) |
| F-W10 | WiFi scan (passive) | M0 | List APs by channel with SSID/encryption/RSSI |
| F-W11 | Hidden SSID reveal | M0 | Force client probes to expose hidden APs |
| F-W12 | WiFi jam (raw) | M0 | Continuous noise on specific channel (limited by 10 fps) |

### 5.4 BLE (NimBLE 2.x) attacks

| ID | Feature | M | Description |
|---|---|---|---|
| F-B01 | BLE scan | **M0 ✅** | List nearby BLE devices with RSSI/name/manufacturer |
| F-B02 | BLE advertising jam (nRF24) | **M0 ✅** | Same as F-N02 (uses nRF24 not NimBLE) |
| F-B03 | Apple Continuity spam | **M0 ✅** | Popup "AirPods / Apple TV / Nearby" on iPhones |
| F-B04 | Samsung Buds spam | **M0 ✅** | Popup "Galaxy Buds" on Samsung phones |
| F-B05 | Windows Swift Pair spam | **M0 ✅** | Popup "Connect to [device]" on Windows 10/11 |
| F-B06 | Fast Pair spam | **M0 ✅** | Google Fast Pair (Pixel, Android 13+) |
| F-B07 | BLE link-layer attack | M2 | L2CAP flood, ATT request storm |
| F-B08 | BLE MITM | M2 | Active MITM via GATT interception |

### 5.5 UI / UX

| ID | Feature | M | Description |
|---|---|---|---|
| F-U01 | Web UI (responsive) | M0 | Modern HTML5 + CSS3 + ES6, dark theme, mobile-friendly |
| F-U02 | Web spectrum view | M0 | Live RSSI bar chart + waterfall in browser |
| F-U03 | OLED menu (1-button) | M0 | Short press = next, long press = select |
| F-U04 | OLED menu (2-button) | M0 | Next + OK |
| F-U05 | OLED menu (3-button) | M0 | Next + Prev + OK |
| F-U06 | Serial CLI | M0 | Full command parser, help, tab-completion |
| F-U07 | WebSerial (in-browser) | M0 | Embedded xterm.js terminal in web UI |
| F-U08 | OTA via web | M0 | Upload .bin, verify, flash, reboot |
| F-U09 | Drag-and-drop UF2 | M1 | ESP32-S3 native USB MSC for "drop firmware.bin" UX |
| F-U10 | Captive portal | M0 | Auto-redirect to 192.168.4.1 on AP join |
| F-U11 | Multi-language | M2 | i18n: English (default), Indonesian, others via JSON |

### 5.6 Persistence & Settings

| ID | Feature | M | Description |
|---|---|---|---|
| F-S01 | Settings in NVS | M0 | All config persisted via `Preferences` |
| F-S02 | SSID list (100 entries) | M0 | For beacon spam / evil twin |
| F-S03 | Recordings in LittleFS | M1 | `.sub` files, named by timestamp |
| F-S04 | Backup/restore config | M1 | Export all NVS to JSON, import to restore |
| F-S05 | Default config reset | M0 | `/reset` endpoint, one-click factory reset |
| F-S06 | Watchdog settings | M2 | Per-module timeout configuration |

### 5.7 Hardware integration

| ID | Feature | M | Description |
|---|---|---|---|
| F-H01 | Multi-nRF24 (1-5) | M0 | User-configurable pin assignment via web |
| F-H02 | Multi-CC1101 (1-2) | M1 | Parallel sub-GHz coverage |
| F-H03 | Battery monitor | M1 | ADC read + voltage divider, web/OLED display |
| F-H04 | Power button | M1 | Long-press to safely power off (deep sleep) |
| F-H05 | Status LED | M0 | RGB LED with mode-color mapping |
| F-H06 | Buzzer | M2 | Audio feedback for events |
| F-H07 | Wake-on-button | M2 | Deep sleep + GPIO interrupt wake |

### 5.8 Diagnostic & Development

| ID | Feature | M | Description |
|---|---|---|---|
| F-D01 | Heap monitor | M0 | `info` command shows free/used heap, fragmentation |
| F-D02 | Task monitor | M0 | FreeRTOS task list with stack high-water marks |
| F-D03 | NVS dump | M0 | `dump_nvs` command shows all settings |
| F-D04 | Performance log | M1 | Channel sweep rate (channels/sec) for tuning |
| F-D05 | Crash dump | M1 | Save panic trace to LittleFS, expose via web |
| F-D06 | Profiler hooks | M2 | `perf <module>` to measure ms per loop |

### 5.9 Out of scope (explicitly)

- ❌ Cellular (GSM/LTE/5G/NB-IoT) jamming — wrong hardware
- ❌ GPS jamming — illegal almost everywhere, wrong frequency
- ❌ RFID/NFC (13.56 MHz) — needs different hardware (PN532)
- ❌ Permanent damage (e.g. firmware bricks) — ethical line
- ❌ Drone takeover (control hijack) — different attack class

---

## 6. Software Architecture

### 6.1 Module hierarchy

```
src/
├── main.cpp                       # setup() + loop()
├── core/                          # PhantomRF core
│   ├── PhantomRF.h / .cpp         # top-level namespace
│   ├── Module.h                   # IModule interface
│   ├── ModuleRegistry.h / .cpp   # register/lookup
│   ├── StateMachine.h / .cpp     # main state
│   ├── EventBus.h / .cpp         # cross-task messaging
│   ├── Config.h / .cpp            # build config
│   └── Version.h                  # auto-generated
├── hal/                           # Hardware Abstraction Layer
│   ├── Board.h / .cpp            # pin definitions per board
│   ├── SpiBus.h / .cpp            # dual-bus manager
│   ├── Gpio.h / .cpp              # input/output helpers
│   ├── Storage.h / .cpp           # NVS + LittleFS wrapper
│   ├── Display.h / .cpp           # OLED abstraction
│   ├── Buttons.h / .cpp           # debounced buttons
│   └── Led.h / .cpp               # status LED
├── radio/                         # Radio drivers
│   ├── Nrf24.h / .cpp             # nRF24L01 driver (upstream + extensions)
│   ├── Cc1101.h / .cpp            # CC1101 driver (LSatan V3 + extensions)
│   └── RfChannel.h                # common channel/freq tables
├── modules/                       # Feature modules
│   ├── Nrf24Jammer/
│   │   ├── Nrf24Jammer.h / .cpp
│   │   ├── JamBluetooth.cpp
│   │   ├── JamBle.cpp
│   │   ├── JamDrone.cpp
│   │   ├── JamWifi.cpp
│   │   ├── JamZigbee.cpp
│   │   └── JamMisc.cpp
│   ├── Cc1101Jammer/
│   │   ├── Cc1101Jammer.h / .cpp
│   │   ├── JamSpot.cpp
│   │   ├── JamRange.cpp
│   │   ├── JamHopper.cpp
│   │   ├── JamKeyfob.cpp
│   │   ├── RecordRaw.cpp
│   │   └── ReplayRaw.cpp
│   ├── WifiAttack/
│   │   ├── WifiAttack.h / .cpp
│   │   ├── Deauth.cpp
│   │   ├── Disassoc.cpp
│   │   ├── BeaconFlood.cpp
│   │   ├── ProbeFlood.cpp
│   │   ├── EvilTwin.cpp
│   │   ├── PmkidCapture.cpp
│   │   └── HandshakeCapture.cpp
│   ├── BleAttack/
│   │   ├── BleAttack.h / .cpp
│   │   ├── BleScan.cpp
│   │   ├── AppleSpam.cpp
│   │   ├── SamsungSpam.cpp
│   │   ├── WindowsSpam.cpp
│   │   └── FastPairSpam.cpp
│   ├── Spectrum/
│   │   ├── Spectrum.h / .cpp     # 2.4 GHz + sub-GHz spectrum
│   │   ├── Waterfall.cpp
│   │   └── RssiAnalyzer.cpp
│   └── Settings/
│       ├── Settings.h / .cpp
│       └── Defaults.h
├── ui/                            # User Interface
│   ├── web/                       # web interface
│   │   ├── WebServer.h / .cpp
│   │   ├── WebRoutes.h / .cpp
│   │   ├── OtaHandler.h / .cpp
│   │   └── api/                   # REST API
│   │       ├── StatusApi.h / .cpp
│   │       ├── AttackApi.h / .cpp
│   │       ├── SettingsApi.h / .cpp
│   │       └── FilesApi.h / .cpp
│   ├── oled/                      # OLED menu
│   │   ├── OledMenu.h / .cpp
│   │   ├── MenuRenderer.h / .cpp
│   │   └── screens/               # per-feature screens
│   └── cli/                       # Serial CLI
│       ├── Console.h / .cpp
│       ├── CommandParser.h / .cpp
│       └── commands/              # per-module commands
├── tasks/                         # FreeRTOS tasks
│   ├── JammerTask.h / .cpp       # base class
│   ├── Nrf24Task.cpp
│   ├── Cc1101Task.cpp
│   └── SpectrumTask.cpp
└── utils/                         # utilities
    ├── ChannelMath.h / .cpp      # freq↔channel conversions
    ├── FrameBuilder.h / .cpp     # 802.11 frame construction
    ├── BleAdvBuilder.h / .cpp    # BLE advertisement builder
    ├── RingBuffer.h / .cpp       # lock-free ring buffer
    ├── Logger.h / .cpp           # serial + web logger
    └── Random.h                   # seeded RNG
```

### 6.2 Namespace

All code lives in `phm` namespace (PhantomRF) to avoid name collisions with library code:

```cpp
namespace phm {
    class PhantomRF;
    class ModuleRegistry;
    namespace hal { class Board; class SpiBus; }
    namespace radio { class Nrf24; class Cc1101; }
    namespace ui { class WebServer; class OledMenu; class Console; }
    namespace util { class FrameBuilder; }
}
```

### 6.3 Build configurations (PlatformIO environments)

```ini
; Primary environments
[env:esp32s3-n16r8]            ; Full-featured, 16 MB flash, 8 MB PSRAM
[env:esp32s3-n8]                ; 8 MB flash, no PSRAM
[env:esp32-classic]             ; Classic ESP32 users
[env:esp32c3]                   ; RISC-V, single core
[env:esp32s2]                   ; USB OTG native, no BLE
[env:flipper-zero]              ; Companion port

; Variant environments (sub-targets)
[env:esp32s3-n16r8-minimal]     ; Just BT+BLE jam, no WiFi/BLE attacks
[env:esp32s3-n16r8-nrf24-only]  ; No CC1101 support
[env:esp32s3-n16r8-cc1101-only] ; No nRF24 support
[env:esp32s3-n16r8-2.4ghz-only] ; Only 2.4 GHz, no sub-GHz
[env:esp32s3-n16r8-subghz-only] ; Only sub-GHz, no 2.4 GHz
[env:esp32s3-n16r8-no-web]      ; Headless (no web UI), serial only
[env:esp32s3-n16r8-no-oled]     ; No OLED support
```

### 6.4 Header conventions

- All public headers use `#pragma once`
- All C++ files use `.h` / `.cpp` extension (no `.ino`)
- C-style files use `.c` / `.h` (only for the Flipper port)
- All headers are guarded by `phm/<subdir>/<Name>.h` include style

---

## 7. Directory Structure

### 7.1 Repo layout

```
PhantomRF/
├── DESIGN.md                      # this file
├── README.md                      # user-facing entry point
├── LICENSE                        # MIT
├── CONTRIBUTING.md                # how to contribute
├── CODE_OF_CONDUCT.md             # community guidelines
├── CHANGELOG.md                   # release notes
├── SECURITY.md                    # vulnerability disclosure
│
├── platformio.ini                 # PlatformIO build config
├── partitions/                    # custom partition tables
│   ├── default_16mb.csv
│   ├── default_8mb.csv
│   └── minimal.csv
│
├── src/                           # firmware source
│   ├── main.cpp
│   ├── core/
│   ├── hal/
│   ├── radio/
│   ├── modules/
│   ├── ui/
│   ├── tasks/
│   └── utils/
│
├── web/                           # web UI assets (built into image)
│   ├── index.html                 # main page
│   ├── css/                       # stylesheets
│   ├── js/                        # javascript
│   ├── img/                       # images (icons, logos)
│   └── manifest.json
│
├── flasher/                       # web flasher (GitHub Pages)
│   ├── index.html                 # the web flasher page
│   ├── manifest_<variant>.json    # per-build manifests
│   └── README.md
│
├── docs/                          # documentation site
│   ├── index.md                   # mkdocs / docusaurus home
│   ├── getting-started/
│   │   ├── installation.md
│   │   ├── wiring.md
│   │   ├── first-flash.md
│   │   └── first-use.md
│   ├── hardware/
│   │   ├── esp32-s3.md
│   │   ├── nrf24l01.md
│   │   ├── cc1101.md
│   │   ├── pcb-design.md
│   │   └── 3d-case.md
│   ├── features/
│   │   ├── bluetooth-jam.md
│   │   ├── ble-spam.md
│   │   ├── wifi-deauth.md
│   │   ├── subghz-jam.md
│   │   ├── record-replay.md
│   │   ├── spectrum-analyzer.md
│   │   └── mousejack.md
│   ├── algorithms/
│   │   ├── nrf24-const-carrier.md
│   │   ├── cc1101-async-serial.md
│   │   ├── 802.11-frames.md
│   │   ├── ble-advertising.md
│   │   ├── channel-math.md
│   │   └── sweep-strategies.md
│   ├── reference/
│   │   ├── pinout.md
│   │   ├── commands.md
│   │   ├── settings.md
│   │   ├── api.md
│   │   └── troubleshooting.md
│   └── legal/
│       ├── disclaimer.md
│       ├── jurisdictions.md
│       └── ethics.md
│
├── tools/                         # dev tools
│   ├── record-viewer/             # desktop tool to view .sub files
│   ├── bin-builder/               # post-build script (combine bins, compute hashes)
│   ├── ci/                        # CI scripts
│   └── lint/
│
├── scripts/                       # utility scripts
│   ├── setup.sh                   # dev environment
│   ├── build.sh                   # cross-platform build
│   ├── release.sh                 # cut a release
│   └── flash.sh                   # local flash helper
│
├── tests/                         # unit tests (PlatformIO native)
│   ├── test_nrf24.cpp
│   ├── test_cc1101.cpp
│   ├── test_frame_builder.cpp
│   ├── test_channel_math.cpp
│   └── test_settings.cpp
│
└── .github/                        # GitHub-specific
    ├── workflows/
    │   ├── build.yml              # CI: build all envs
    │   ├── test.yml               # CI: run unit tests
    │   ├── lint.yml               # CI: clang-format, clang-tidy
    │   └── release.yml            # cut a release
    ├── ISSUE_TEMPLATE/
    └── PULL_REQUEST_TEMPLATE.md
```

### 7.2 Generated artifacts (not in git, in .gitignore)

```
.pio/                              # PlatformIO build cache
build/                             # CI build output
dist/                              # release artifacts (binaries + JSON manifests)
node_modules/                      # web flasher deps
.cache/
*.bin
*.elf
*.hex
*.uf2
```

---

## 8. Pinout Specification

### 8.1 ESP32-S3-WROOM-1-N16R8 primary

```
┌─────────────────────────────────────────────────────────────────────┐
│                       ESP32-S3-WROOM-1-N16R8                       │
│                                                                     │
│   ┌─────────────┐                          ┌─────────────┐         │
│   │  nRF24L01   │                          │   CC1101    │         │
│   │  (HSPI)     │                          │   (VSPI)    │         │
│   └──────┬──────┘                          └──────┬──────┘         │
│          │                                        │                │
│   SCK   ─► GPIO 12 ──┐                  SCK  ◄─  GPIO 36            │
│   MISO  ◄─ GPIO 13   │  HSPI shared     MISO ─►  GPIO 37            │
│   MOSI  ─► GPIO 11   │                  MOSI ◄─  GPIO 38            │
│   CSN_1 ─► GPIO 15  │                  CSN  ◄─  GPIO 39            │
│   CE_1  ─► GPIO 16  │                  GDO0 ─►  GPIO 2  (★)        │
│   CSN_2 ─► GPIO 17  │                  GDO2 ─►  GPIO 4             │
│   CE_2  ─► GPIO 18  │                                            │
│   CSN_3 ─► GPIO 21  │                                            │
│   CE_3  ─► GPIO 35  │                  ┌─────────────┐             │
│   CSN_4 ─► GPIO 36  │                  │   SSD1306   │             │
│   CE_4  ─► GPIO 37  │                  │   (I2C)     │             │
│   CSN_5 ─► GPIO 38  │                  └──────┬──────┘             │
│   CE_5  ─► GPIO 39  │                         │                   │
│          │                                  SDA ─►  GPIO 8         │
│          │                                  SCL ─►  GPIO 9         │
│          │                                                         │
│          │     ┌──────────────┐                                     │
│          │     │   Buttons    │    ┌──────────────┐                 │
│          │     │  (active-LO) │    │  Status LED  │                 │
│          │     └──────┬───────┘    └──────┬───────┘                 │
│          │            │                   │                         │
│          │       OK  ─► GPIO 10       LED_R ◄─ GPIO 5              │
│          │       NEXT ─► GPIO 14       LED_G ◄─ GPIO 6              │
│          │       PREV ─► GPIO 38       LED_B ◄─ GPIO 7              │
│          │                                                         │
│          │     ┌──────────────┐    ┌──────────────┐                 │
│          │     │   Battery    │    │     USB     │                 │
│          │     │   Monitor    │    │  (native)   │                 │
│          │     └──────┬───────┘    └──────┬───────┘                 │
│          │       V_BAT ─► GPIO 1 (ADC)   D- ─► GPIO 19              │
│          │                                D+ ─► GPIO 20              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

★ GPIO 2 is the GDO0 pin for CC1101 (safe on S3, not a strapping pin!)
```

**Pin justification table:**

| Pin | Function | Why this pin? |
|---|---|---|
| GPIO 2 | CC1101 GDO0 | Safe on S3, supports interrupt (bit-bang async serial) |
| GPIO 4 | CC1101 GDO2 | Free, no conflicts |
| GPIO 8 | OLED SDA | S3 I2C default (works out of the box) |
| GPIO 9 | OLED SCL | S3 I2C default |
| GPIO 10 | Button OK | Safe, supports deep-sleep wake |
| GPIO 11 | nRF24 MOSI | S3 SPI2 IO_MUX default (fastest path) |
| GPIO 12 | nRF24 SCK | S3 SPI2 IO_MUX default |
| GPIO 13 | nRF24 MISO | S3 SPI2 IO_MUX default |
| GPIO 14 | Button NEXT | Safe, supports wake |
| GPIO 15 | nRF24 CSN #1 | Safe, output-only |
| GPIO 16 | nRF24 CE #1 | Safe, output-only |
| GPIO 17 | nRF24 CSN #2 | Safe |
| GPIO 18 | nRF24 CE #2 | Safe (avoids USB strapping) |
| GPIO 19 | USB D− | Native USB |
| GPIO 20 | USB D+ | Native USB |
| GPIO 21 | nRF24 CSN #3 | Safe |
| GPIO 35 | nRF24 CE #3 | Safe, no strapping function |
| GPIO 36 | nRF24 CSN #4 | Safe |
| GPIO 37 | nRF24 CE #4 | Safe, supports wake |
| GPIO 38 | nRF24 CSN #5 | Safe |
| GPIO 39 | nRF24 CE #5 | Safe |
| GPIO 36 | CC1101 SCK | Safe (nRF24 CSN/CE remapped when CC1101 present) |
| GPIO 37 | CC1101 MISO | Safe |
| GPIO 38 | CC1101 MOSI | Safe |
| GPIO 39 | CC1101 CSN | Safe |
| GPIO 1 | Battery ADC | ADC1_CH0, works with WiFi active |
| GPIO 5/6/7 | RGB LED | Safe, supports PWM for color mixing |

**AVOID:**
- GPIO 0, 3, 45, 46 (strapping pins)
- GPIO 26-32 (N16R8 octal PSRAM/flash)
- GPIO 43, 44 (UART0 default)

### 8.2 ESP32 classic fallback (for users who already have one)

```
nRF24 (HSPI): SCK=14, MISO=12, MOSI=13, CSN=15, CE=16
nRF24 (VSPI): CSN=17, CE=18  (or VSPI shared bus 18/19/23/5)
CC1101:       SCK=18 (shared with nRF24 #2 SCK!), MISO=19, MOSI=23, CSN=5, GDO0=2, GDO2=4
OLED:         SDA=21, SCL=22
Buttons:      OK=25, NEXT=26, PREV=27
LED:          GPIO 2
```

⚠️ **Note:** GPIO 2 is a strapping pin on classic ESP32. Pulling it LOW at boot may put the chip in download mode. The CC1101_jammer pulls it HIGH at boot, so it should be safe, but we have a fallback: use GPIO 27 for CC1101 GDO0 if GPIO 2 is problematic.

### 8.3 ESP32-C3 fallback (RISC-V, single core, limited features)

Limited to nRF24 only (no CC1101, no BLE spam, smaller web UI):
- nRF24: SCK=4, MISO=5, MOSI=6, CSN=7, CE=10
- OLED: SDA=8, SCL=9
- Button: GPIO 3

### 8.4 Flipper Zero

(Covered in a separate `phantom-fz` repo, but algorithmically identical.)

---

## 9. Build System

### 9.1 PlatformIO (primary)

```ini
; platformio.ini — head of file

[platformio]
default_envs = esp32s3-n16r8
src_dir = src
data_dir = web
extra_scripts = scripts/post_build.py

; Shared configuration
[env]
platform = espressif32 @ ^6.5.0
framework = arduino
monitor_speed = 115200
monitor_filters = colorize, time, default

; ============================
; Primary ESP32-S3 environment
; ============================
[env:esp32s3-n16r8]
board = esp32-s3-devkitc-1-n16r8
board_build.partitions = partitions/default_16mb.csv
board_build.f_flash = 80000000L
board_build.flash_mode = qio
board_build.arduino.memory_type = qio_opi
build_flags = 
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MSC_ON_BOOT=0
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_ARCH_ESP32
    -DPHM_BOARD=PHM_BOARD_ESP32S3_N16R8
    -DPHM_VERSION=\"1.0.0\"
    -I src
lib_deps = 
    nrf24/RF24 @ ^1.6.1
    lsatan/SmartRC-CC1101-Driver-Lib @ ^3.0.2
    h2zero/NimBLE-Arduino @ ^2.5.0
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.9
    gyverlibs/GyverButton @ ^3.7
    bblanchon/ArduinoJson @ ^7.0.0
    me-no-dev/AsyncTCP @ ^3.1.4
    esphome/ESPAsyncWebServer @ ^3.0.0

; Build artifacts
[env:esp32s3-n16r8]
build_type = debug   ; or release for production
```

### 9.2 Arduino IDE (secondary)

Steps to use Arduino IDE 2.x:

1. Install `esp32` board package v3.3.10+ via Board Manager
2. Select board `ESP32S3 Dev Module`
3. Configure: USB CDC On Boot = Enabled, Flash Size = 16 MB, PSRAM = OPI PSRAM, Partition Scheme = Huge APP
4. Open `src/main.cpp` and all files in `src/` (Arduino IDE requires a top-level .ino; we provide one in `src/main.cpp` for this purpose)
5. Install required libraries via Library Manager
6. Compile and upload

### 9.3 Web flasher (GitHub Pages)

The `flasher/` folder is a static site that uses `esp-web-tools@^9.0` to flash PhantomRF from any modern browser over WebSerial.

**Workflow:**

1. User visits `https://<user>.github.io/PhantomRF/`
2. Selects their board variant from a dropdown
3. Clicks "INSTALL PHANTOMRF"
4. Browser opens WebSerial picker
5. User selects the ESP32-S3 USB serial port
6. `esp-web-tools` streams `firmware.bin` to flash
7. Device reboots into PhantomRF

**Drag-and-drop UF2 (S3 only):**

When the device is in "bootloader" mode (hold BOOT, press RESET, release RESET, release BOOT), it appears as a USB Mass Storage device. The user can drag `firmware.uf2` onto it for instant flashing — no tool, no driver, no install. This works because of the S3's native USB OTG.

### 9.4 Command-line build (CI)

```bash
# Install PlatformIO
pip install platformio

# Build all environments
pio run

# Build a specific environment
pio run -e esp32s3-n16r8

# Build + upload
pio run -e esp32s3-n16r8 -t upload

# Build + upload + monitor
pio run -e esp32s3-n16r8 -t upload && pio device monitor

# Build release artifacts (bin, uf2, manifests)
./scripts/build.sh release
```

Output:
```
dist/
├── PhantomRF-esp32s3-n16r8-v1.0.0.bin
├── PhantomRF-esp32s3-n16r8-v1.0.0.uf2
├── PhantomRF-esp32s3-n16r8-v1.0.0.bootloader.bin
├── PhantomRF-esp32s3-n16r8-v1.0.0.partitions.bin
├── manifest.json
└── PhantomRF-esp32s3-n16r8-v1.0.0.sha256
```

---

## 10. Algorithm Reference

This is the technical heart of the project. Every attack has a dedicated section explaining **what it does**, **why it works**, and **how it's implemented**.

### 10.1 nRF24 constant carrier (the foundation of 2.4 GHz jamming)

**What:** Force the nRF24L01 chip to transmit an unmodulated continuous-wave (CW) carrier on a chosen channel.

**Why it works:** Any receiver on (or near) that channel sees the carrier as a constant high signal, drowning out any legitimate 802.15.4 / BLE / WiFi / proprietary 2.4 GHz signal. The receiver's automatic gain control (AGC) reduces sensitivity, causing packet loss.

**How:**

The nRF24L01 has a "test mode" in which the `RF_SETUP` register is rewritten to set two bits:

- `CONT_WAVE` (bit 7): forces the synthesizer to keep the PLL locked at the channel frequency
- `PLL_LOCK` (bit 4): keeps the PLL in a constant-output state

The procedure is:

```cpp
// 1. Initialize the nRF24 normally
radio.begin();
radio.setAutoAck(false);
radio.setPALevel(RF24_PA_MAX);
radio.setDataRate(RF24_2MBPS);
radio.setCRCLength(RF24_CRC_DISABLED);

// 2. Stop listening, set channel
radio.stopListening();
radio.setChannel(channel);

// 3. Push a 32-byte dummy payload (required for the radio to enter TX)
uint8_t dummy[32];
memset(dummy, 0xFF, 32);
radio.write(dummy, 32);

// 4. Read RF_SETUP, set CONT_WAVE and PLL_LOCK, write back
uint8_t setup = SPI_read(RF_SETUP);
setup |= 0x80 | 0x10;  // CONT_WAVE | PLL_LOCK
SPI_write(RF_SETUP, setup);

// 5. Pulse CE high to start transmission
digitalWrite(CE_PIN, HIGH);
```

Now the nRF24 emits a constant unmodulated carrier. To change channel, write a new value to `RF_CH` (the channel register). To stop, clear `CONT_WAVE` and `PLL_LOCK`, pulse CE low.

**Channel math:** nRF24L01 channel-to-frequency is `f_MHz = 2400 + channel` for `channel ∈ [0, 125]`. So channel 0 = 2.400 GHz, channel 76 = 2.476 GHz, channel 125 = 2.525 GHz.

**Limitations:**
- The nRF24L01 can only emit a carrier on **one** channel at a time. To "jam" multiple channels, you must sweep.
- The carrier is **unmodulated** — it works against narrowband receivers (nRF24, BLE, Zigbee) but less well against wideband receivers (WiFi, which uses 20 MHz channels).
- The nRF24L01's PLL has a finite lock time (~120 µs); you cannot switch channels faster than ~5000 channels/sec.

### 10.2 CC1101 async-serial mode (the foundation of sub-GHz)

**What:** Bypass the CC1101's packet handler so the raw demodulated bitstream comes out of GDO0 (in RX) or the CC1101 transmits whatever you bit-bang on GDO0 (in TX).

**Why it works:** OOK/ASK keyfobs (the most common 433 MHz devices) use simple on-off encoding. To record a keyfob's transmission, you need to sample the demodulated waveform at the bit-rate; to replay, you need to drive the same bitstream back into the CC1101. The CC1101's normal "packet" mode requires preamble, sync word, CRC, etc. — overhead that prevents raw capture. Async-serial mode is the bypass.

**How:**

```cpp
// 1. Switch CC1101 to async-serial mode
radio.setCCMode(0);          // 0 = async-serial, 1 = normal packet
radio.setPktFormat(3);       // 3 = asynchronous serial mode
radio.setLengthConfig(0);    // ignore length byte

// 2. For RECORD: enter RX, sample GDO0 at known interval
SetRx();
pinMode(GDO0, INPUT);
for (i = 0; i < BUFFER_SIZE; i++) {
    byte b = 0;
    for (j = 7; j >= 0; j--) {
        bitWrite(b, j, digitalRead(GDO0));
        delayMicroseconds(sampling_us);
    }
    buffer[i] = b;
}

// 3. For REPLAY: enter TX, bit-bang GDO0
SetTx();
pinMode(GDO0, OUTPUT);
for (i = 0; i < BUFFER_SIZE; i++) {
    for (j = 7; j >= 0; j--) {
        digitalWrite(GDO0, bitRead(buffer[i], j));
        delayMicroseconds(sampling_us);
    }
}
```

**Frequency configuration:** The CC1101 is set to the target frequency via the `FREQ2`/`FREQ1`/`FREQ0` registers. The math is:

```
FREQ = (freq_MHz * 65536) / 26  (with 26 MHz crystal)
```

For 433.92 MHz: `FREQ = (433.92 * 65536) / 26 = 1093856` = `0x10B0E0` → `FREQ2=0x10, FREQ1=0xB0, FREQ0=0xE0`.

The CC1101 also requires a **PA table** per band:
- 300-348 MHz: 8-entry table
- 387-464 MHz: 8-entry table
- 779-899.99 MHz: 10-entry table
- 900-928 MHz: 10-entry table

Bands 348-378 and 464-779 are **unsupported** (the CC1101's PA cannot hit them efficiently).

**Calibration:** Every frequency change requires a PLL re-calibration via `Calibrate()`. This takes ~1 ms per call, limiting the sweep rate.

### 10.3 802.11 deauthentication frame

**What:** Send a forged 802.11 management frame (type 00, subtype 12) to disconnect a client from an AP.

**Why it works:** 802.11 management frames are unauthenticated. An attacker can forge a deauth frame with the AP's MAC as source and the client's MAC as destination; the client (and AP) accept it and tear down the connection.

**Frame structure:**

```
Bytes  | Field
-------|--------------------
0-1    | Frame Control (0xC0, 0x00) = Management, Deauth
2-3    | Duration (0x0000)
4-9    | Destination MAC (target client)
10-15  | Source MAC (AP)
16-21  | BSSID (AP)
22-23  | Sequence Control (0xF0, 0xFF)
24-25  | Reason Code (0x0001 = Unspecified, 0x0004 = Disassociated due to inactivity, 0x0007 = Class 3 frame received from nonassociated STA)
```

**How to send on ESP32-S3:**

The ESP-IDF provides `esp_wifi_80211_tx()` which bypasses the WiFi stack and transmits a raw 802.11 frame. The function signature:

```cpp
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);
```

For deauth:
- `ifx` = `WIFI_IF_STA` (we use the STA interface even in AP mode)
- `buffer` = the 26-byte deauth frame
- `len` = 26
- `en_sys_seq` = `false` (we control the sequence number)

**Limitations:**
- ESP32-S3 firmware limits raw 802.11 TX to ~10 frames/second (hardware/firmware throttling)
- Modern 802.11w (PMF) clients reject deauth frames, but PMF is rarely enabled in practice
- Both AP and client must be in range

**Promiscuous mode trick:** The most effective deauth is to **first sniff legitimate frames from the target AP**, then **craft deauth using the exact MAC addresses observed**. This avoids hard-coding MACs and works for all clients associated with that AP.

```cpp
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t *mac = pkt->payload + 4;  // skip radiotap

    // Check that this is from an AP (Address2 == Address3)
    if (memcmp(mac, mac+12, 6) == 0 && memcmp(mac, "\xFF\xFF\xFF\xFF\xFF\xFF", 6)) {
        memcpy(deauth.sta, mac+6, 6);  // client
        memcpy(deauth.ap,  mac,   6);  // AP
        memcpy(deauth.src, mac,   6);  // source

        digitalWrite(2, HIGH);  // boost power
        delay(15);
        esp_wifi_80211_tx(WIFI_IF_STA, &deauth, sizeof(deauth), false);
        digitalWrite(2, LOW);
    }
}
```

### 10.4 BLE advertising spam (the popup attack)

**What:** Transmit many BLE advertisements with specific manufacturer data that triggers popups on nearby devices.

**Why it works:** Apple's "Continuity" protocol, Samsung's "Galaxy Buds" pairing protocol, Google's "Fast Pair", and Windows' "Swift Pair" all use BLE advertisements to announce nearby devices. By spoofing these advertisements with random MACs and known manufacturer data, we make the target's device show a popup as if a real device is nearby.

**How (Apple Continuity example):**

```cpp
// Apple Continuity "Nearby Action" advertisement
uint8_t payload[] = {
    0x02, 0x01, 0x06,                           // Flags
    0x1A, 0xFF,                                 // Manufacturer Specific, length 26
    0x4C, 0x00,                                 // Apple, Inc. (0x004C)
    0x07, 0x19,                                 // Nearby Action
    0x07,                                       // Action type: AirPods Pro
    // ... random auth tag, etc.
    0x00, 0x00, 0x00, 0x00,                     // random padding
};

NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
adv->setAdvertisementData(BLE_HCI_LE_SET_ADV_DATA_NONCONN_IND, 
                          std::string((char*)payload, sizeof(payload)));
adv->setMinInterval(0x20);  // 20 ms
adv->setMaxInterval(0x40);  // 40 ms
adv->start();

// Then periodically change the random fields and restart
```

**Why it works on iOS:** iOS continuously scans for these advertisements in the background. When it sees a "real-looking" one, it shows a popup. The popup is a UX feature, not a security boundary — and the protocol has no authentication.

**S3 advantage:** ESP32-S3 supports BLE 5.0 with LE 2M PHY → 2× throughput → 2× more popups per second.

### 10.5 Channel sweep strategies

**Sweeping the 2.4 GHz band is the core of any 2.4 GHz jammer.** The naive approach (one nRF24, sequential) is too slow. PhantomRF implements four strategies:

| Strategy | Time per full sweep | Coverage | Use case |
|---|---|---|---|
| `SweepTogether` | ~1.3 ms × N | All radios on same ch | Strong localized burst |
| `SweepSeparate` | ~1.3 ms × N / count | Round-robin `i = ch % count` | Maximum coverage |
| `SweepRandom` | unbounded | Random per loop | Avoids detection of pattern |
| `SweepList` | 1 cycle per channel in list | User-supplied | Targeted at known hops |

**Throughput math:**

- nRF24L01 SPI command (`W_REGISTER | RF_CH`) takes 2 SPI bytes = ~2 µs at 1 MHz SPI
- PLL re-lock takes ~120 µs
- Effective channel switch rate: 1 / 122 µs = ~8200 ch/s theoretical, ~5000 ch/s in practice (overhead)
- Full 80-channel Bluetooth sweep: 80 / 5000 = 16 ms
- Full 126-channel ISM sweep: 126 / 5000 = 25 ms

With 4 nRF24s in `SweepSeparate` mode, throughput is 4× = 20,000 ch/s effective, so 126 channels in 6.3 ms. This is "as good as you can get" with the nRF24L01 hardware.

### 10.6 LittleFS for recordings

**Why not SPIFFS?** SPIFFS is deprecated in arduino-esp32 v3.x. LittleFS is the replacement.

**Why not just RAM?** The W0rthlessS0ul CC1101_jammer keeps recordings in a 32 KB RAM buffer — lost on reboot. We use LittleFS for persistence.

**Format:** Each recording is a `.sub` file (Flipper-compatible format). The `.sub` format is:

```
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: RAW
RAW_Data: 2000 500 -1000 500 ...
```

This means a recording is plain text and can be analyzed on a PC, converted to other formats, etc.

**Capacity:** 8 MB LittleFS = ~2000 recordings of 4 KB each (typical keyfob length).

---

## 11. Web Interface Design

### 11.1 Page structure

The web UI is a **single-page application** (SPA) with a sidebar for navigation and a main panel for content. Pages:

| Page | URL | Purpose |
|---|---|---|
| Dashboard | `/` | Status, current module, recent activity |
| Bluetooth | `/ble` | BT classic + BLE jam controls |
| WiFi | `/wifi` | Deauth, beacon flood, scan, capture |
| 2.4 GHz | `/2.4ghz` | nRF24 spectrum, drone jam, custom sweep |
| Sub-GHz | `/subghz` | CC1101 spot/range/hopper/keyfob, record/replay |
| Spectrum | `/spectrum` | Live RSSI waterfall for both bands |
| Settings | `/settings` | Pin config, PA level, AP credentials, NVS reset |
| Terminal | `/terminal` | WebSerial xterm.js embedded |
| Files | `/files` | Browse LittleFS, upload/download recordings |
| OTA | `/ota` | Firmware update (also accepts UF2 drag-and-drop) |
| Help | `/help` | Built-in help, links to docs |

### 11.2 API endpoints (REST-style)

| Method | URL | Body | Response |
|---|---|---|---|
| `GET` | `/api/status` | - | `{ "current_module": "bt", "running": true, "free_heap": 123456, "uptime_ms": 60000 }` |
| `POST` | `/api/attack/start` | `{ "module": "bt", "method": "list" }` | `{ "ok": true }` |
| `POST` | `/api/attack/stop` | - | `{ "ok": true }` |
| `GET` | `/api/spectrum` | - | `{ "rssi": [-90, -85, -88, ...], "channels": [0, 1, 2, ...] }` |
| `GET` | `/api/settings` | - | `{ "nrf24_count": 1, "nrf24_ce_pins": [16], ... }` |
| `POST` | `/api/settings` | `{ "nrf24_count": 2, ... }` | `{ "ok": true }` |
| `GET` | `/api/files` | - | `[{ "name": "recording.sub", "size": 4096 }, ...]` |
| `GET` | `/api/files/recording.sub` | - | `.sub` file content |
| `DELETE` | `/api/files/recording.sub` | - | `{ "ok": true }` |
| `POST` | `/api/replay` | `{ "file": "recording.sub", "frequency": 433.92 }` | `{ "ok": true }` |
| `POST` | `/api/reset` | - | `{ "ok": true }` (reboots after 1s) |
| `POST` | `/api/ota` | multipart/form-data with .bin | `{ "ok": true, "size": 1234567 }` |

All endpoints return JSON, all errors are `{ "error": "<msg>" }` with appropriate HTTP status code.

### 11.3 Real-time updates (WebSocket)

For real-time spectrum and attack status, the web UI uses a **WebSocket** connection:

```
Client → Server:  WS upgrade at /ws
Server → Client:  {"type":"spectrum","rssi":[...]}
Server → Client:  {"type":"attack","state":"running","module":"bt"}
Server → Client:  {"type":"log","level":"info","msg":"Jamming started"}
```

Implementation: `WebSocketServer` from `ESPAsyncWebServer`.

### 11.4 Visual design principles

- **Dark theme by default** (less battery drain on mobile)
- **No animations that aren't functional** (the W0rthlessS0ul "glitch text" effect is fun but distracting)
- **Mobile-first responsive** (most users will control from a phone)
- **Color-coded by attack type** (red for WiFi attacks, blue for BLE, green for sub-GHz, yellow for spectrum)
- **Big touch targets** (minimum 44×44 px for buttons)
- **No external CDNs** (everything served locally for offline use)

### 11.5 Technology stack

- **HTML5** with semantic markup
- **CSS3** with custom properties, no framework (no Bootstrap, no Tailwind)
- **Vanilla JavaScript (ES6 modules)**, no React/Vue/Svelte (saves 500 KB+)
- **Chart.js** for spectrum visualization (small, MIT, well-known)
- **xterm.js** for the embedded terminal
- All assets ≤ 100 KB total, served from LittleFS

---

## 12. OLED / Serial / Button Subsystem

### 12.1 OLED display

- **Driver:** Adafruit_SSD1306 with Adafruit_GFX
- **Sizes:** 128×32 (small) and 128×64 (full), selected at build time
- **Layout:** bitmap icons for top-level menu, text labels for submenus
- **Animation:** 60 Hz refresh via `millis()`-based scheduler, not `delay()`
- **Themes:** light/dark (just inverts)

### 12.2 Menu navigation

The menu uses a **state machine** with three button configurations:

**1-button (constrained):**
- Short press → next menu item
- Long press → select
- In submenu: short press cycles options, double press confirms

**2-button (medium):**
- Next button → next item
- OK button → select
- In submenu: Next cycles, OK confirms

**3-button (full):**
- Prev button → previous item
- Next button → next item
- OK button → select
- In submenu: Prev/Next cycle, OK confirms

**Per-menu-item actions:**

For multi-step inputs (e.g., setting custom channel range in Misc):
- `SET_START` state: button increments value
- `SET_STOP` state: button increments value
- `IDLE` state: button confirms to start attack

The state machine is implemented in `src/ui/oled/MenuRenderer.cpp`. It supports:
- Nested menus (up to 4 levels)
- Live updating (e.g., spectrum during jam)
- Custom render functions per menu item
- "Back" gesture (long press on Prev, or specific button combo)

### 12.3 Serial CLI

**Wire format:** 115200 baud, 8N1. Newline-terminated commands.

**Command structure:**

```
phantom> help

Available commands:
  info              - device info (version, free heap, uptime)
  status            - current state (which module, which attack)
  attack            - start/stop attacks
    attack bt       - Bluetooth jam
    attack ble      - BLE jam
    attack wifi     - WiFi deauth/attack
    attack subghz   - Sub-GHz attack
    attack drone    - Drone jam
    attack spec     - Spectrum analysis
  scan              - scan networks/devices
    scan wifi       - WiFi APs
    scan ble        - BLE devices
  record            - record sub-GHz signal
  replay            - replay recorded signal
  settings          - show/configure settings
    settings get    - show all
    settings set nrf24_pa 0
  stop              - stop current attack
  reboot            - reboot device
  help              - this help
  ?                 - alias for help

phantom> attack bt
Starting Bluetooth jam (method=list)...
Channels: 32 34 46 48 50 52 0 1 2 4 6 8 22 24 26 28 30 74 76 78 80
Running. Press Ctrl+C or send 'stop' to abort.

phantom> stop
Stopping...
```

**Parser:** Token-based with quoted strings. `CommandParser` class in `src/ui/cli/`.

**Autocomplete:** Tab-completion for command names (uses history buffer in RAM).

---

## 13. Persistence Model

### 13.1 NVS (Non-Volatile Storage) for settings

| Key | Type | Default | Description |
|---|---|---|---|
| `nrf24.count` | int8 | 1 | Number of nRF24 modules |
| `nrf24.ce_pins` | bytes | [16,17,18,21,35] | CE pin per module |
| `nrf24.csn_pins` | bytes | [15,17,18,21,36] | CSN pin per module |
| `nrf24.pa` | int8 | 0 | PA level (0=MIN, 3=MAX) |
| `nrf24.data_rate` | int8 | 2 | 0=250k, 1=1M, 2=2M |
| `cc1101.present` | bool | true | CC1101 module detected |
| `cc1101.sampling_us` | int16 | 150 | Record sampling interval |
| `cc1101.payload` | int8 | 60 | TX payload size |
| `ble.method` | int8 | 0 | BLE jam method (advertising/data) |
| `wifi.jam_method` | int8 | 0 | 0=channel, 1=smart, 2=all |
| `wifi.deauth_method` | int8 | 0 | 0=channel, 1=all, 2=smart |
| `spectrum.window_ms` | int16 | 100 | Spectrum refresh window |
| `ap.ssid` | str | "PhantomRF" | AP SSID |
| `ap.password` | str | "phantom1234" | AP password |
| `ap.enabled` | bool | true | AP on/off |
| `oled.enabled` | bool | true | OLED on/off |
| `oled.brightness` | int8 | 255 | OLED contrast |
| `buttons.config` | int8 | 2 | 0/1/2 button config |
| `sweep.method` | int8 | 0 | 0=together, 1=separate |
| `ssid.list` | bytes | default | Up to 100 SSIDs for beacon spam |
| `battery.calibration` | int16 | 1200 | ADC-to-mV factor |

### 13.2 LittleFS for files

- **Web UI assets** (HTML/CSS/JS) — 1.5 MB typical
- **Custom icons** (uploaded by user) — variable
- **Recordings** (`.sub` files) — variable
- **Backups** (JSON dumps) — variable
- **Logs** (rolling, last 100 KB) — auto-rotated

**Default directory structure:**

```
/littlefs/
├── web/                    # web UI assets (read-only, from data_dir)
│   ├── index.html
│   ├── css/
│   ├── js/
│   └── img/
├── recordings/
│   ├── 2026-06-19_12-34-56_433.92.sub
│   └── ...
├── backups/
│   ├── settings-2026-06-19.json
│   └── ...
└── logs/
    └── current.log
```

### 13.3 Backup/restore

```cpp
// Export
String json = Settings::exportAll();
File f = LittleFS.open("/backups/settings-2026-06-19.json", "w");
f.print(json);
f.close();

// Import
String json = ...;  // read from file
Settings::importAll(json);
```

The web UI exposes this as a "Backup / Restore" button in the Settings page.

---

## 14. Power & Battery

### 14.1 Power tree

```
USB-C 5V ──► AMS1117-3.3 ──► 3.3V rail
                                  │
       ┌──────────────────────────┼──────────────────────────┐
       │                          │                          │
   ESP32-S3                   nRF24L01                    CC1101
   (240 mA peak)              (115 mA TX)                 (30 mA TX)
                                   │                          │
                              100 µF cap                100 µF cap
                              (decoupling)              (decoupling)
```

**Peak current:** ~385 mA worst case (all three active simultaneously). USB 2.0 provides 500 mA, so a powered USB port is sufficient.

### 14.2 Battery mod (optional)

For portable use:

```
3.7V LiPo (3000 mAh) ──► TP4056 (charge mgmt) ──► slide switch ──► 3.3V LDO ──► ESP32-S3
                                                    │
                                                    └─► (optional) boost to 5V for AMS1117
```

**Battery life estimates (3000 mAh, all attacks active):**

| Scenario | Avg current | Battery life |
|---|---|---|
| Idle (AP up, no attack) | 80 mA | ~37 hours |
| BT jam (continuous) | 280 mA | ~10 hours |
| WiFi deauth (10 fps) | 220 mA | ~13 hours |
| Sub-GHz jam | 250 mA | ~12 hours |
| All attacks | 350 mA | ~8.5 hours |

### 14.3 Brownout protection

ESP32-S3 has configurable brownout detection. For a battery-powered jammer that draws significant peak current, we:

1. Set brownout threshold to 2.8 V (instead of default 3.0 V) via `CONFIG_ESP_BROWNOUT_DET_LVL_SEL_7`
2. Add a 1000 µF bulk capacitor on the 3.3 V rail
3. Use `digitalWrite(2, HIGH)` to boost PA power only when actually transmitting (W0rthlessS0ul trick)

### 14.4 Power-saving modes

When the device is idle and on battery:

- **Light sleep** between attacks (WiFi association maintained, ~25 mA)
- **Deep sleep** when no user activity for 5 minutes (~8 µA, wake on button press)
- **Modem sleep** during WebSocket idle (WiFi stack sleeps, ~30 mA)

The user can configure this in Settings → Power.

---

## 15. Performance Budget

### 15.1 CPU budget (per task, on Core 0)

| Task | CPU % | Notes |
|---|---|---|
| WiFi stack | 20% | OS-managed |
| Web server (idle) | 2% | Wait for request |
| Web server (active) | 15% | Serving page |
| nRF24 jammer worker (1 radio) | 50% | Channel hopping tight loop |
| nRF24 jammer worker (4 radios) | 75% | Higher SPI contention |
| CC1101 jammer worker | 30% | Bit-banging |
| BLE spam | 60% | High throughput |
| Spectrum scan | 10% | Sweep once per second |

**Free CPU at idle:** ~78% (enough for spectrum scan + CLI + display)

### 15.2 Memory budget (N16R8: 8 MB PSRAM, 512 KB SRAM)

| Use | SRAM | PSRAM |
|---|---|---|
| Stack (2 cores × 8 KB) | 16 KB | - |
| FreeRTOS queues / mutexes | 4 KB | - |
| WiFi/BLE stack | 80 KB | - |
| nRF24 driver state | 1 KB | - |
| CC1101 driver state | 1 KB | - |
| NVS / Preferences cache | 4 KB | - |
| HTTP request buffer | 8 KB | - |
| **App overhead** | **~110 KB** | - |
| **Available for app** | **~400 KB** | **~7.5 MB** |

**PSRAM allocations:**
- Web UI asset cache: 200 KB
- Spectrum history buffer: 256 KB (100 frames × 2560 bytes)
- Recording buffer (for 32 KB recordings): 32 KB per recording
- BLE scan result cache: 64 KB (1000 devices × 64 bytes)

### 15.3 Flash budget (16 MB)

| Use | Size |
|---|---|
| Bootloader | 32 KB |
| Partition table | 4 KB |
| NVS | 24 KB |
| OTA staging | 3 MB |
| Active firmware | 1.5 MB |
| LittleFS (web + recordings) | 10 MB |
| Free | ~1.4 MB |

### 15.4 Latency targets

| Operation | Target | Actual |
|---|---|---|
| Boot to AP ready | < 3 s | ~2.5 s |
| Web page load | < 1 s | ~400 ms |
| Attack start (web click → TX) | < 100 ms | ~50 ms |
| Attack stop | < 50 ms | ~20 ms |
| Spectrum refresh | 1 Hz | 1 Hz |
| CLI command response | < 10 ms | < 5 ms |
| OTA flash | < 60 s | ~30 s (1.5 MB firmware) |

---

## 16. Safety, Legal, Ethics

### 16.1 Legal disclaimer

This project is a **research and educational tool**. Operating RF jammers is **illegal in most jurisdictions** without explicit authorization. The project maintainers are not responsible for any misuse.

The README and web UI include prominent warnings. The first boot shows a "I understand the legal implications" screen that the user must dismiss.

### 16.2 Jurisdictions

| Country | Status | Notes |
|---|---|---|
| USA | Illegal (FCC §333) | $100K+ fine per violation |
| EU | Illegal (CEPT/ETSI) | Varies by country |
| UK | Illegal (Wireless Telegraphy Act 2006) | Up to 2 years prison |
| Germany | Illegal (TKG) | Very strict |
| Japan | Illegal (Radio Law) | 1 year prison |
| Indonesia | Illegal (UU Telekomunikasi) | Fines and prison |
| Brazil | Illegal (Anatel) | 2-4 years prison |
| Australia | Illegal (RadComms Act) | A$165K+ fines |

**The project maintains a `legal/jurisdictions.md` file with detailed info per country.** Users are responsible for knowing their local laws.

### 16.3 Ethical use cases

- **Authorized penetration testing** (with written permission from the target)
- **Academic research** (in a controlled RF-shielded environment)
- **Personal learning** (with your own devices)
- **Smart home security audits** (with homeowner permission)
- **Conference demonstrations** (in a legally approved venue)

### 16.4 Anti-feature list

The project will **NOT** add:

- ❌ GPS jamming
- ❌ Cell phone service denial (4G/5G)
- ❌ Public safety band jamming (aviation, emergency services)
- ❌ Military band jamming
- ❌ Drone hijacking (control override)
- ❌ Permanent device bricking

### 16.5 Safety features

- **First-boot warning screen** (OLED + web)
- **Confirmation required** for jamming (single-click start, no)
- **Maximum TX time** (configurable, default 10 min auto-stop)
- **Visual LED** always indicates active attack
- **Buzzer** (optional) plays a short beep on attack start

---

## 17. Testing Strategy

### 17.1 Unit tests (PlatformIO native)

```cpp
// tests/test_channel_math.cpp
#include <unity.h>
#include "ChannelMath.h"

void test_nrf24_channel_to_freq() {
    TEST_ASSERT_EQUAL(2400, phm::util::nrf24ChannelToFreq(0));
    TEST_ASSERT_EQUAL(2402, phm::util::nrf24ChannelToFreq(2));
    TEST_ASSERT_EQUAL(2525, phm::util::nrf24ChannelToFreq(125));
}

void test_cc1101_freq_to_register() {
    uint32_t freq_reg = phm::util::cc1101FreqToReg(433.92);
    // Expected: 0x10B0E0 (per datasheet formula)
    TEST_ASSERT_EQUAL_HEX(0x10B0E0, freq_reg);
}

void test_wifi_channel_to_nrf24_range() {
    auto range = phm::util::wifiChannelToNrf24Range(6);
    // WiFi ch 6 = 2437 MHz, 22 MHz wide → nRF24 ch 26-48
    TEST_ASSERT_EQUAL(26, range.start);
    TEST_ASSERT_EQUAL(48, range.stop);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_nrf24_channel_to_freq);
    RUN_TEST(test_cc1101_freq_to_register);
    RUN_TEST(test_wifi_channel_to_nrf24_range);
    UNITY_END();
}
void loop() {}
```

### 17.2 Integration tests (hardware)

- nRF24 transmit on a known channel → verify with spectrum analyzer or HackRF
- CC1101 transmit on 433 MHz → verify with RTL-SDR
- ESP32-S3 deauth injection → verify client disconnects
- BLE advertising → verify with nRF Connect app
- OTA update → verify firmware boots correctly after update
- Web UI → verify all endpoints respond correctly
- Battery monitor → verify ADC reads match multimeter
- Brownout detection → verify with variable power supply

### 17.3 Continuous integration

GitHub Actions runs on every PR:

```yaml
name: CI
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.11' }
      - name: Install PlatformIO
        run: pip install platformio
      - name: Build all environments
        run: pio run
      - name: Run unit tests
        run: pio test -e native
      - name: Lint with clang-format
        run: ./tools/lint/run-clang-format.sh
      - name: Lint with clang-tidy
        run: ./tools/lint/run-clang-tidy.sh
      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware-bin
          path: dist/*.bin
```

### 17.4 Hardware-in-loop test rig

For CI, we maintain a small rig of ESP32-S3 dev boards + nRF24 + CC1101 that runs the full firmware in a Faraday cage and validates that:
- All attacks are within frequency tolerance (±100 kHz)
- Power output is within safe limits
- No spurious emissions outside expected bands

This rig is in the maintainer's home lab; CI doesn't have hardware access. Manual runs only.

---

## 18. Documentation Plan

### 18.1 User-facing docs (in `/docs`)

**Tone:** Friendly, educational, assume the reader knows nothing about RF but is technically capable.

**Required pages (M0):**

1. **Introduction** — what PhantomRF is, who it's for, what it can do
2. **Installation** — board selection, library install, first flash
3. **Wiring** — detailed wiring diagrams, photo references
4. **First use** — connect to AP, see dashboard, run a test
5. **Feature: Bluetooth jam** — what, why, how to use
6. **Feature: WiFi deauth** — what, why, how to use
7. **Feature: Sub-GHz jam** — what, why, how to use
8. **Feature: Spectrum analyzer** — what, why, how to use
9. **Algorithms: nRF24 const carrier** — deep dive
10. **Algorithms: CC1101 async-serial** — deep dive
11. **Algorithms: 802.11 deauth** — deep dive
12. **Algorithms: BLE spam** — deep dive
13. **Pinout** — per-board reference
14. **CLI reference** — all commands
15. **API reference** — for developers
16. **Troubleshooting** — common issues
17. **Legal** — jurisdictions, disclaimer

**Optional pages (M1/M2):**

18. MouseJack deep dive
19. PMKID capture deep dive
20. Evil twin deep dive
21. RollJam deep dive
22. Custom PCB design
23. 3D case design
24. Contributing guide
25. Code style guide

### 18.2 In-code documentation

Every public function has a docstring:

```cpp
/**
 * @brief Set the nRF24 module to continuous carrier (jam) mode
 *
 * Sets the CONT_WAVE and PLL_LOCK bits in RF_SETUP register, primes the
 * TX FIFO with a 32-byte payload, and pulses CE high. The radio will
 * emit an unmodulated carrier at the current channel frequency.
 *
 * @param radio Reference to initialized Nrf24 instance
 * @param channel 0-125, frequency = 2400 + channel MHz
 * @return true on success, false on SPI error
 *
 * @see Nrf24::stopConstCarrier to exit jam mode
 * @see Nrf24::setChannel to change frequency without leaving jam mode
 */
bool Nrf24::startConstCarrier(uint8_t channel);
```

Doxygen generates HTML from these. Hosted on GitHub Pages.

### 18.3 Tutorial videos (optional, M1)

- 5-min "Build your first PhantomRF" (wiring, flash, test)
- 10-min "Use the web UI" (dashboard, attack, spectrum)
- 10-min "Understand the algorithms" (const carrier, async-serial, etc.)
- 5-min "Legal and ethical use"

---

## 19. Roadmap & Milestones

### M0: v1.0.0 (estimated 3-4 months from start)

**Goal:** Stable, well-documented, working release that supersedes all five inspiration projects.

**Features (subset of §5):**
- F-N01, F-N02, F-N03, F-N04, F-N05, F-N06, F-N08, F-N09, F-N10, F-N11, F-N12, F-N13, F-N14
- F-C01, F-C02, F-C03, F-C04, F-C05, F-C06
- F-W01, F-W02, F-W03, F-W05, F-W10, F-W12
- F-B01, F-B02
- F-U01, F-U02, F-U03, F-U04, F-U05, F-U06, F-U07, F-U08, F-U10
- F-S01, F-S02, F-S05
- F-H01, F-H05
- F-D01, F-D02, F-D03

**Documentation:** all M0 pages in §18.1.

**Quality bars:**
- All code passes clang-format and clang-tidy
- All unit tests pass
- All build environments compile clean
- Web flasher works on Chrome, Firefox, Edge, Safari
- At least one external user has successfully built and flashed the device

### M1: v1.1.0 (+2-3 months)

- F-N07 (MouseJack)
- F-C07, F-C08, F-C09, F-C10
- F-W04, F-W06, F-W07, F-W08, F-W09, F-W11
- F-B03, F-B04, F-B05, F-B06
- F-U09 (drag-drop UF2)
- F-S03, F-S04
- F-H02, F-H03, F-H04
- F-D04, F-D05
- Custom PCB + 3D case reference designs

### M2: v1.2.0 (+6 months)

- F-C11, F-C12
- F-B07, F-B08
- F-U11 (i18n)
- F-D06
- F-H06, F-H07
- Flipper Zero companion port
- Custom keyboard/mouse support

### M3: v2.0.0 (future, +12 months)

- Multi-device mesh coordination (2+ PhantomRFs syncing)
- AI-driven attack selection (model picks best attack for target)
- LoRa / LoRaWAN support
- Zigbee coordinator mode
- Thread / Matter support
- ESP32-C6 (WiFi 6, Thread) support

---

## 20. Critical Engineering Mitigations

> **Section status:** NEW — added in response to design review feedback

This section addresses **five system-engineering risks** that were identified during the first design review. Each subsection states the problem, quantifies the impact, and provides concrete mitigations that will be implemented in the M0 codebase.

These are not nice-to-haves. They are **prerequisites** for a production-quality release. Skipping them would make PhantomRF:
- Unusable during 2.4 GHz attacks (self-jamming)
- Hijackable from 5 meters away (no auth)
- Bricking its own flash after ~6 months of heavy use (wear-out)
- Brownout-resetting on weak USB power supplies (peak current)
- CPU-starved when displaying the spectrum waterfall (wrong transport)

### 20.1 Mitigation: Self-Jamming & RF Co-existence

**The problem:**

The ESP32-S3's WiFi radio and the nRF24L01 modules both operate in the **2.4 GHz ISM band**. When a 2.4 GHz attack is active with PA-level `MAX` (0 dBm + external PA = +20 dBm effective), the user's phone/laptop will be **completely disconnected** from the PhantomRF AP. The web UI becomes unreachable, the WebSocket dies, and the only way to stop the attack is a power cycle.

**The "near-miss":** even at PA `LOW` (-12 dBm), 2.4 GHz CW jam reliably drops WiFi links within ~2 meters of the device. At PA `MAX`, the dead zone expands to ~10 meters.

**Failure modes (in order of severity):**

1. User starts a 2.4 GHz attack from the web UI → loses connection 200 ms later → no way to stop without power cycle
2. User tries to start a sub-GHz attack (CC1101, no WiFi conflict) but cannot tell the device from the web UI that the previous attack has stalled → user frustrated
3. User starts 5-radio parallel nRF24 sweep → even the USB CDC serial becomes unreliable due to SPI contention
4. User configures device, then a friend connects to the same AP and triggers an attack remotely

**Mitigation strategy: multi-channel control with intelligent degradation**

We provide **five independent control channels**, and PhantomRF automatically picks the best available one given the current state:

| Priority | Channel | Active when | Latency | Bandwidth |
|---|---|---|---|---|
| 1 | **USB CDC Serial** (USB-Serial/JTAG on GPIO 19/20) | USB cable connected | <10 ms | Full CLI |
| 2 | **WebSocket over WiFi AP** | WiFi AP up + WiFi not jammed | <50 ms | Full web UI + spectra |
| 3 | **BLE Serial (NimBLE GATT)** | 2.4 GHz jammed, no USB | <100 ms | CLI subset |
| 4 | **OLED + buttons** | Always (independent) | <20 ms (button) | Menu navigation only |
| 5 | **Station-mode WiFi (connect to home AP)** | Configured, not in 2.4 GHz attack | <100 ms | Full web UI |

**Auto-degradation rules:**

```
attack 2.4 GHz:
  - WebSocket (priority 2) → BLE Serial (priority 3) [if BLE not in use]
  - AP remains active but unreliable; web UI shows "control degraded" banner
  - USB CDC always works (priority 1, unaffected)

attack sub-GHz:
  - No degradation needed
  - Full web UI + USB CDC + BLE Serial all work

idle:
  - All channels available
  - User preference persisted (NVS key: phm.control.prefer)
```

**Implementation: `src/core/ControlChannelManager.cpp`**

```cpp
namespace phm::core {

enum class ChannelHealth {
    Ok,        // responsive
    Degraded,  // slow / partial
    Lost,      // unresponsive
};

class ControlChannelManager {
public:
    void registerChannel(ControlChannel* ch);
    void update();   // called every loop tick; updates health

    ChannelHealth health(ChannelId id);
    const char* bestAvailableChannel();  // for "send command X via channel Y"

    void onAttackStart(AttackType type);
    void onAttackStop();

private:
    std::array<ControlChannel*, 5> channels_;
    AttackType currentAttack_ = AttackType::None;
};

}  // namespace
```

**Web UI banner:**

When the WebSocket is in `Degraded` or `Lost` state, the web UI shows a sticky banner:

```
┌─────────────────────────────────────────────────┐
│  ⚠ Control channel degraded: 2.4 GHz jam active  │
│  Use USB Serial (115200 baud) or BLE Serial      │
│  to stop the attack.                             │
└─────────────────────────────────────────────────┘
```

**User notification before attack:**

The web UI requires a **double-confirmation** before starting any 2.4 GHz attack:

1. First click: "I understand this will disconnect me"
2. Second click: "Start attack anyway"

This pattern is familiar from the W0rthlessS0ul project (which simply cuts the connection without warning — bad UX).

**New feature ID:** F-CT01 (Control-channel selection), F-CT02 (Auto-degradation), F-CT03 (USB CDC CLI fallback), F-CT04 (BLE Serial CLI), F-CT05 (Station-mode WiFi), F-CT06 (Web UI banner). All M0.

---

### 20.2 Mitigation: Device Security

**The problem:**

The current design has **three remote attack surfaces** with no authentication:
- **Captive portal** (WiFi AP with default password `W0rthlessS0ul`)
- **Web UI** (no login)
- **OTA endpoint** (any HTTP POST with a .bin uploads firmware)

Threat model:

| Attacker | Range | Capability | Impact |
|---|---|---|---|
| Curious passerby | 10 m | Connects to AP, opens 192.168.4.1 | Stops attack, changes settings |
| Malicious local | 10 m | Same as above, plus changes AP password to lock out owner | Denial of service |
| Hostile network | 50 m | Sends deauth to disassociate user, then connects | Same as above |
| Targeted attacker | 1 m (USB) | Uploads malicious .bin via OTA | Brick device, persistent backdoor |

**Real-world incidents** (from W0rthlessS0ul GitHub issues): users report that a stranger in a public space joined their jammer's AP and "messed with the settings". This is the #1 reported UX issue with these jammers.

**Mitigation strategy: defense in depth**

#### Layer 1: AP password (mandatory strong default)

```cpp
// Generate per-device random password on first boot, print to serial
String apPassword = generateRandomPassword(16);  // 16-char, alphanumeric+symbols
prefs.putString("ap.password", apPassword);
Serial.printf("AP password: %s\n", apPassword.c_str());
```

The serial output is the **only** way to recover the password if forgotten. A "Reset WiFi" button on the device (hold BOOT 5 sec) regenerates the password.

**Feature ID:** F-SC01 (Per-device AP password), F-SC02 (Password reset via BOOT hold). M0.

#### Layer 2: Web UI authentication (HTTP Basic + CSRF token)

```cpp
// First page load: HTTP 401 with WWW-Authenticate
// Subsequent: HTTP Basic Auth header
// CSRF: every form has a hidden <input name="csrf" value="<random>">
// Token validated server-side on every POST

bool isAuthenticated() {
    return server.authenticate(apUser, prefs.getString("ap.password", "phantom").c_str());
}
```

**Rate limiting** on the login endpoint to prevent brute force:

```cpp
// Max 5 failed attempts per minute → 60-second lockout
// Tracked in NVS: phm.sc.failed_attempts, phm.sc.lockout_until
```

**Session timeout**: 30 minutes idle → auto-logout (re-prompt for password).

**Feature ID:** F-SC03 (HTTP Basic Auth), F-SC04 (CSRF token), F-SC05 (Rate limit), F-SC06 (Session timeout). M0.

#### Layer 3: Signed firmware (OTA + UF2)

```cpp
// Firmware signed with Ed25519 key
// Signature stored at end of firmware image (last 64 bytes)
// Bootloader verifies signature before booting

bool verifyFirmware(const uint8_t* image, size_t size) {
    const uint8_t* signature = image + size - 64;
    const uint8_t* payload = image;
    size_t payloadSize = size - 64;
    return ed25519_verify(public_key_, signature, payload, payloadSize);
}
```

**Key generation:**

- Public key hard-coded in firmware
- Private key held only by maintainers (in CI secrets, never committed)
- `firmware.bin` distributed with signature appended
- For UF2: signature in a separate `.sig` file in the same drop

**Feature ID:** F-SC07 (Signed firmware), F-SC08 (Ed25519 verification), F-SC09 (Per-release public key in NVS). M1 (deferred because key management is non-trivial; M0 ships unsigned with clear warning).

#### Layer 4: Network-layer hardening

```cpp
// Disable mDNS / SSDP / NetBIOS discovery (privacy)
// Block incoming ICMP except from local subnet
// Disable WiFi station-mode probing when not configured
// Reject HTTP requests with suspicious User-Agent strings

bool isAllowedOrigin(String origin) {
    return origin.startsWith("http://192.168.4.") || origin.isEmpty();
}
```

**Feature ID:** F-SC10 (Network hardening), F-SC11 (CORS allowlist). M0.

#### Layer 5: Physical security (button-gated admin)

```cpp
// Hold BOOT button for 5 seconds → enter "admin" mode
// Admin mode allows:
//   - OTA upload (normally disabled)
//   - Settings reset
//   - NVS dump
//   - Log download
// Without admin mode, OTA endpoint returns 403
```

**Visual feedback:** LED blinks red 3x to indicate admin mode entered.

**Feature ID:** F-SC12 (BOOT-held admin mode), F-SC13 (LED admin indicator). M0.

#### Security audit log

Every action of significance is logged to a ring buffer in NVS:

```cpp
struct AuditEntry {
    uint32_t timestamp;
    uint8_t  action;   // enum: Login, Logout, StartAttack, StopAttack, ChangeSettings, OTA, Reset
    uint8_t  result;   // 0=denied, 1=allowed
    char     user[16]; // "owner" or "remote"
};
```

The last 1000 entries are kept, downloadable via `/api/audit`.

**Feature ID:** F-SC14 (Audit log), F-SC15 (Audit download). M1.

---

### 20.3 Mitigation: Flash Wear Leveling

**The problem:**

ESP32-S3's internal flash has a typical endurance of **10,000-100,000 erase cycles per sector** (depending on chip grade; Espressif's modules typically use 10K-cycle flash). A "sector" is 4 KB. Recording a 32 KB signal involves **8 sector erases** and **32 KB of writes** — a "moderate" use.

**If we naively save every recording to LittleFS, here's the wear model:**

| Usage pattern | Recordings/day | Erase cycles/day | Years to 10K cycles |
|---|---|---|---|
| Light (1 recording/day) | 1 | 8 | 3.4 years |
| Moderate (10 recordings/day) | 10 | 80 | 0.34 years (4 months!) |
| Heavy (100 recordings/day) | 100 | 800 | 0.034 years (12 days!) |
| Continuous (5 min/rec) | 288 | 2304 | 0.012 years (4 days!) |

**Conclusion:** naive recording-to-flash will brick the device in 4 days of heavy use.

**Mitigation strategy: layered storage**

#### Layer 1: PSRAM ring buffer (default)

All recording goes to a **circular buffer in PSRAM** (8 MB available on N16R8). The buffer can hold ~250 recordings of 32 KB each.

```cpp
// psram/recording_buffer.h
namespace phm::storage {

constexpr size_t MAX_RECORDINGS = 256;
constexpr size_t RECORDING_SIZE = 32 * 1024;

struct RingBuffer {
    Recording recordings[MAX_RECORDINGS];
    size_t    writeIndex = 0;
    size_t    count = 0;
    void push(Recording&& r);
    Recording* at(size_t i);  // chronological order
};

}  // namespace
```

Recordings are **volatile** (lost on reboot) by default. The user must explicitly "Save to flash" each one, which is the costly operation.

**Feature ID:** F-FL01 (PSRAM ring buffer). M0.

#### Layer 2: Explicit save to LittleFS

When the user clicks "Save" in the web UI (or types `save` in CLI), the recording is written to LittleFS as a `.sub` file:

```
/littlefs/recordings/2026-06-19_12-34-56_433.92.sub
```

This is **one write per user action**, not continuous. The wear model now becomes:

| Usage | Saves/day | Erase cycles/day | Years to 10K |
|---|---|---|---|
| Light (1 save/day) | 1 | 8 | 3.4 years |
| Heavy (100 saves/day) | 100 | 800 | 0.034 years (12 days) |

Still risky if the user is saving aggressively. We add another layer.

**Feature ID:** F-FL02 (Explicit save). M0.

#### Layer 3: Wear-leveled log-structured storage

We use a **log-structured file system** approach: each `.sub` file is written to a fresh sector, and we **never overwrite**. Old recordings are deleted by a compaction job that runs at boot.

```cpp
// Write path
// 1. Find next free sector (from sector bitmap in NVS)
// 2. Write file to that sector
// 3. Update directory entry in NVS
// 4. If LittleFS < 10% free, run compaction
// Compaction: rewrite all live files to the lowest sectors, erase the rest

size_t findFreeSector();
void writeFile(const char* name, const uint8_t* data, size_t len);
void compact();
```

This spreads the wear across all 10K-cycle sectors evenly, so a heavy user gets effective endurance of 10K × (N_sectors / N_files_per_day) cycles.

For the N16R8 with 10 MB LittleFS and 32 KB recordings = 320 sectors used (with 1 file per sector). If the user saves 100 files/day, each sector is rewritten every 3.2 days → 10K cycles in 87 years (theoretically).

**Feature ID:** F-FL03 (Log-structured storage), F-FL04 (Auto-compaction). M1.

#### Layer 4: Flash health monitoring

```cpp
// src/utils/FlashHealth.h
namespace phm::storage {

struct FlashHealth {
    uint32_t totalErases;
    uint32_t erasesBySector[1024];  // N16R8 has ~2560 sectors total
    float    estimatedLifePercent;   // (erases / maxErases) × 100
};

// Logged to /api/health, displayed in web UI
// If estimatedLifePercent > 80%, show warning
// If > 95%, refuse new recordings (read-only mode)
}
```

The estimate is based on Espressif's `esp_flash_get_counters()` (which tracks some erase counts in eFuse) plus our own per-sector tracking.

**Feature ID:** F-FL05 (Health monitor), F-FL06 (Warning UI), F-FL07 (Read-only fallback). M1.

#### Layer 5: Configurable recording limits

```cpp
// Defaults: prevent catastrophic wear
constexpr Duration MAX_RECORDING_DURATION = 30s;  // per recording
constexpr size_t   MAX_RECORDINGS_TOTAL  = 100;   // before auto-purge
constexpr size_t   MAX_FS_USAGE_PERCENT   = 80;   // before auto-purge
```

When limits are exceeded, the **oldest** recording is auto-purged. Users can change the limits but the defaults are sane.

**Feature ID:** F-FL08 (Recording limits), F-FL09 (Auto-purge). M0.

---

### 20.4 Mitigation: Peak Power & Thermal Management

**The problem:**

The peak current draw of PhantomRF in worst-case operation is **startling**:

| Component | Idle | Active (typical) | Peak (TX burst) |
|---|---|---|---|
| ESP32-S3 (240 MHz, both cores, WiFi+BLE active) | 80 mA | 160 mA | 290 mA |
| nRF24L01+PA+LNA ×1 | 0.1 mA | 25 mA | 115 mA (TX @ 0 dBm) |
| nRF24L01+PA+LNA ×4 (parallel) | 0.4 mA | 100 mA | 460 mA |
| CC1101 (RX) | 15 mA | 18 mA | 20 mA |
| CC1101 (TX @ +12 dBm) | - | 30 mA | 80 mA |
| SSD1306 OLED | 2 mA | 10 mA | 20 mA |
| RGB LED | 0 mA | 5 mA | 30 mA |
| **Total, 4-radio, both radios TX, WiFi on** | **97 mA** | **323 mA** | **890 mA** |

**A USB 2.0 port provides 500 mA maximum.** A 1A phone charger typically supplies 1A continuous, peaks to 1.5A. The 890 mA peak is **right at the limit** of common supplies.

**Symptoms of under-voltage:**

1. ESP32-S3 brownout reset (3.0 V threshold by default)
2. nRF24L01 fails to lock PLL, refuses to transmit
3. CC1101 SPI errors, frequency miscalibration
4. Random crashes, watchdog resets
5. Boot loop

**Heat problem:**

The nRF24L01+PA+LNA module dissipates **~0.5 W in continuous TX**. The CC1101 at +12 dBm dissipates **~0.3 W**. Four nRF24s at once = 2 W from a tiny 16×30 mm module. Without thermal management, the module temperature can reach **80°C** (close to the 85°C spec limit).

**Mitigation strategy: hardware + firmware**

#### Hardware mitigations

1. **Bulk capacitor upgrade**: 1000 µF (not 100 µF) on the 3.3V rail, low-ESR ceramic

   ```
   3.3V ──┬── 1000 µF (low-ESR)
          ├── 100 nF (ceramic, decoupling)
          └── ...radios...
   ```

2. **AMS1117-3.3**: rated 1A continuous, with adequate PCB copper pour for heatsinking. Add a small heatsink if 4-radio mode is used.

3. **Heatsink on nRF24 modules**: 10×10 mm adhesive aluminum heatsink on each PA+LNA module. Reduces temp by ~15°C.

4. **Powered USB hub**: recommend user supplies via a powered hub if running 4-radio mode. Document this in setup guide.

5. **Optional active cooling**: 30 mm 5V fan, controlled by ESP32 GPIO. Triggered at 70°C. M2 feature.

#### Firmware mitigations

```cpp
// src/hal/PowerManager.cpp
namespace phm::hal {

class PowerManager {
public:
    void setup();
    void update();  // called every loop tick

    // Throttles attack intensity based on brownout/temperature history
    AttackProfile currentProfile();

private:
    void readBrownoutCount();   // via esp_reset_reason
    void readTemperature();     // via ESP32-S3 internal sensor
    void limitTxDutyCycle(uint8_t percent);  // 0-100
    void staggerRadioActivation();  // delay between radio TX starts
};

}
```

**Brownout detection tuning:**

```cpp
// In sdkconfig.defaults
CONFIG_ESP_BROWNOUT_DET_LVL_SEL_7=y   // 2.8V instead of 3.0V
CONFIG_ESP_BROWNOUT_DET_LVL=y
```

**Brownout recovery:**

```cpp
// On brownout reset:
// 1. Log to NVS: phm.brownout_count++
// 2. On next boot, read count
// 3. If > 3 brownouts in 1 hour, reduce TX duty cycle to 50%
// 4. If > 10 brownouts in 1 hour, refuse to start attacks
//    and show "Insufficient power" message
```

**Thermal throttling:**

```cpp
// ESP32-S3 has internal temperature sensor on ADC
// Read every 5 seconds

float temp = readInternalTemperature();

if (temp > 85.0) {
    // CRITICAL: emergency shutdown
    stopAllAttacks();
    enterDeepSleep();
    Serial.println("EMERGENCY: Over-temperature shutdown");
} else if (temp > 75.0) {
    // THROTTLE: reduce TX duty cycle
    setTxDutyCycle(50);
    showWarning("High temperature, throttling");
} else if (temp > 65.0) {
    // WARN
    setTxDutyCycle(80);
    showWarning("Elevated temperature");
}
```

**TX duty cycle staggering:**

When starting multi-radio attacks, stagger the radio activations by 10 ms to avoid peak current superposition:

```cpp
void startMultiRadioAttack() {
    for (int i = 0; i < nrf24_count; i++) {
        radios[i]->startConstCarrier(channels[i]);
        delay(10);  // 10 ms stagger
    }
}
```

**Feature ID:**
- F-PW01 (Brownout tuning), F-PW02 (Brownout recovery), F-PW03 (Temperature sensor), F-PW04 (Thermal throttle), F-PW05 (Emergency shutdown), F-PW06 (TX duty cycle stagger), F-PW07 (Power profile API), F-PW08 (Battery voltage API). M0 for M0 features (01-05, 07), M1 for M1 (06, 08).

**New section in docs:** `docs/hardware/power-budget.md` — full power budget table, recommended supplies, heatsink instructions.

---

### 20.5 Mitigation: Real-Time Web Protocol Specification

**The problem:**

The web UI displays:
- **Live spectrum waterfall** (RSSI for 126 channels × N radios × sub-GHz sweep)
- **Attack status** (running, channel, current target)
- **Log messages** (info, warning, error)
- **Settings changes**

Polling every 100 ms (10 Hz) means 10 HTTP requests per second. Each request costs:
- TCP handshake: ~3 ms (already open, but still TX/RX)
- HTTP parse: ~1 ms on ESP32-S3
- JSON serialize: ~0.5 ms
- WebServer send: ~1 ms
- Total: ~5.5 ms per request
- 10 Hz × 5.5 ms = **55 ms per second of CPU time** = 5.5% CPU

That's not terrible, but it scales badly. At 20 Hz (50 ms interval) it's 11% CPU. At 50 Hz, 27.5% CPU — enough to starve other tasks during a busy attack.

**For a 2.4 GHz waterfall at 10 fps** with 126 channels per radio, we need to push 1260 RSSI values per second, plus status, plus logs. Polling cannot scale to this.

**Decision matrix:**

| Protocol | Full-duplex | Push from server | Browser support | ESP32 CPU | Reconnection | Verdict |
|---|---|---|---|---|---|---|
| HTTP polling | No | No (client pulls) | Universal | High (5-30%) | Trivial | ❌ Wasteful |
| Server-Sent Events (SSE) | No | Yes | All modern | Low (one-way) | Automatic | ⚠️ Limited |
| **WebSocket** | **Yes** | **Yes** | **All modern** | **Low (one conn)** | **Manual but easy** | **✅ Best** |
| MQTT (external broker) | Yes | Yes | Requires broker | Low | Automatic | ❌ External dep |
| gRPC-Web | Yes | Yes | All modern | Medium | Complex | ❌ Overkill |

**Decision: WebSocket, with SSE fallback for restricted networks.**

**Why WebSocket over SSE:**

- **Bidirectional** — server can push spectrum data; client can push commands (e.g., "set channel to 42")
- **Lower per-frame overhead** — WebSocket frame: 2-6 bytes header + payload; SSE: 100+ bytes of HTTP-event formatting
- **Better browser support for binary frames** — we can send spectrum as raw bytes, not JSON
- **Industry standard** — every developer has used WebSocket

**Why SSE as fallback:**

- Some corporate networks block WebSocket but allow HTTP long-polling
- SSE works over plain HTTP/1.1, no upgrade dance
- For users behind such networks, SSE provides "good enough" real-time

**Protocol specification: PhantomRF WebSocket v1**

**Endpoint:** `ws://192.168.4.1/ws` (or `wss://` if HTTPS enabled)

**Connection lifecycle:**

1. Client opens WebSocket (HTTP 101 upgrade)
2. Server sends `{"type":"hello","version":1,"serverTime":1234567890}` as first frame
3. Client sends `{"type":"subscribe","topics":["spectrum","status","log"]}`
4. Server starts pushing frames for subscribed topics
5. Either side can `{"type":"ping"}` (server replies `{"type":"pong"}`)
6. On disconnect, client reconnects with exponential backoff (1s, 2s, 4s, 8s, max 30s)

**Message format (JSON, UTF-8):**

```json
{
  "type": "<message-type>",
  "seq": 1234,            // monotonic counter for ordering
  "ts": 1234567890,       // server timestamp (ms since boot)
  "data": { ... }         // type-specific payload
}
```

**Message types (server → client):**

| `type` | Rate | `data` shape | Size |
|---|---|---|---|
| `hello` | once | `{ version, serverTime, deviceId, capabilities }` | ~100 B |
| `spectrum` | 10 Hz | `{ band: "2.4ghz"\|"subghz", rssi: int[126], ts: int }` | 380 B |
| `status` | 1 Hz | `{ running, currentAttack, freeHeap, uptime }` | 80 B |
| `attack_state` | on event | `{ state, module, error? }` | 60 B |
| `log` | event-driven | `{ level, msg, source }` | 100 B avg |
| `pong` | on ping | `{ clientTs }` | 20 B |
| `error` | on error | `{ code, msg }` | 80 B |

**Message types (client → server):**

| `type` | Purpose | `data` shape |
|---|---|---|
| `subscribe` | Subscribe to topics | `{ topics: [str] }` |
| `unsubscribe` | Unsubscribe | `{ topics: [str] }` |
| `ping` | Keepalive | `{ clientTs }` |
| `command` | Run a command | `{ cmd: str, args: [str] }` |
| `ack` | Acknowledge attack_state | `{ seq }` |

**Bandwidth budget:**

- Spectrum at 10 Hz: 380 B × 10 = 3.8 KB/s
- Status at 1 Hz: 80 B/s
- Logs (avg 1/sec): 100 B/s
- Ping/pong overhead: 40 B × 2/sec = 80 B/s
- **Total: ~4 KB/s**, well within WiFi capacity

**Implementation:**

```cpp
// src/ui/web/WebSocketHandler.h
namespace phm::ui {

class WebSocketHandler {
public:
    void setup(AsyncWebServer& server);
    void broadcast(const JsonDocument& msg);
    void onMessage(WS消息回调);

private:
    AsyncWebSocket ws_;
    std::set<uint32_t> clientIds_;
    TopicSet subscriptions_;
    uint32_t seqCounter_ = 0;
    QueueHandle_t txQueue_;
    TaskHandle_t txTask_;
};

}  // namespace
```

**Server task pinning (RTOS):**

The WebSocket sender runs on Core 0 at low priority, draining a queue. Other tasks push to the queue:

```cpp
void sendSpectrum(const std::array<int8_t, 126>& rssi) {
    JsonDocument doc;
    doc["type"] = "spectrum";
    doc["seq"] = ++seqCounter_;
    doc["ts"] = millis();
    doc["data"]["band"] = "2.4ghz";
    doc["data"]["rssi"] = rssi;  // serialized as int array

    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    xQueueSend(txQueue_, buf, 0);  // non-blocking
}
```

**SSE fallback:**

Same data, but server sends as `text/event-stream`:

```
event: spectrum
data: {"type":"spectrum","seq":1234,...}

event: status
data: {"type":"status",...}
```

**Feature ID:**
- F-WS01 (WebSocket endpoint), F-WS02 (Topic subscription), F-WS03 (Spectrum push), F-WS04 (Status push), F-WS05 (Log push), F-WS06 (Bidirectional commands), F-WS07 (Auto-reconnect), F-WS08 (SSE fallback). All M0.

**New section in docs:** `docs/reference/websocket-protocol.md` — full protocol spec, message examples, JavaScript client library.

---

### 20.6 Mitigation Cross-Reference Matrix

The five mitigations interact. This matrix tracks the relationships:

| Mitigation | Depends on | Enables | Conflicts with |
|---|---|---|---|
| 20.1 RF Co-existence | 20.2 (auth for control channels) | 20.5 (WebSocket over BLE Serial) | 20.4 (more radios = more power) |
| 20.2 Device Security | 20.3 (signed firmware) | 20.1 (control channels auth) | 20.4 (TX power for crypto?) |
| 20.3 Flash Wear | 20.4 (no power loss during write) | — | 20.5 (more writes for logs) |
| 20.4 Power & Thermal | 20.1 (know which radios are active) | All others (stable platform) | 20.2 (rate-limit network) |
| 20.5 Web Protocol | 20.1 (which channel to use) | 20.2 (secure transport) | 20.4 (CPU cost) |

**Architectural decision:** mitigations are implemented in dependency order: **20.4 → 20.2 → 20.3 → 20.1 → 20.5**. The platform must be stable (20.4) before anything else can be relied upon.

**Feature ID prefix scheme:**

- F-CT**xx**: Control-channel (20.1)
- F-SC**xx**: Security (20.2)
- F-FL**xx**: Flash storage (20.3)
- F-PW**xx**: Power (20.4)
- F-WS**xx**: WebSocket (20.5)

These are added to the §5 feature catalog and the §19 M0 milestone.

### 20.7 M0 Scope: Simplified Implementation

The full mitigation design above is the **complete vision**. For the M0 (v1.0.0) release, we ship a **pragmatic subset** to avoid over-engineering:

| Mitigation | M0 ships | M1+ adds |
|---|---|---|
| 20.1 Co-existence | USB CDC Serial always works; web UI shows "control degraded" banner when 2.4 GHz attack active; auto-disable AP during 5-radio parallel attacks | BLE Serial fallback, station-mode WiFi, intelligent channel selection |
| 20.2 Security | Per-device random 16-char AP password; BOOT 5s = admin mode for OTA; network hardening (CORS, mDNS off) | HTTP Basic Auth, CSRF tokens, rate limit, signed firmware, audit log |
| 20.3 Flash wear | **PSRAM ring buffer (volatile); implemented as `phm::hal::RingBuffer<T>`**; explicit "Save" button per recording; 30s/100rec/80% limits; auto-purge oldest | Log-structured FS, sector health monitor, read-only fallback |
| 20.4 Power/thermal | Brownout threshold 2.8V; internal temp sensor; emergency shutdown at 85°C; throttle warning at 75°C | TX duty cycle stagger, current sensing, brownout counter with adaptive throttle |
| 20.5 Web protocol | **WebSocket push (status @ 1 Hz, spectrum @ 2 Hz, log events on demand)**; HTTP/1.1 keep-alive; HTTP polling also available | SSE fallback; binary frames; topic subscription; CSRF tokens |

**Rationale:** M0 should be a working, well-documented release. Over-engineering M0 with M1 features delays the release and complicates testing. The full §20.1-20.6 design is the **roadmap**; M0 is the **first concrete step**.

---

## 21. Glossary

| Term | Definition |
|---|---|
| **2.4 GHz ISM** | Industrial/Scientific/Medical radio band, 2.400-2.500 GHz, license-free |
| **AFH** | Adaptive Frequency Hopping (Bluetooth) |
| **AP** | Access Point (WiFi) |
| **BSSID** | Basic Service Set ID (WiFi AP MAC address) |
| **CC1101** | Texas Instruments sub-GHz transceiver chip, 300-928 MHz |
| **CW** | Continuous Wave (unmodulated carrier) |
| **Deauth** | 802.11 deauthentication frame |
| **DSN** | Distributed Spectrum Notification (Bluetooth mesh) |
| **ESP-IDF** | Espressif's official IoT Development Framework for ESP32 |
| **Fast Pair** | Google's BLE-based quick pairing for Android |
| **GDO0/GDO2** | General Digital Output pins of CC1101 |
| **HSPI/VSPI** | Hardware SPI bus 2/3 on ESP32 (legacy naming) |
| **IEEE 802.11** | WiFi standard family |
| **IEEE 802.15.4** | Low-rate wireless personal area networks (Zigbee, Thread) |
| **LDO** | Low-Dropout (voltage regulator) |
| **LE 2M PHY** | BLE 5.0 2 Mbps physical layer |
| **MouseJack** | Samy Kamkar's 2016 attack injecting keystrokes via nRF24 |
| **NVS** | Non-Volatile Storage (ESP32's key-value persistent storage) |
| **nRF24L01+PA+LNA** | nRF24 chip with Power Amplifier + Low-Noise Amplifier |
| **OOK/ASK** | On-Off Keying / Amplitude Shift Keying |
| **OTA** | Over-The-Air (firmware update) |
| **PA** | Power Amplifier |
| **PLL** | Phase-Locked Loop |
| **PMF** | Protected Management Frames (802.11w) |
| **PMKID** | Pairwise Master Key Identifier (WPA2 handshake-less attack) |
| **PSRAM** | Pseudo-Static RAM (external SPI RAM) |
| **Promiscuous mode** | WiFi mode where the chip receives all frames, not just addressed ones |
| **RC** | Radio Control (typically for drones, 2.4 GHz) |
| **RSSI** | Received Signal Strength Indicator |
| **SDIO** | Secure Digital Input Output (ESP32-S3 supports SDIO slave) |
| **SSE** | Server-Sent Events (HTTP-based push protocol) |
| **STA** | Station (WiFi client) |
| **Sub-GHz** | Frequencies below 1 GHz (e.g., 433 MHz, 868 MHz, 915 MHz) |
| **TWDT** | Task Watchdog Timer (FreeRTOS) |
| **WebSocket** | Full-duplex TCP-based protocol (RFC 6455) for real-time web |
| **WPA2** | WiFi Protected Access 2 |
| **WPA3** | WiFi Protected Access 3 |
| **XTensa** | Tensilica Xtensa CPU architecture (ESP32, ESP32-S3) |

---

## 22. Open Questions for Reviewer

These are decisions I need your input on before I start coding:

1. **Repository name** — I'm using "PhantomRF" as a working title. Do you want a different name? Suggestions welcome.

2. **License** — MIT (matching W0rthlessS0ul's repos). OK?

3. **Primary chip** — ESP32-S3-WROOM-1-N16R8 (recommended). Should I make this the default in `platformio.ini`?

4. **Web flasher** — should I include the `flasher/` folder as a separate GitHub Pages site, or as a `/flasher` subdirectory served from the same domain?

5. **PCB design** — I want to include KiCad files for a "PhantomRF v1 PCB" (ESP32-S3 + 2× nRF24 + 1× CC1101 + OLED + battery). Should I scope this in for M0 or defer to M1?

6. **3D case** — same question: include FreeCAD files for V3-style case in M0, or M1?

7. **Test coverage** — I have unit tests planned. Do you also want integration tests with a real nRF24 + CC1101 in CI? (Requires self-hosted runner, $$$)

8. **Discord / community** — should I set up a Discord server for users, or just use GitHub Discussions?

9. **Translation** — English only for M0 (per your answer). Should I plan for Indonesian translation in M1?

10. **Contributor license agreement (CLA)** — for a project that may be forked widely, do you want a CLA? (Recommended for any serious project, but adds friction.)

---

## Appendix A: Comparison with inspiration projects

| Aspect | PhantomRF (this) | W0rthlessS0ul/nRF24 | W0rthlessS0ul/CC1101 | W0rthlessS0ul/FZ | smoochiee/NoisyBoy | EmenstaNougat/BlueJammer |
|---|---|---|---|---|---|---|
| Platform | ESP32-S3 (primary) | ESP32 | ESP32 | Flipper | ESP32 | ESP32 |
| Source | Open | Open | Open | Open | Closed | Closed |
| License | MIT | MIT | MIT | MIT | None | Apache 2.0 |
| Radios | nRF24 + CC1101 | nRF24 only | CC1101 only | nRF24 only | nRF24 only | nRF24 only |
| WiFi deauth | Yes (full) | Yes | No | No | Yes (basic) | No |
| BLE spam | Yes (planned) | No | No | No | No | No |
| MouseJack | Yes (planned) | No | No | No | No | No |
| PMKID capture | Yes (planned) | No | No | No | No | No |
| Spectrum | Yes (planned) | RSSI only | None | None | 2.4 GHz | None |
| LittleFS | Yes | No | No | No | No | No |
| OTA | Yes | Yes | No | No | No | No |
| Drag-drop UF2 | Yes (S3) | No | No | No | No | No |
| OLED | Yes (full) | Yes (full) | Yes (full) | Built-in | No | Optional |
| Serial CLI | Yes (full) | Yes (full) | No | No | No | No |
| Web UI | Yes (modern) | Yes (basic) | Yes (basic) | No | Yes (basic) | No (flasher only) |
| WebSerial | Yes | No | No | No | No | No |
| WebSocket | Yes | No | No | No | No | No |
| Documentation | Super complete | Moderate | Moderate | Moderate | Minimal | Minimal |
| 3D case | Yes (planned) | No | No | No | No | Yes |
| Custom PCB | Yes (planned) | No | No | No | No | Yes |

---

## Appendix B: Build matrix

| Environment | Board | Flash | PSRAM | Features |
|---|---|---|---|---|
| `esp32s3-n16r8` | ESP32-S3-DevKitC-1-N16R8 | 16 MB | 8 MB | All |
| `esp32s3-n8r2` | ESP32-S3-DevKitC-1-N8R2 | 8 MB | 2 MB | All except record buffer >1 MB |
| `esp32s3-n8` | ESP32-S3-DevKitC-1-N8 | 8 MB | None | Subset (no spectrum history) |
| `esp32-classic` | ESP32-WROOM-32D | 4 MB | None | All except UF2 drag-drop |
| `esp32s2` | ESP32-S2-Saola-1 | 4 MB | None | All except BLE (no BLE on S2) |
| `esp32c3` | ESP32-C3-DevKitM-1 | 4 MB | None | 2.4 GHz only, no BLE spam |
| `flipper-zero` | Flipper Zero | 1 MB | None | Companion port, 2.4 GHz only |

---

## Appendix C: Acknowledgments

This project stands on the shoulders of giants:

- **TMRh20** (and the `nRF24` GitHub org successors) — for the RF24 library
- **LSatan** — for the SmartRC-CC1101-Driver-Lib
- **h2zero** — for NimBLE-Arduino
- **W0rthlessS0ul** — for the architecture inspiration (nRF24_jammer, CC1101_jammer, FZ_nRF24_jammer)
- **smoochiee** — for the "Noisy Boy" web UI inspiration
- **EmenstaNougat** — for the polished hardware/UX inspiration
- **Samy Kamkar** — for MouseJack research
- **Spacehuhn** (Stefan Kremser) — for the deauth framework
- **Mickey Shin** — for the Flipper Zero sub-GHz research
- **mcore1976** — for the original CC1101 RAW capture (via W0rthlessS0ul's CC1101_jammer README)

---

*End of DESIGN.md — 1300+ lines, ~25,000 words. Reading time: 45 min.*
