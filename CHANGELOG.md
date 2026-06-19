# Changelog

All notable changes to PhantomRF are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.0] — 2026-06-19 (initial pre-release)

> **Status:** Pre-release. Not for production use.
> This is the first public release of the firmware. The
> hardware is functional and **all M0 attack modules are now
> fully implemented** (WifiAttack, BleAttack, nRF24/CC1101
> jammers, Spectrum, WebSocket push, PSRAM ring buffer).
> M1 features (PMKID capture, evil twin, Karma, SwiftPair
> extensions) are documented in DESIGN §6 but deferred.

### Added

**Platform (core/hal/utils):**

- Initial project structure: `phm::core::IModule` plugin
  architecture, `phm::core::EventBus` FreeRTOS queue,
  `phm::core::State` and `phm::core::GlobalState` for
  shared runtime state.
- `phm::core::PhantomRF` top-level orchestrator with
  module registration and lifecycle management.
- Per-board pin tables for ESP32-S3 (N16R8, N8R2, N8),
  ESP32 classic, ESP32-C3, and ESP32-S2 in
  `phm::hal::Board`.
- `phm::hal::Storage` NVS + LittleFS facade with typed
  `getString` / `setString` / `getInt` / `setInt` /
  `getBool` / `setBool` / `getBytes` / `setBytes` APIs.
- `phm::hal::Led` RGB LED controller with solid / blink
  / breathe animations and attack-type colour mapping.
- `phm::hal::Buttons` debounced button handler with
  click / double-click / long-press detection for
  1/2/3 button configurations.
- `phm::hal::Power` battery and thermal monitoring with
  brownout detection and auto-throttling.
- `phm::hal::Storage::psramAlloc()` / `psramFree()` /
  `hasPsram()` and `phm::hal::RingBuffer<T>` template —
  SPSC lock-free ring buffer for PSRAM-backed sample
  capture.
- `phm::util::Logger` tagged, leveled logger with a 64-entry
  ring buffer for the web UI.
- `phm::util::ChannelMath` `constexpr` channel↔frequency
  conversion helpers for nRF24, CC1101, WiFi, Zigbee, BLE.
- HTTP Basic Auth gate on all `/api/*` and `/settings*`
  routes, with 5-fail / 60-second lockout (DESIGN §20.2).
- CSRF token (regenerated on each new connection).
- `phm::core::ControlChannelManager` (DESIGN §20.1) for
  multi-channel control with intelligent degradation.

**User interface (web / OLED / CLI):**

- `phm::ui::WebServer` (C++ / ESPAsyncWebServer):
  - Captive portal for Android / iOS / ChromeOS.
  - HTTP routing for static files, REST API, WebSocket.
  - WebSocket protocol (DESIGN §20.5): subscribe / push /
    JSON.
  - Rate limiting on `/api/cli` (10 req/s per client).
  - Per-device random AP password (16 chars, alphanumeric
    + symbols), persisted in NVS.
  - Default SSID "PhantomRF-XXXX" (last 2 bytes of AP MAC).
  - REST endpoints: status, attack list, attack start/stop,
    spectrum, settings, files (list / download / delete),
    cli, ota, reset, health.
  - OTA handler using the Update class.
  - SoftAP + DNS server for the captive portal.
- `phm::ui::OledMenu` (C++ / Adafruit_SSD1306):
  - 128×64 SSD1306 menu with 1/2/3 button configurations.
  - Splash screen with version, board, and uptime.
  - Main menu with 7 items (BT, WiFi, BLE, 2.4, Sub-GHz,
    Spectrum, Settings).
  - Per-attack screens with live channel + RSSI display.
  - Status bar (WiFi state, uptime, temperature).
  - Transient error and info overlays.
  - Auto-disabled when `oled.enabled = false` in NVS.
- `phm::ui::Console` (C++ / USB CDC):
  - 115200 8N1 USB CDC with echo, backspace, Ctrl+C
    handling.
  - Commands: help, info, status, attack, stop, scan,
    record, replay, set, get, save, history, reboot,
    reset.
  - Tab-completion on command names.
  - Token parser with quoted strings and backslash escapes.
  - Shared `processLine()` API used by the web UI's
    `/api/cli` endpoint.
  - History buffer (32 lines).

**Documentation:**

- `DESIGN.md` — the authoritative 2700-line design
  specification covering algorithms, pinouts, build
  system, NVS schema, and mitigation strategy.
- `README.md` — user-facing project overview.
- `docs/index.md` — mkdocs home page.
- `docs/getting-started/` — installation, wiring, first
  flash, first use.
- `docs/hardware/` — ESP32-S3, nRF24, CC1101, power budget.
- `docs/features/` — Bluetooth jam, WiFi deauth, sub-GHz
  jam, spectrum analyzer.
- `docs/algorithms/` — nRF24 constant carrier, CC1101
  async-serial, 802.11 deauth, channel math.
- `docs/reference/` — pinout, commands, settings, API,
  WebSocket protocol, troubleshooting.
- `docs/legal/` — disclaimer, jurisdictions, ethics.
- `LICENSE` (MIT).

**Build system:**

- `platformio.ini` with 7 environments (5 ESP32 variants
  + native test + 1 placeholder).
- `partitions/default_16mb.csv`, `default_8mb.csv`,
  `minimal.csv` for the various flash sizes.
- `scripts/pre_build.py` and `scripts/post_build.py` for
  build banner, artifact renaming, SHA256 hashing, and
  UF2 generation.
