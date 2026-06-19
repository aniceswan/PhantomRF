# CLI commands

> Complete reference for the USB CDC and web-terminal command
> set. The same parser is used by the OLED menu actions, the
> web `/api/cli` endpoint, and the WebSocket `command` frame.

The CLI is line-oriented. Newline-terminated commands, 115200
baud 8N1 on USB CDC. The web terminal accepts the same
commands over a WebSocket or the `/api/cli` REST endpoint.

---

## Table of contents

- [Command summary](#command-summary)
- [`help`, `?`](#help)
- [`info`](#info)
- [`status`](#status)
- [`attack`](#attack)
- [`stop`](#stop)
- [`scan`](#scan)
- [`record`](#record)
- [`replay`](#replay)
- [`set`](#set)
- [`get`](#get)
- [`save`](#save)
- [`history`](#history)
- [`reboot`](#reboot)
- [`reset`](#reset)
- [Editor features](#editor-features)
- [Next steps](#next-steps)

---

## Command summary

| Command | Aliases | Args | Description |
|---|---|---|---|
| `help` | `?` | — | List all commands |
| `info` | — | — | Device info (version, heap, uptime) |
| `status` | — | — | Current state (idle / running, which module) |
| `attack` | — | `<target> [method]` | Start an attack |
| `stop` | — | — | Stop the current attack |
| `scan` | — | `wifi` \| `ble` | Scan for networks / devices |
| `record` | — | `<mhz> [ms]` | Record sub-GHz signal |
| `replay` | — | `<mhz>` | Replay last recording |
| `set` | — | `<key> <value>` | Set NVS key |
| `get` | — | `<key>` | Get NVS key |
| `save` | — | — | Commit pending NVS writes |
| `history` | — | — | Show command history |
| `reboot` | — | — | Restart the device |
| `reset` | — | — | Factory reset (clear NVS) |

The prompt is `phantom> `. Lines are tokenised on whitespace;
quoted strings are kept as a single token:

```text
phantom> set ap.ssid "PhantomRF Lab"
```

The single and double quotes are both accepted. Backslash
escapes are honoured inside quoted strings: `\"` and `\\`.

## `help`, `?`

Print the list of commands with one-line descriptions.

```text
phantom> help

Available commands:
  help     - List commands
  ?        - Alias for help
  info     - Device info (version, free heap, uptime)
  status   - Current state (which module, which attack)
  attack   - Start an attack
  stop     - Stop the current attack
  scan     - Scan networks / devices
  record   - Record sub-GHz signal
  replay   - Replay recorded signal
  set      - Set NVS key
  get      - Get NVS key
  save     - Commit pending NVS writes
  history  - Show command history
  reboot   - Reboot device
  reset    - Factory reset
```

## `info`

Print device info: version, board, chip, CPU, heap, uptime,
temperature, battery, AP status.

```text
phantom> info
Device:   PhantomRF
Version:  0.1.0
Board:    ESP32-S3-N16R8
Chip:     ESP32-S3
CPU:      240 MHz
Heap:     198456 bytes free
Min heap: 185320
Uptime:   312 s
Temp:     42.3 C
Vbat:     3872 mV
AP:       active
WS clis:  1
```

## `status`

Print the current operating state.

```text
phantom> status
State:     Idle
Module:    (none)
```

While an attack is running:

```text
phantom> status
State:     Running
Module:    1
```

The "Module" field is the `IModule::id()` of the active
attack module. `0` means none / idle.

## `attack`

Start an attack. The `<target>` selects the radio / protocol;
the optional `<method>` selects the sub-mode.

```text
phantom> attack <target> [method]
```

### Targets

| Target | Radio | Description |
|---|---|---|
| `bt`     | nRF24  | Bluetooth classic jam |
| `ble`    | nRF24  | BLE spam (Apple, Samsung, Fast Pair) |
| `wifi`   | ESP32  | 802.11 deauth / beacon flood |
| `2_4`    | nRF24  | 2.4 GHz constant carrier |
| `subghz` | CC1101 | Sub-GHz jam (spot, range, hopper) |
| `drone`  | nRF24  | Drone bands (2.4 / 5.8) |
| `spec`   | both   | Spectrum analyzer (RX only) |
| `stop`   | —      | Stop the current attack |

### Methods

| Target | Method | Description |
|---|---|---|
| `bt`    | `default` | Sweep the 20 most common BT channels |
| `bt`    | `full`    | Sweep all 79 BT channels (slower) |
| `bt`    | `spot`    | Jam a single channel (use `set nrf24.channel` first) |
| `wifi`  | `deauth`  | Forged deauth frames |
| `wifi`  | `beacon`  | Beacon flood with random SSIDs |
| `wifi`  | `smart`   | Auto-detect APs and deauth their clients |
| `subghz`| `spot`    | Single frequency |
| `subghz`| `range`   | Sweep a range of frequencies |
| `subghz`| `hopper`  | Pseudo-random hop |
| `2_4`   | `sweep`   | Sweep all 126 channels |
| `2_4`   | `spot`    | Single channel |

### Examples

```text
phantom> attack bt
Starting bt (method=default)...
Running. Send 'attack stop' to abort.

phantom> attack wifi deauth
Starting wifi (method=deauth)...
Running. Send 'attack stop' to abort.

phantom> attack subghz range
Starting subghz (method=range)...
Running. Send 'attack stop' to abort.
```

## `stop`

Stop the current attack. Equivalent to `attack stop`.

```text
phantom> stop
Stopped.
```

## `scan`

Scan for nearby networks or devices.

```text
phantom> scan wifi
Scanning WiFi... (will populate /recordings/wifi_scan.json)

phantom> scan ble
Scanning BLE... (will populate /recordings/ble_scan.json)
```

In M0, the scan command prints "not implemented" because the
radio modules are stubs. M1 wires the actual scan to the WiFi
and NimBLE stacks.

## `record`

Record a sub-GHz signal. The CC1101 enters RX mode, samples
GDO0 at the configured rate, and saves the bitstream to a
`.sub` file on LittleFS.

```text
phantom> record <freq_mhz> [duration_ms]
```

Default duration is 1000 ms.

```text
phantom> record 433.92
Recording at 433.92 MHz for 1000 ms
Saved to /recordings/2026-06-19_12-34-56_433.92.sub
```

## `replay`

Replay the last recording at a given frequency.

```text
phantom> replay <freq_mhz>
```

```text
phantom> replay 433.92
Replaying 433.92 MHz...
```

## `set`

Set an NVS key. The value is auto-detected as int, bool, or
string.

```text
phantom> set <key> <value>
```

```text
phantom> set nrf24.pa 3
Set nrf24.pa = 3

phantom> set ap.ssid "My PhantomRF"
Set ap.ssid = My PhantomRF

phantom> set oled.enabled false
Set oled.enabled = false
```

The `set` command writes to NVS but does not commit. Use
`save` to commit (or the firmware commits automatically on
reboot).

## `get`

Get an NVS key. Returns the value with its type.

```text
phantom> get <key>
```

```text
phantom> get nrf24.pa
nrf24.pa = 3

phantom> get oled.enabled
oled.enabled = true

phantom> get ap.ssid
ap.ssid = My PhantomRF

phantom> get nonexistent
(key not set)
```

The complete list of NVS keys is in
[settings.md](settings.md).

## `save`

Commit any pending NVS writes. Use this after a series of
`set` commands to ensure they're persisted to flash.

```text
phantom> save
Settings saved to flash.
```

## `history`

Show the recent command history (last 32 lines).

```text
phantom> history
1  attack bt
2  attack stop
3  set nrf24.pa 3
4  info
5  attack subghz range
```

## `reboot`

Restart the device. Equivalent to pressing the RST button.

```text
phantom> reboot
Rebooting...
```

The serial monitor will disconnect and re-connect as the
device reboots. Wait 2–3 seconds for the banner to appear
again.

## `reset`

Factory reset: clears the NVS namespace and reboots. All
settings return to their defaults, and the AP password is
regenerated.

```text
phantom> reset
Factory reset: clearing NVS and rebooting...
```

> :warning: **This is destructive.** Your custom settings,
> recordings (no, those are on LittleFS, not NVS), and
> audit log will be lost. The default AP password (printed
> to the serial console) is regenerated.

## Editor features

When typing on the USB CDC terminal, the following shortcuts
are honoured:

| Shortcut | Action |
|---|---|
| Enter / Return | Submit the line |
| Backspace / DEL | Delete the previous character |
| Ctrl+C | Stop the current attack (same as `attack stop`) |
| Ctrl+L | Redraw the line (no clear-screen) |
| Tab | Complete the current command name |

Command history (up / down arrow keys) is not implemented in
M0 for the USB CDC terminal — the MCU doesn't have a real
TTY layer. The web terminal does support up / down arrow
recall via the browser's input history (via
`localStorage`).

## Next steps

- All NVS keys (what you can `set` / `get`) are in
  [settings.md](settings.md).
- The REST API (the same commands as JSON over HTTP) is in
  [api.md](api.md).
- The WebSocket protocol (push from server, command from
  client) is in [websocket-protocol.md](websocket-protocol.md).
- The legal picture is in
  [Disclaimer](../legal/disclaimer.md).
