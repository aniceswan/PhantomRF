# Bluetooth classic jam

> Disrupts Bluetooth classic (BR/EDR) connections within a 30 m
> radius. The first attack ever implemented in PhantomRF.

Bluetooth classic operates in the same 2.4 GHz ISM band as WiFi
and Zigbee. Unlike BLE, classic BT uses 79 channels of 1 MHz
each, hopping at 1600 hops/second. Jamming it requires a
broadband 2.4 GHz interferer — the nRF24 constant carrier, in
constant mode or in fast-hop mode, does the job.

---

## Table of contents

- [What it does](#what-it-does)
- [Why it works](#why-it-works)
- [How to use it](#how-to-use-it)
- [What you'll see](#what-youll-see)
- [Limitations](#limitations)
- [CLI / web equivalent](#cli--web-equivalent)
- [Next steps](#next-steps)

---

## What it does

The Bluetooth jam transmits a constant or hopping carrier on
the 2.4 GHz band. Any Bluetooth classic connection within
range will see the carrier as noise and either:

- Drop the connection immediately (the master polls the slave
  every 1.25 ms; if the slave misses three polls, it
  disconnects)
- Reduce the data rate dramatically (the BT adaptive
  frequency-hopping algorithm will pick the quietest channels,
  but if the entire band is jammed, it has nowhere to go)
- Get stuck in a "reconnect" loop (the master and slave
  continuously try to reconnect, fail, try again)

In practice, you'll see the connected device fall off the
network within 1–2 seconds of starting the jam, and
reconnection attempts every 5–10 seconds.

## Why it works

Bluetooth classic uses **adaptive frequency hopping (AFH)** to
avoid interference. The master classifies each channel as
"good", "bad", or "unknown" based on packet error rates, and
the hop set is restricted to the good channels.

The nRF24 constant carrier, however, occupies **a single 1 MHz
channel** at a time. To make the entire 79-channel BT band
unusable, we need to:

- **Sweep** through all 79 channels fast enough that the AFH
  algorithm doesn't have time to re-classify them
- **Transmit continuously** on each channel for a few
  milliseconds

The PhantomRF firmware does both:

```cpp
// Pseudo-code from modules/btjam/BtJam.cpp (M0)
constexpr uint8_t kBtChannels[] = {
    32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8,
    22, 24, 26, 28, 30, 74, 76, 78, 80
};
// (subset — most popular; full 79-channel set is in DESIGN §10.4)

void loop() {
    for (uint8_t ch : kBtChannels) {
        radio.setChannel(ch);
        radio.write(dummy, 32);
        // set CONT_WAVE | PLL_LOCK in RF_SETUP
        delay(2);  // 2 ms per channel
    }
}
```

The 2 ms dwell is short enough that the BT AFH algorithm
can't keep up. Even if the connection re-establishes on a
"good" channel, we'll be there in less than 200 ms (the
full sweep takes 20 channels × 2 ms = 40 ms).

## How to use it

### From the web UI

1. Connect to the PhantomRF AP and open the dashboard.
2. Click **Bluetooth Jam** in the sidebar.
3. (Optional) Set the dwell time per channel. Default is 2 ms.
4. Click **Start**.

### From the CLI

```text
phantom> attack bt
Starting bt (method=default)...
Running. Send 'attack stop' to abort.
phantom>
```

To stop:

```text
phantom> attack stop
Stopped.
```

### From the OLED

1. Use Next to highlight **BT Jam** in the main menu.
2. Press OK to enter the submenu.
3. Press OK again to start the attack.
4. Long-press OK to stop and return to the main menu.

## What you'll see

When the jam is running, devices within range will:

- **Bluetooth headphones:** audio cuts out, "device
  disconnected" notification appears, attempts to reconnect
  every 5–10 seconds.
- **Bluetooth speakers:** same as headphones.
- **Wireless keyboard/mouse:** unresponsive for 1–2 seconds
  at a time, may disconnect entirely.
- **Smart watches:** lose the phone connection, fall back to
  on-device mode.
- **Car infotainment:** the connected phone drops, may show
  "no media" or "no phone connected".

Devices outside the jam's range (or in a shielded enclosure)
are unaffected.

## Limitations

- **The PhantomRF AP is in the 2.4 GHz band.** If the
  PhantomRF is in AP mode, the jam will also disrupt the
  PhantomRF's own web UI. See
  [DESIGN §20.1](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#201-mitigation-self-jamming--rf-co-existence)
  for the auto-degradation rules.
- **Range is line-of-sight.** Indoors, expect 20–30 m.
  Outdoors with a Yagi antenna, 100+ m is possible.
- **Modern BT 5.x has "channel selection algorithm #2"** that
  makes hopping more robust. The jam is less effective against
  BT 5.x than against BT 4.x and earlier.
- **No effect on BLE** (Bluetooth Low Energy). BLE has only
  40 channels and uses a different hopping pattern. For BLE,
  see [BLE spam](../features/wifi-deauth.md) (M1) or the
  dedicated [BLE jam page](#) (M0 placeholder).

## CLI / web equivalent

Both the web UI and the CLI call the same `attack bt` command
under the hood. The web UI's `/api/attack/start` endpoint
accepts:

```json
{
  "module": "bt",
  "method": "default"
}
```

The `method` field can be:
- `default` — sweep the 20 most common channels, 2 ms dwell
- `full` — sweep all 79 channels, 1 ms dwell (heavier load)
- `spot` — jam a single channel (the user picks it)

## Next steps

- Want to see the spectrum? The
  [Spectrum analyzer](spectrum-analyzer.md) page walks you
  through it.
- The WiFi deauth attack is the most popular one in this
  family. See [wifi-deauth.md](wifi-deauth.md).
- For the underlying radio trick, see
  [nrf24-const-carrier.md](../algorithms/nrf24-const-carrier.md).
- For the legal picture, see
  [Disclaimer](../legal/disclaimer.md).
