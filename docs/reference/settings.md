# NVS settings

> Every NVS key, with its type, default, valid range, and
> description. The authoritative reference for the `set` /
> `get` CLI commands and the `/api/settings` REST endpoint.

Settings are stored in the NVS namespace `"phm"`. They
survive reboot, OTA flash, and power loss. `set` writes
without committing; use `save` (or reboot) to commit.

---

## Table of contents

- [nRF24 settings](#nrf24-settings)
- [CC1101 settings](#cc1101-settings)
- [Bluetooth settings](#bluetooth-settings)
- [WiFi settings](#wifi-settings)
- [AP settings](#ap-settings)
- [OLED settings](#oled-settings)
- [Buttons settings](#buttons-settings)
- [Spectrum settings](#spectrum-settings)
- [Power settings](#power-settings)
- [Sweep settings](#sweep-settings)
- [Beacon spam settings](#beacon-spam-settings)
- [Persistence settings](#persistence-settings)
- [Next steps](#next-steps)

---

## nRF24 settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `nrf24.count` | int8 | 1 | 1–5 | Number of nRF24 modules |
| `nrf24.ce_pins` | bytes | `[16,18,35,37,39]` | 0–48 | CE pin per module (overrides Board table) |
| `nrf24.csn_pins` | bytes | `[15,17,21,36,38]` | 0–48 | CSN pin per module |
| `nrf24.pa` | int8 | 0 | 0–3 | PA level: 0=MIN (-18 dBm), 1=LOW (-12), 2=HIGH (-6), 3=MAX (0 dBm, with PA-LNA +20 dBm effective) |
| `nrf24.data_rate` | int8 | 2 | 0–2 | 0=250 kbps, 1=1 Mbps, 2=2 Mbps |
| `nrf24.channel` | int8 | 76 | 0–125 | Default channel for "spot" attacks |
| `nrf24.channel_min` | int8 | 0 | 0–125 | Lower bound of channel sweep |
| `nrf24.channel_max` | int8 | 125 | 0–125 | Upper bound of channel sweep |
| `nrf24.dwell_ms` | int16 | 2 | 1–1000 | Dwell time per channel in ms |
| `nrf24.sweep_method` | int8 | 0 | 0–1 | 0=together (all radios at once), 1=separate (one at a time) |

### Example

```text
phantom> set nrf24.pa 3
Set nrf24.pa = 3
phantom> save
```

> :warning: **PA level 3 with the bare nRF24 (no PA-LNA)
> gives 0 dBm, not +20 dBm.** The +20 dBm is from the
> external PA chip on the PA-LNA module. Setting `pa = 3`
> on a bare module still works, but you'll see very short
> range.

## CC1101 settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `cc1101.present` | bool | true | — | CC1101 module detected (set by `setup()`) |
| `cc1101.frequency` | float | 433.92 | 300–928 | Default frequency in MHz |
| `cc1101.sampling_us` | int16 | 150 | 10–10000 | Sampling interval for async-serial mode (µs) |
| `cc1101.payload` | int8 | 60 | 1–255 | TX payload size (not used in async-serial) |
| `cc1101.modulation` | int8 | 3 | 0–3 | 0=2-FSK, 1=GFSK, 3=OOK/ASK |
| `cc1101.pa_table` | int8 | 0 | 0–3 | PA table index (band-specific) |
| `cc1101.output_power` | int8 | 7 | 0–7 | Output power: 0=-30 dBm, 7=+12 dBm |

### Example

```text
phantom> set cc1101.frequency 868.35
Set cc1101.frequency = 868.35
```

## Bluetooth settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `ble.method` | int8 | 0 | 0–4 | BLE jam method: 0=adv, 1=data, 2=all, 3=apple, 4=samsung |
| `ble.apple_auth_tag` | bytes | (random) | 16 B | Rolling auth tag for Apple Continuity spam |
| `ble.samsung_payload` | bytes | (template) | 16 B | Galaxy Buds template for Samsung spam |
| `ble.fast_pair_model_id` | int32 | 0x718FA4 | any | Google Fast Pair model ID |
| `ble.adv_interval_ms` | int16 | 30 | 20–10000 | BLE advertising interval |
| `ble.tx_power` | int8 | 3 | 0–3 | TX power: 0=-12, 1=-6, 2=0, 3=+6 dBm |

## WiFi settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `wifi.jam_method` | int8 | 0 | 0–2 | 0=channel, 1=smart, 2=all |
| `wifi.deauth_method` | int8 | 0 | 0–2 | 0=channel, 1=all, 2=smart |
| `wifi.reason_code` | int16 | 1 | 1–68 | 802.11 deauth reason code |
| `wifi.channel_hop_ms` | int16 | 250 | 50–5000 | Time per channel when hopping |
| `wifi.beacon_ssid` | string | "PhantomRF" | ≤32 chars | SSID to flood (or "random") |
| `wifi.beacon_count` | int16 | 1 | 1–1000 | Beacons to send per second |
| `wifi.pmf_required` | bool | false | — | Skip networks with PMF required |

### Example

```text
phantom> set wifi.deauth_method 2
Set wifi.deauth_method = 2
phantom> set wifi.reason_code 7
Set wifi.reason_code = 7
```

## AP settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `ap.ssid` | string | "PhantomRF-XXXX" | ≤32 chars | SoftAP SSID (last 4 of MAC) |
| `ap.password` | string | (16-char random) | 8–63 chars | WPA2-PSK password |
| `ap.enabled` | bool | true | — | AP on/off |
| `ap.channel` | int8 | 1 | 1–13 | AP channel |
| `ap.max_connections` | int8 | 4 | 1–8 | Max simultaneous clients |
| `ap.hidden` | bool | false | — | Hide the SSID in beacon frames |

### Example

```text
phantom> set ap.ssid "PhantomRF Lab"
Set ap.ssid = PhantomRF Lab
phantom> set ap.password "change-me-now"
Set ap.password = change-me-now
phantom> save
```

> :warning: **The password is stored in plaintext in NVS.**
> The NVS namespace is not encrypted. Don't use a password
> you reuse elsewhere.

## OLED settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `oled.enabled` | bool | true | — | OLED on/off |
| `oled.brightness` | int8 | 255 | 0–255 | OLED contrast (0=off, 255=max) |
| `oled.flip` | bool | false | — | Flip the display 180° |
| `oled.theme` | int8 | 0 | 0–1 | 0=dark (default), 1=light |

### Example

```text
phantom> set oled.enabled false
Set oled.enabled = false
phantom> save
```

After `save`, the OLED will be off on the next boot. To
re-enable, set it back to `true` from the web UI.

## Buttons settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `buttons.config` | int8 | 2 | 0–2 | 0=1-button, 1=2-button, 2=3-button |
| `buttons.debounce_ms` | int16 | 35 | 10–500 | Debounce time |
| `buttons.long_press_ms` | int16 | 600 | 200–3000 | Long-press threshold |
| `buttons.double_click_ms` | int16 | 300 | 100–1000 | Double-click window |

### Example

```text
phantom> set buttons.config 0
Set buttons.config = 0
phantom> save
```

This switches the OLED menu to 1-button mode (short = next,
long = select). Change it back to `2` for the default
3-button mode.

## Spectrum settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `spectrum.window_ms` | int16 | 100 | 10–10000 | Sweep window per push (ms) |
| `spectrum.band` | int8 | 0 | 0–1 | 0=2.4 GHz, 1=sub-GHz |
| `spectrum.samples_per_channel` | int8 | 1 | 1–10 | RSSI samples to average |
| `spectrum.min_rssi` | int8 | -100 | -120 to -40 | Bottom of the waterfall (dBm) |
| `spectrum.max_rssi` | int8 | -40 | -100 to -20 | Top of the waterfall (dBm) |

## Power settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `power.thermal_warn_c` | int16 | 650 | 50–1000 | Thermal warning (0.1 °C units, so 650 = 65.0 °C) |
| `power.thermal_throttle_c` | int16 | 750 | 50–1000 | Thermal throttle (75.0 °C) |
| `power.thermal_critical_c` | int16 | 850 | 50–1000 | Thermal shutdown (85.0 °C) |
| `power.battery_calibration` | int16 | 1200 | 500–2500 | ADC → mV factor (×100, so 1200 = 12.00 mV/LSB) |
| `power.auto_sleep_ms` | int32 | 0 | 0–3600000 | Auto-sleep after N ms idle (0=disabled) |

## Sweep settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `sweep.method` | int8 | 0 | 0–1 | 0=together (all radios at once), 1=separate (one at a time) |
| `sweep.dwell_ms` | int16 | 2 | 1–1000 | Dwell time per channel in ms |
| `sweep.skip_channels` | bytes | (empty) | 0–125 list | nRF24 channels to skip (e.g. for testing) |

## Beacon spam settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `ssid.list` | bytes | (default) | ≤100 SSIDs | Up to 100 SSIDs for beacon spam |
| `ssid.random` | bool | true | — | Generate random SSIDs if `ssid.list` is empty |
| `ssid.count` | int16 | 1 | 1–100 | Number of SSIDs to send per beacon |

### Example

```text
phantom> set ssid.list "Free WiFi,ATT-Wifi,linksys"
Set ssid.list = Free WiFi,ATT-Wifi,linksys
```

(Comma-separated for the CLI; in NVS the value is stored as
a binary blob.)

## Persistence settings

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `persist.compression` | int8 | 0 | 0–2 | 0=none, 1=gzip, 2=lzf |
| `persist.max_log_bytes` | int32 | 102400 | 0–1048576 | Max size of the rolling log file |
| `persist.max_recording_count` | int16 | 256 | 1–1024 | Max recordings on the PSRAM ring |
| `persist.audit_log_enabled` | bool | true | — | Record actions to the audit log |

## Next steps

- The CLI commands that read / write these keys are in
  [commands.md](commands.md).
- The REST API equivalent is in [api.md](api.md).
- The WebSocket protocol is in
  [websocket-protocol.md](websocket-protocol.md).
