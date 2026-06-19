# Spectrum analyzer

> Live RSSI waterfall for both the 2.4 GHz and sub-GHz bands.
> The "is anything transmitting here?" question, answered in
> real time.

A spectrum analyzer sweeps through a range of frequencies and
measures the received signal strength (RSSI) on each one. The
result is a graph: frequency on the X axis, RSSI on the Y
axis, time scrolling downward. PhantomRF does this on both the
nRF24 (2.4 GHz, 126 channels) and the CC1101 (sub-GHz, up to
256 channels).

The spectrum analyzer is **read-only** — the radio only
listens, never transmits. It's the safest attack to demo.

---

## Table of contents

- [What it shows](#what-it-shows)
- [How it works](#how-it-works)
- [How to use it](#how-to-use-it)
- [Interpreting the waterfall](#interpreting-the-waterfall)
- [What's normal in your area](#whats-normal-in-your-area)
- [Limitations](#limitations)
- [CLI / web equivalent](#cli--web-equivalent)
- [Next steps](#next-steps)

---

## What it shows

The spectrum page renders two views:

**Bar chart (top of the page):** the current RSSI on every
channel. Updated 1× per second in M0 (10× per second in M1
with the binary-frame WebSocket protocol).

**Waterfall (bottom of the page):** the RSSI history. Each
column is a channel (X axis = frequency, Y axis = time, color
= RSSI). A new row scrolls in every 100 ms. The full waterfall
is 200 rows tall, giving 20 seconds of history.

The waterfall uses a thermal colormap: dark blue = -100 dBm
(nothing), red = -40 dBm (saturated), green/yellow = the
useful range (-90 to -60 dBm).

## How it works

The 2.4 GHz waterfall is built on the nRF24L01+ in RX mode:

1. Set the nRF24 to channel N.
2. Wait 128 µs (the nRF24's RSSI settling time).
3. Read the RSSI register.
4. Set the nRF24 to channel N+1.
5. Repeat for all 126 channels.
6. Push the result over the WebSocket.

One full sweep takes 126 × 150 µs ≈ 20 ms. The M0 firmware
runs 1 sweep per second (50× faster than needed) and uses the
spare time to process button events, update the OLED, and
forward log messages.

The sub-GHz waterfall is built on the CC1101 in RX mode. The
CC1101 has a wider range and finer resolution, but the
calibration delay (1 ms per frequency change) limits the sweep
to about 100 channels/second. For the typical 256-channel
view, one full sweep takes ≈ 3 seconds.

## How to use it

### From the web UI

1. Open the **Spectrum** page from the sidebar.
2. Pick the band (2.4 GHz or sub-GHz).
3. Pick the centre frequency (sub-GHz only).
4. Pick the channel width (1 MHz or 5 MHz).
5. Click **Start**.

The waterfall will start scrolling within 1 second. Bright
spots indicate RF activity.

### From the CLI

```text
phantom> attack spec
Starting spec (method=2.4ghz)...
Running. Send 'attack stop' to abort.
```

To switch to sub-GHz:

```text
phantom> attack spec subghz
```

### From the OLED

The OLED shows a simplified version: a 128-pixel-wide bar
graph with the current RSSI on each channel. It updates every
100 ms.

1. Use Next to highlight **Spectrum** in the main menu.
2. Press OK to start.
3. Long-press OK to stop.

## Interpreting the waterfall

The waterfall tells you what's transmitting, on what
frequency, and how strong. Here's what the patterns mean.

### 2.4 GHz band

| Pattern | What it is | Why |
|---|---|---|
| Spikes on channels 1, 6, 11 | WiFi APs | The 3 non-overlapping channels most APs use |
| Wide bright stripe at low frequencies | Bluetooth | BT hops across all 79 channels |
| Repeating pattern every 2 channels | Zigbee | Zigbee uses channels 11–26, 5 MHz spacing |
| Random flashes | Microwave oven | Magnetron leakage at 2.45 GHz |
| Steady low-level noise | WiFi APs in promiscuous RX | APs always beacon, even when idle |

### Sub-GHz band

| Pattern | What it is | Why |
|---|---|---|
| Spikes at 433.92, 434.0, 434.1 MHz | ISM devices | EU 433 MHz ISM band |
| Wide stripe at 868 MHz | LoRaWAN | 8 channels in the EU 868 MHz band |
| Brief flashes every 30 seconds | Weather station | Sends data periodically |
| Brief flashes when you press a keyfob | Keyfob | 32-bit ID, 5–10 repeats |

## What's normal in your area

In a typical urban environment, you should see:

- **2.4 GHz:** 5–20 WiFi APs (the exact number depends on
  density), some Bluetooth activity, possibly Zigbee.
- **Sub-GHz (433 MHz):** weather stations, keyfobs, sometimes
  a TPMS sensor from a passing car.
- **Sub-GHz (868 MHz):** LoRaWAN devices (if there's a
  gateway nearby), some industrial sensors.
- **Sub-GHz (915 MHz):** LoRaWAN (US), some garage door
  openers.

In a quiet rural area, the spectrum will be much emptier —
mostly background noise with occasional spikes.

## Limitations

- **The nRF24 can only measure 126 discrete channels**, each
  1 MHz wide. True wideband signals (like a WiFi channel) are
  spread across multiple nRF24 channels and look "smeared".
- **The nRF24's RSSI is 8-bit (0–255) and not calibrated in
  dBm.** The firmware maps it to a -100 to -40 dBm range for
  display, but the absolute values are approximate (±5 dBm).
- **The waterfall is for visualisation only.** It doesn't
  decode the signals. For protocol-level analysis, capture
  the data with a logic analyser + nRF24, or use an SDR.
- **M0's waterfall is 1 Hz**, not the 10 Hz promised in
  DESIGN §20.5. M1 will upgrade to 10 Hz with binary WebSocket
  frames.

## CLI / web equivalent

The web UI's `/api/attack/start` endpoint:

```json
{
  "module": "spec",
  "method": "2.4ghz"
}
```

Valid `method` values:
- `2.4ghz` — nRF24 sweep
- `subghz` — CC1101 sweep (if available)
- `both` — alternate between the two

For just the data (no UI), the `/api/spectrum?band=2.4ghz`
endpoint returns the current RSSI array as JSON. The
WebSocket `/ws` topic `spectrum` pushes a new frame every
second (M0) or 100 ms (M1).

## Next steps

- For the radio trick behind the RSSI measurement, see
  [nrf24-const-carrier.md](../algorithms/nrf24-const-carrier.md).
- For the channel↔frequency conversion, see
  [channel-math.md](../algorithms/channel-math.md).
- For a deeper look at the waterfall rendering, see
  [DESIGN §20.5](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#205-mitigation-real-time-web-protocol-specification).
- For the legal picture (some jurisdictions require a licence
  even for receive-only operation), see
  [Disclaimer](../legal/disclaimer.md).
