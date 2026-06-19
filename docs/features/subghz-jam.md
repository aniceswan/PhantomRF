# Sub-GHz jam

> Disrupt 433 / 868 / 915 MHz devices using the CC1101 radio.
> Includes spot jam, range jam, hopper, and record-and-replay.

The CC1101's async-serial mode lets the firmware transmit raw
bit patterns without the overhead of the chip's packet
handler. This is the foundation of every sub-GHz attack in
PhantomRF: we can transmit anything from a constant carrier
(spot jam) to a pre-recorded keyfob signal (replay attack).

---

## Table of contents

- [What it does](#what-it-does)
- [Why it works](#why-it-works)
- [Sub-GHz jam modes](#sub-ghz-jam-modes)
- [How to use it](#how-to-use-it)
- [Record and replay](#record-and-replay)
- [What you'll see](#what-youll-see)
- [Limitations](#limitations)
- [CLI / web equivalent](#cli--web-equivalent)
- [Next steps](#next-steps)

---

## What it does

The sub-GHz jam operates on the 300–348 MHz, 387–464 MHz, and
779–928 MHz bands (the CC1101's three supported ranges). It
can:

- **Spot jam:** transmit a constant carrier on a single
  frequency. Knocks out any narrowband receiver on that
  frequency (e.g. a 433 MHz weather station).
- **Range jam:** transmit on a user-defined range of
  frequencies, sweeping through them. Knocks out a band of
  devices.
- **Hopper:** pseudo-randomly hop through a set of
  frequencies, matching the target's hop pattern. Effective
  against simple FHSS devices.
- **Record:** capture the raw demodulated bitstream from a
  target transmission (e.g. a 433 MHz keyfob) and save it to
  flash.
- **Replay:** re-transmit a recorded bitstream at the same
  frequency, simulating the original transmitter.

The most popular use is **record + replay**: capture a 433 MHz
keyfob's "unlock" command, then replay it later to unlock the
door without the keyfob.

## Why it works

Most sub-GHz consumer devices use simple, narrow-band
receivers. A keyfob, for example:

1. Wakes up on a button press
2. Transmits a fixed 32-bit ID at 433.92 MHz, OOK-modulated,
   at 1 kbps
3. Repeats the transmission 5–10 times for redundancy
4. Goes back to sleep

The receiver is always listening. If it sees a transmission
with the matching ID, it acts (unlock the door, open the
gate, etc.). The transmission is **not encrypted** in 99% of
consumer devices — the "security" is that the ID is hard to
guess, not that the signal is hard to forge.

The CC1101's async-serial mode captures and replays the raw
bitstream. The receiver has no way to tell the replay from a
genuine transmission, because the bit pattern is identical.

For the constant-carrier (jam) mode: a strong signal on the
target's frequency desensitises its receiver, so it can't
hear the legitimate transmitter. The receiver's automatic
gain control (AGC) reduces sensitivity in response to the
jam, making the legitimate signal even harder to decode.

## Sub-GHz jam modes

| Mode | What it does | Typical use |
|---|---|---|
| Spot | Single frequency, constant carrier | Disrupt a known device on a known frequency |
| Range | Sweep through a range of frequencies | Disrupt all devices in a band |
| Hopper | Pseudo-random frequency hop | Disrupt simple FHSS devices |
| Record | Capture raw bitstream to PSRAM/flash | Capture a keyfob / sensor signal |
| Replay | Re-transmit a recorded bitstream | Replay a captured signal |

## How to use it

### Spot jam from the web UI

1. Open the **Sub-GHz** page.
2. Set the frequency (e.g. 433.92 MHz for European keyfobs).
3. Set the PA level (0 = 0 dBm, 7 = +12 dBm max).
4. Click **Spot Jam**.

### Range jam from the web UI

1. Open the **Sub-GHz** page.
2. Set the start and stop frequencies (e.g. 433.0 to 434.0
   MHz to cover the European 433 MHz ISM band).
3. Set the dwell time per channel (default 100 ms).
4. Click **Range Jam**.

### From the CLI

```text
phantom> attack subghz
Starting subghz (method=spot)...
Running. Send 'attack stop' to abort.
```

For range jam:

```text
phantom> attack subghz range
```

For hopper:

```text
phantom> attack subghz hopper
```

### From the OLED

1. Use Next to highlight **Sub-GHz Jam** in the main menu.
2. Press OK.
3. Press OK again to start (the OLED doesn't have a frequency
   picker; it uses the last frequency from NVS).

## Record and replay

Recording is the most useful sub-GHz feature. The flow is:

1. **Record:** put the CC1101 in RX mode, sample GDO0 at a
   known interval, save the bits to a `.sub` file on LittleFS.
2. **Replay:** put the CC1101 in TX mode, bit-bang GDO0 with
   the recorded pattern, re-create the original transmission.

### Record from the CLI

```text
phantom> record 433.92
Recording at 433.92 MHz for 1000 ms
Done. Saved to /recordings/2026-06-19_12-34-56_433.92.sub
```

The default duration is 1000 ms; override with the second
argument:

```text
phantom> record 433.92 5000
```

### Replay from the CLI

```text
phantom> replay 433.92
Replaying last recording at 433.92 MHz
```

### Replay from the web UI

1. Open the **Sub-GHz** page.
2. Scroll to the **Recordings** list at the bottom.
3. Pick a recording.
4. Set the replay frequency.
5. Click **Replay**.

The recording file format is the raw bitstream (1 bit per
sampling interval, packed into bytes). File naming convention
is `YYYY-MM-DD_HH-MM-SS_FREQ.sub`. The CC1101 driver knows
how to read these back.

## What you'll see

When a sub-GHz jam is running against a real device:

- **433 MHz keyfob:** the door / gate / car doesn't respond
  to button presses. The receiver is deaf.
- **433 MHz weather station:** the display stops updating
  (the temperature/humidity sensor transmits every 30
  seconds; if the receiver can't hear the transmission, the
  display shows stale data).
- **LoRa device:** the device sees the channel as busy and
  backs off its duty cycle. Packet loss goes from 1% to 100%.
- **Garage door opener:** pressing the remote does nothing
  (or, with a replay attack, the door opens even though you
  didn't press anything).

> :warning: **The replay attack on a keyfob is a real-world
> security issue.** Don't use it on a lock you don't own or
> have explicit authorisation to test. See
> [Disclaimer](../legal/disclaimer.md).

## Limitations

- **Range is much shorter than the nRF24.** The CC1101's
  maximum output power is +12 dBm (16 mW), versus the
  nRF24+PA-LNA's +20 dBm (100 mW). Indoors, expect 10–30 m
  for sub-GHz.
- **The frequency is fixed at order time.** A 433 MHz module
  cannot transmit on 868 MHz. Buy the right variant for your
  region.
- **The CC1101 has a 1 ms PLL re-calibration delay** when
  changing frequency. This limits the sweep rate to about
  1000 channels/second.
- **Modern rolling-code keyfobs (KeeLoq and similar) defeat
  the replay attack.** Each button press uses a fresh code
  derived from a shared secret and a counter; the receiver
  rejects codes it's seen before. The replay still works on
  the older fixed-code keyfobs that are still common in
  cheap devices.
- **Two CC1101s in parallel would help, but the firmware
  supports only one in M0.** M2 will add a second CC1101 for
  dual-band recording.

## CLI / web equivalent

The web UI's `/api/attack/start` endpoint:

```json
{
  "module": "subghz",
  "method": "spot"
}
```

Valid `method` values:
- `spot` — single frequency
- `range` — sweep
- `hopper` — pseudo-random hopping

For record and replay, use the dedicated `/api/record` and
`/api/replay` endpoints (M1), or the CLI `record` / `replay`
commands.

## Next steps

- The async-serial mode is the foundation of every sub-GHz
  attack. See
  [cc1101-async-serial.md](../algorithms/cc1101-async-serial.md)
  for the technical deep dive.
- The channel math (frequency ↔ CC1101 register) is in
  [channel-math.md](../algorithms/channel-math.md).
- The Bluetooth jam (similar in spirit, different in
  mechanism) is in
  [bluetooth-jam.md](bluetooth-jam.md).
- The legal picture is in
  [Disclaimer](../legal/disclaimer.md).
