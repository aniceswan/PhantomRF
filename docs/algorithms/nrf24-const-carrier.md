# nRF24 constant carrier

> The trick that makes every 2.4 GHz attack in PhantomRF
> possible. A 17-year-old Nordic Semiconductor undocumented
> feature, reverse-engineered and put to work.

This page is the deep technical dive into the nRF24L01's
"constant carrier" mode. For the user-facing documentation
(Bluetooth jam, WiFi deauth, etc.), see the
[Features](../features/) section. For the C++ source, see
`src/radio/nrf24/` in the repo.

---

## Table of contents

- [Background](#background)
- [The test-mode register](#the-test-mode-register)
- [Step-by-step procedure](#step-by-step-procedure)
- [Why the carrier works](#why-the-carrier-works)
- [Limitations](#limitations)
- [Channel math](#channel-math)
- [Sweep strategy](#sweep-strategy)
- [C++ implementation notes](#c-implementation-notes)
- [References](#references)

---

## Background

The nRF24L01 has a documented "test mode" intended for
factory calibration. Two bits in the `RF_SETUP` register
control it:

- `CONT_WAVE` (bit 7): forces the synthesizer to keep the
  PLL locked at the channel frequency
- `PLL_LOCK` (bit 4): keeps the PLL in a constant-output
  state

When both are set, the chip stops trying to transmit packets
and instead emits a **continuous unmodulated carrier** on the
selected channel. The carrier is centred on `f = 2400 + ch`
MHz, where `ch` is the channel register (0–125).

This is useful for factory calibration (you set the
transmitter to a known frequency and measure the output with a
spectrum analyser). It is **not** documented as a user
feature. Nordic's product spec describes it as a "test mode
that should not be used in production firmware".

But the chip doesn't know what production firmware is. The
bits are there. The carrier is there. The receiver on the
other end doesn't care.

## The test-mode register

`RF_SETUP` is at address `0x06`. The relevant bits:

```
   7     6     5     4     3     2     1     0
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ CONT│     │ RF  │ PLL │ RF  │ RF  │  RF_DR  │
│ WAVE│reserved│ PWR│LOCK │ DR_HIGH│ DR_LOW │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
  0x80                              0x08    0x01
```

The default value of `RF_SETUP` after power-up is `0x0F` (2 Mbps,
0 dBm). To enable constant carrier:

```cpp
uint8_t setup = radio.getRFSetup();      // read current value
setup |= 0x80;                            // CONT_WAVE
setup |= 0x10;                            // PLL_LOCK
radio.setRFSetup(setup);                 // write back
```

The 0x80 and 0x10 values are written to the chip as a
register, not as part of the SPI command. The
`RF24::setRFSetup()` method handles the SPI write.

## Step-by-step procedure

The full procedure, as implemented in PhantomRF:

```cpp
// 1. Initialize the nRF24 normally
radio.begin();
radio.setAutoAck(false);
radio.setPALevel(RF24_PA_MAX);            // 0 dBm
radio.setDataRate(RF24_2MBPS);
radio.setCRCLength(RF24_CRC_DISABLED);
radio.setAddressWidth(3);                 // smaller addr = faster TX
radio.setRetries(0, 0);

// 2. Stop listening, set channel
radio.stopListening();
radio.setChannel(channel);                // 0..125

// 3. Push a 32-byte dummy payload (required for the radio to enter TX)
uint8_t dummy[32];
memset(dummy, 0xFF, 32);
radio.write(dummy, 32);                   // returns true on success

// 4. Read RF_SETUP, set CONT_WAVE and PLL_LOCK, write back
uint8_t setup = SPI_read(RF_SETUP);
setup |= 0x80 | 0x10;                     // CONT_WAVE | PLL_LOCK
SPI_write(RF_SETUP, setup);

// 5. Pulse CE high to start transmission
digitalWrite(CE_PIN, HIGH);
```

The carrier is now emitting on `2400 + channel` MHz. To
change channel, just write a new value to `RF_CH`:

```cpp
radio.setChannel(newChannel);
```

The PLL re-locks in about 120 µs. There is a brief "chirp"
during the re-lock — receivers see this as a 1-sample blip
on the spectrum.

To stop the carrier, clear the bits and lower CE:

```cpp
setup &= ~(0x80 | 0x10);
SPI_write(RF_SETUP, setup);
digitalWrite(CE_PIN, LOW);
```

## Why the carrier works

A receiver on (or near) the carrier frequency sees the
constant signal as a strong, narrow-band interferer. Three
effects combine to disrupt reception:

1. **AGC desensitisation.** The receiver's automatic gain
   control reduces the front-end gain to keep the ADC in
   range. This makes the receiver "deaf" to weaker signals
   in the same band.

2. **ADC saturation.** A strong carrier can push the ADC
   into saturation. The demodulated output is clipped, and
   legitimate signals can't be recovered.

3. **Bit error rate.** Even if the ADC doesn't saturate, the
   carrier adds noise to the received signal. The bit error
   rate goes from < 1% to 50–100%, depending on the
   legitimate signal's strength.

For narrowband receivers (nRF24, BLE, Zigbee), a single
nRF24's carrier is enough to disrupt reception across a
20-MHz window (the receiver's IF bandwidth). For wideband
receivers (WiFi), the disruption is limited to the 1-MHz
channel the carrier is on; the WiFi receiver's other 19 MHz
are unaffected. This is why PhantomRF uses **multiple
nRF24s** in M2 to cover a WiFi channel completely.

## Limitations

- **Single channel at a time.** The carrier is on exactly one
  frequency. To "jam" multiple channels, you have to sweep.
  Sweeping takes time (120 µs PLL re-lock + 100 µs settling),
  and the nRF24's RF front-end is busy while re-locking. The
  practical sweep rate is 2000–5000 channels/second.

- **Unmodulated carrier.** The carrier has no modulation. It
  works well against narrowband receivers (which expect
  OOK/FSK in a 1-MHz channel) and less well against
  wideband receivers (which integrate over a 20-MHz channel
  and ignore narrow spikes).

- **PA-LNA's PA can overheat.** At +20 dBm CW, the PA chip
  dissipates about 0.5 W. Without a heatsink, the chip hits
  80 °C within 3 minutes and the firmware auto-throttles. See
  [Power budget](../hardware/power-budget.md#thermal).

- **PLL lock time limits the sweep rate.** The PLL takes
  ~120 µs to re-lock after a channel change. Below 5000
  channels/second, the receiver has time to recover between
  sweeps. Above 10 000 channels/second, the chirps during
  re-lock become significant.

## Channel math

The nRF24 channel register is 0–125, mapping to frequencies
of 2400–2525 MHz. The conversion is:

```
f_MHz = 2400 + channel
```

Or in reverse:

```
channel = f_MHz - 2400
```

This is straightforward but easy to get wrong when you're
thinking in terms of WiFi channels. The WiFi channel 1 is
at 2412 MHz, which is nRF24 channel 12. The full mapping is
in [channel-math.md](channel-math.md).

The 1-MHz channel spacing of the nRF24 is **coarser** than
the 5-MHz spacing of WiFi. A WiFi channel 1 transmission
(at 2412 MHz) shows up as a bright spot on nRF24 channel 12
and a slightly dimmer spot on channels 11 and 13.

## Sweep strategy

The PhantomRF firmware uses a **deterministic sweep** through
the channel list:

1. Start at the lowest channel (0).
2. Set the channel, wait 100 µs for PLL lock.
3. Read RSSI (for the spectrum analyzer) or hold for 2 ms
   (for the jam).
4. Increment channel.
5. Repeat until the highest channel (125).
6. Start over.

The dwell time per channel is configurable:

| Dwell | Effect |
|---|---|
| 100 µs | Continuous jam, 1000 channels/second. Light load on the chip. |
| 1 ms | Standard jam, 1000 channels/second. |
| 2 ms | Heavy jam, the receiver has time to "give up" and disconnect. Default for Bluetooth. |
| 10 ms | Slow jam, good for spectrum analysis. |

For a Bluetooth jam, the firmware sweeps the 20 most common
BT channels (covering ≈ 90% of the BT band) at 2 ms dwell.
The full sweep takes 40 ms, which is faster than the BT
adaptive frequency hopping algorithm can react.

## C++ implementation notes

The PhantomRF code wraps the constant carrier in a `Jam` class
in `src/radio/nrf24/Nrf24Jam.cpp`. The relevant methods:

```cpp
class Nrf24Jam {
public:
    void begin(uint8_t cePin, uint8_t csnPin);
    void setChannel(uint8_t ch);
    void startCarrier();          // sets CONT_WAVE | PLL_LOCK
    void stopCarrier();           // clears them
    void setPaLevel(uint8_t level);  // 0..3
    int8_t readRssi();            // last RSSI sample
private:
    RF24 radio_;
    uint8_t cePin_, csnPin_;
    bool    carrierOn_ = false;
};
```

The `startCarrier()` method is the one that calls the
trick described above. It's used by:

- `modules/btjam/BtJam.cpp` — Bluetooth jam
- `modules/wifi/WifiDeauth.cpp` — sometimes (in addition to
  the standard ESP-IDF deauth path)
- `modules/dronejam/DroneJam.cpp` — drone-band jam
- `modules/spectrum/SpectrumAnalyzer.cpp` — RSSI sampling
  (carrier is off, just sweeping channels in RX)

The `readRssi()` method returns the nRF24's 8-bit RSSI
register, mapped to a -100 to -40 dBm range. The mapping is
approximate (±5 dBm) and is calibrated in production by
measuring a known signal source.

## References

- [nRF24L01+ Product Specification](https://www.nordicsemi.com/Products/Low-power-short-range-wireless/nRF24-series) — Nordic Semiconductor
- [Bastille Research: nRF24L01+ constant carrier abuse](https://github.com/BastilleResearch/nrf24-research) — the original public disclosure
- [W0rthlessS0ul/nRF24_jammer](https://github.com/W0rthlessS0ul/nRF24_jammer) — the lineage project that put this into practice
- DESIGN.md §10.1 — the design doc's version of this page

---

## Next steps

- The same trick, applied to the CC1101, is in
  [cc1101-async-serial.md](cc1101-async-serial.md).
- The WiFi-specific 802.11 trick (different radio, different
  approach) is in [802.11-deauth.md](802.11-deauth.md).
- The channel math (nRF24 ↔ WiFi ↔ BLE) is in
  [channel-math.md](channel-math.md).