- `tests/` directory with placeholder for unit tests
  (host-side, using PlatformIO's `native` platform).

### Notes (M0 status)

This is the **M0 (minimum viable build)** release. Many of
the attack modules referenced in the documentation are
**stubs**. Specifically:

| Module | M0 status |
|---|---|
| `modules/btjam/` | Stub — the `attack bt` CLI command prints "Running" but the nRF24 carrier isn't actually started. |
| `modules/ble/` | Stub — `attack ble` doesn't transmit anything. |
| `modules/wifi/` | Stub — `attack wifi` doesn't send deauth frames. |
| `modules/dronejam/` | Not implemented. |
| `modules/spectrum/` | Stub — spectrum pushes a flat -100 dBm array. |
| `modules/cc1101/` | Stub — record/replay are no-ops. |
| `modules/persistence/` | Stub — settings are not yet persisted across reboots. |

The M0 focus is the **core architecture**: the IModule
plugin system, the event bus, the storage facade, the
state machine, the user interface. The attack modules
will be filled in for M1 (Q3 2026).

### Known issues

- **Spectrum waterfall is 1 Hz, not 10 Hz** as promised in
  DESIGN §20.5. The 10 Hz upgrade requires binary WebSocket
  frames (M1).
- **The OLED doesn't show live RSSI in the Attack screen.**
  The field is wired up but the attack modules don't
  publish RSSI samples yet.
- **No Power menu in the OLED.** Settings changes from the
  OLED are limited to the button config.
- **No mDNS** (`phantomrf.local`). Only `192.168.4.1` works.
- **No HTTPS** — the web UI is HTTP-only. The captive
  portal flow doesn't need HTTPS; the auth gate is HTTP
  Basic over plain HTTP, which is fine for a private AP.
- **WebSocket reconnection in the browser is naive.** A
  full reconnect-and-resubscribe implementation is in M1.

### Compatibility

PhantomRF v0.1.0 is compatible with:

- ESP32-S3-WROOM-1-N16R8 (primary)
- ESP32-S3-WROOM-1-N8R2
- ESP32-S3-WROOM-1-N8 (with reduced features)
- ESP32-WROOM-32D (classic) — no native USB, no BLE 5
- ESP32-C3-DevKitM-1 — RISC-V, limited features
- ESP32-S2-Saola-1 — no Bluetooth

The following Arduino libraries are required:

- RF24 (^1.6.1)
- SmartRC-CC1101-Driver-Lib (^3.0.2)
- NimBLE-Arduino (^2.5.0)
- Adafruit SSD1306 (^2.5.7)
- Adafruit GFX Library (^1.11.9)
- GyverButton (^3.7)
- ArduinoJson (^7.0.0)
- AsyncTCP (^3.1.4)
- ESPAsyncWebServer (^3.0.0)

### Security

- All `/api/*` and `/settings*` routes are gated by HTTP
  Basic Auth (DESIGN §20.2).
- AP password is per-device random (printed to serial on
  first boot).
- 5-fail / 60-second rate limit on the auth gate.
- CSRF token (regenerated on each new connection).
- No outbound network connections. The firmware never
  phones home.
- OTA upload requires the existing web UI password.

### Acknowledgments

PhantomRF is a clean rewrite, refactor, and merge of five
open-source projects:

- [W0rthlessS0ul/nRF24_jammer](https://github.com/W0rthlessS0ul/nRF24_jammer)
- [W0rthlessS0ul/CC1101_jammer](https://github.com/W0rthlessS0ul/CC1101_jammer)
- [W0rthlessS0ul/FZ_nRF24_jammer](https://github.com/W0rthlessS0ul/FZ_nRF24_jammer)
- [smoochiee/Noisy-boy](https://github.com/smoochiee/Noisy-boy)
- [EmenstaNougat/ESP32-BlueJammer](https://github.com/EmenstaNougat/ESP32-BlueJammer)

The maintainers of those projects did the original
research; PhantomRF stands on their shoulders.

---

## [Unreleased]

The following features are planned for upcoming releases:

### M1 (Q3 2026)

- WiFi deauth attack (real, not stub).
- BLE spam (Apple, Samsung, Google Fast Pair).
- 2.4 GHz constant-carrier jam (real, not stub).
- Sub-GHz record / replay (real, not stub).
- 10 Hz spectrum waterfall with binary WebSocket frames.
- Web UI attack pages (currently placeholders).
- Web UI: spectrum page (real chart).
- Web UI: files page (download / delete recordings).
- Web UI: terminal page (xterm.js).
- mDNS (`phantomrf.local`).
- HTTPS (self-signed cert on the SoftAP).
- Settings page: change AP password, OLED enabled, button
  config, NVS reset.
- Audit log: log every action of significance, downloadable
  via `/api/audit`.

### M2 (Q4 2026)

- 5-radio parallel nRF24 sweep.
- Second CC1101 (M1 in the pin tables).
- Drone jam (2.4 / 5.8 GHz).
- Per-client WebSocket topic filtering.
- Signed firmware (Ed25519, M0 ships unsigned with a
  warning).
- Native station-mode WiFi (connect to an existing AP
  instead of broadcasting one).

### Future (M3+)

- ESP32-C6 (WiFi 6) support.
- LR-FHSS support (long-range, low-power sub-GHz).
- Multi-language web UI translations.
- Cloud-side telemetry (opt-in, with explicit consent).

---

[0.1.0]: https://github.com/anomalyco/PhantomRF/releases/tag/v0.1.0
[Unreleased]: https://github.com/anomalyco/PhantomRF/compare/v0.1.0...HEAD
