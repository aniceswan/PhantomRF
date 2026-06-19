# Power budget

> Worst-case current, peak loads, battery life, and recommended
> supplies. Read this before you connect anything bigger than a
> USB-C cable.

The ESP32-S3's on-board 3V3 LDO is rated for ~600 mA. The nRF24
PA-LNA peaks at 115 mA. The CC1101 at 30 mA. The ESP32-S3's
WiFi radio at 500 mA. **Add those up and you're past 1.5 A at
peak.** A single 18650 cell can't deliver that for long, and
many USB ports will brown out.

This page walks through the worst case, recommends a power
supply, and shows how to estimate battery life.

---

## Table of contents

- [Power tree](#power-tree)
- [Worst-case current](#worst-case-current)
- [Recommended supplies](#recommended-supplies)
- [Battery life estimates](#battery-life-estimates)
- [Brownout scenarios](#brownout-scenarios)
- [Thermal](#thermal)
- [Next steps](#next-steps)

---

## Power tree

PhantomRF has **three** rails, all derived from a single
5 V source (USB-C or external):

```
   5 V (USB-C, barrel jack, or battery + boost)
    │
    ├── ESP32-S3 dev board's on-board 3V3 LDO (600 mA)
    │      │
    │      ├── ESP32-S3 core (peak 500 mA during WiFi TX)
    │      ├── OLED (peak 20 mA at full brightness)
    │      ├── Buttons / LED (≈ 30 mA total)
    │      │
    │      └── nRF24L01+PA+LNA (peak 115 mA during TX)
    │             │
    │             └── 3V3 → on-board PA regulator → PA chip
    │
    └── (optional) External 3V3 LDO for the CC1101 (30 mA peak)
```

> :warning: **The CC1101 can be powered from the same 3V3 rail,
> but if you add a separate LDO you can isolate it for cleaner
> RX.** The CC1101 is sensitive to 3V3 noise; a separate
> AP2112K-3.3 regulator is cheap insurance.

## Worst-case current

The table below assumes every subsystem is on at maximum
draw. In practice you never hit all of these simultaneously,
but the peak is what your supply must handle.

| Subsystem | Peak current | Avg current | Notes |
|---|---|---|---|
| ESP32-S3 (WiFi TX, max power) | 500 mA | 160 mA | During a WiFi deauth or AP broadcast |
| ESP32-S3 (CPU only) | 75 mA | 40 mA | Idle WiFi, no TX |
| ESP32-S3 (BLE) | 130 mA | 30 mA | During BLE scanning |
| nRF24 (RX, 2 Mbps) | 13.5 mA | 13.5 mA | |
| nRF24 (TX, +20 dBm CW) | 115 mA | 115 mA | Heavy — heat-sink recommended |
| nRF24 (TX, 0 dBm) | 11.3 mA | 11.3 mA | Bare nRF24, not the PA-LNA |
| CC1101 (RX) | 16 mA | 16 mA | |
| CC1101 (TX, +12 dBm) | 30 mA | 30 mA | |
| OLED (128×64, full on) | 20 mA | 8 mA | Average is much lower; most pixels off |
| RGB LED (white, full brightness) | 60 mA | 5 mA | |
| USB-UART bridge (if any) | 30 mA | 10 mA | Only on classic ESP32 |

**Worst case (everything at peak):**

```
 500 + 115 + 30 + 20 + 60 = 725 mA
```

**Typical case (a 2.4 GHz deauth + WiFi AP active):**

```
 200 + 115 + 20 + 5 = 340 mA
```

**Idle case (AP up, no attack):**

```
 80 + 13 + 16 + 5 = 114 mA
```

## Recommended supplies

| Use case | Recommended supply | Notes |
|---|---|---|
| Bench development | USB-C PD, 5 V / ≥ 1.5 A | The ESP32-S3 dev board's barrel jack is usually rated 1 A — close to the limit |
| Portable, short-term | 2× 18650 in parallel (7.4 V) → buck to 5 V → USB-C | Provides ≈ 6000 mAh at 3.7 V, lasts ≈ 6 hours of typical use |
| Portable, long-term | 4× 18650 in 2S2P, with a TP4056 per cell | ≈ 12 Ah at 7.4 V, ≈ 24 hours of typical use |
| Fixed installation | USB-C, 5 V / 1 A, from a wall wart | Fine for always-on AP mode |
| Solar | 6 V / 2 W panel + TP4056 + 18650 + boost to 5 V | Trickle charges during the day; runs overnight on the cell |

> :warning: **Don't use a "phone charger" without checking the
> rating.** Many cheap phone chargers are labelled 1 A but can
> only sustain 500 mA. The ESP32-S3's brownout detector will
> trip if the voltage sags during a WiFi TX burst, and the
> chip will silently reboot.

## Battery life estimates

Based on the typical case (340 mA average) and the worst case
(725 mA peak, 30% of the time so ≈ 280 mA average for an
attack scenario):

| Battery | Capacity (mAh @ 3.7 V) | Idle (114 mA) | Typical (340 mA) | Attack (280 mA) |
|---|---|---|---|---|
| 1× 18650 | 3000 | 26 h | 8.8 h | 10.7 h |
| 2× 18650 (parallel) | 6000 | 52 h | 17.6 h | 21.4 h |
| 4× 18650 (2S2P) | 6000 @ 7.4 V | 26 h @ 7.4 V → ≈ 19 h after buck loss | 8.8 h | 10.7 h |
| 10000 mAh USB power bank | 10000 @ 3.7 V | 87 h | 29 h | 35 h |
| 20000 mAh USB power bank | 20000 @ 3.7 V | 175 h | 59 h | 71 h |

> :information_source: **The "mAh" rating is at the cell
> voltage.** A 10000 mAh power bank at 3.7 V is ≈ 37000 mWh,
> which after a 5 V boost is ≈ 7400 mAh at 5 V. Always check
> the watt-hours, not the milliamp-hours.

## Brownout scenarios

A brownout happens when the 3V3 rail sags below the chip's
minimum operating voltage. The chip then resets itself. Common
scenarios:

| Scenario | Trigger | Mitigation |
|---|---|---|
| Cheap USB cable | High resistance on the +5 V wire | Use a 22 AWG cable or shorter |
| USB hub with shared power | Hub can't supply 500 mA peak per port | Plug into the computer directly, or use a powered hub |
| nRF24 TX burst during WiFi TX | 115 + 500 = 615 mA peak, regulator sags | Beef up the 3V3 LDO, or sequence the transmissions |
| Weak 18650 under load | Cell voltage sags under 100 mA+ draw | Use a cell rated for 5 A continuous discharge |
| Solar panel with no battery | Cloud passes, voltage drops | Always have a battery in the path |

The ESP32-S3 has a built-in brownout detector that resets the
chip if the 3V3 rail drops below ≈ 3.0 V. The threshold is
configurable in software (`esp_brownout_init` in
`menuconfig`).

## Thermal

The ESP32-S3's internal temperature sensor is read every 5
seconds and exposed on the web UI. The firmware has three
thermal thresholds (see `Config.h`):

| Threshold | Action |
|---|---|
| 65 °C | Yellow LED pulse, "High temp" warning in the log |
| 75 °C | Auto-throttle WiFi TX power; OLED shows "throttled" |
| 85 °C | Emergency shutdown; reboots after 30 s |

The nRF24 PA chip is the biggest external heat source. At
+20 dBm CW (constant carrier), it can hit 80 °C without a
heatsink. Recommended:

- A small aluminium tab (10 × 10 × 5 mm) glued to the PA chip
  with thermal epoxy
- A clip-on heatsink (the kind used for Raspberry Pi SoCs)
- Forced airflow (a small 30 mm fan running at 5 V)

Without any cooling, expect thermal throttling after about
3 minutes of continuous CW jam.

## Next steps

- The wiring is the next thing to check. See
  [Wiring guide](../../getting-started/wiring.md).
- For the per-board pinout, see
  [Pinout per board](../reference/pinout.md).
- If you're building a portable rig, see
  [Installation](../../getting-started/installation.md#hardware-optional)
  for the recommended battery and charger parts.
