# CC1101 async-serial mode

> The bypass that turns the CC1101 from a packet radio into a
> raw bitstream pipe. Foundation of every sub-GHz attack in
> PhantomRF.

The CC1101 has a normal "packet" mode (preamble, sync word,
length, payload, CRC) and an "async-serial" mode that disables
all of that. In async-serial mode, the raw demodulated
bitstream comes out of GDO0 (in RX) or the chip transmits
whatever you bit-bang on GDO0 (in TX). This page is the
technical deep dive.

For the user-facing documentation (sub-GHz jam, record,
replay), see the [Features](../features/) section. For the
C++ source, see `src/radio/cc1101/` in the repo.

---

## Table of contents

- [Background](#background)
- [What async-serial mode does](#what-async-serial-mode-does)
- [Step-by-step: switch to async-serial](#step-by-step-switch-to-async-serial)
- [Step-by-step: record (RX)](#step-by-step-record-rx)
- [Step-by-step: replay (TX)](#step-by-step-replay-tx)
- [Frequency configuration](#frequency-configuration)
- [PA tables](#pa-tables)
- [Why this works for keyfobs](#why-this-works-for-keyfobs)
- [Limitations](#limitations)
- [C++ implementation notes](#c-implementation-notes)
- [References](#references)

---

## Background

The CC1101's packet mode is designed for protocols like
SimpliciTI, RFStudio, and various industrial sensor
protocols. Every packet has:

- A preamble (alternating 0/1 to let the receiver lock)
- A sync word (a 16- or 32-bit pattern the receiver
  searches for)
- A length byte
- A payload
- A 16-bit CRC

For a keyfob or weather station, this overhead is
**enormous** compared to the actual data:

- Keyfob: 32 bits of ID, repeated 5–10 times. Total: 160
  bits.
- Packet overhead: 8 bits preamble + 16 bits sync + 8 bits
  length + 16 bits CRC = 48 bits.
- Net data rate: 160 / (160 + 48 × 5) = 40% of the channel.

Async-serial mode skips the packet mode entirely. The
modem's demodulator is connected directly to GDO0 (RX) or
the modulator is fed directly from GDO0 (TX). The bit rate
is whatever you bit-bang it at, up to the chip's maximum of
600 kbps.

For PhantomRF, the relevant use cases are:

1. **Record a keyfob's raw bitstream** and save it to flash.
2. **Replay** the bitstream to simulate the keyfob.
3. **Constant carrier** (jam) by holding the modulator in
   one state.

## What async-serial mode does

In async-serial mode:

- **No preamble, no sync word, no length, no CRC.** The
  demodulated bits flow continuously out of GDO0.
- **The chip is "transparent" to the bitstream.** Whatever
  comes in (in RX) or goes out (in TX) is the raw data.
- **The bit rate is determined by the symbol rate
  configured in the MDMCFG registers.** This can be set
  from 0.6 kbaud to 600 kbaud.
- **The modulation format is configured normally.** OOK,
  ASK, GFSK, 2-FSK, 4-FSK, MSK are all supported.

The receive path looks like this:

```
Antenna → RF front-end → Demodulator → GDO0 (pin)
                                   ↑
                                   └── your code samples this
```

The transmit path:

```
GDO0 (pin) → Modulator → RF front-end → Antenna
       ↑
       └── your code drives this
```

## Step-by-step: switch to async-serial

The CC1101 has three registers that control the packet
mode. To switch to async-serial, all three need to be set:

```cpp
// 1. PKTCTRL0: packet format control
//    Bit 0-1: LENGTH_CONFIG
//      00 = fixed
//      01 = variable
//      10 = infinite
//      11 = (reserved)
//    For async-serial, set to 00 (fixed) or 10 (infinite)
radio.setLengthConfig(0);            // fixed, but we'll override

// 2. MDMCFG2: modem configuration
//    Bit 0-1: MOD_FORMAT
//      00 = 2-FSK
//      01 = GFSK
//      10 = (reserved)
//      11 = ASK / OOK
//    Bit 6: MANCHESTER
//    Bit 7: SYNC_MODE
//    For async-serial, set MOD_FORMAT to 11 (OOK) for
//    keyfobs, and SYNC_MODE to 00 (no sync)
radio.setModulation(3);              // OOK
radio.setSyncMode(0);                // no sync word

// 3. MDMCFG3: symbol rate / channel bandwidth
//    This determines the bit rate.
//    For 1 kbps (typical keyfob), use 0x55 or so.
radio.setDRate_easy(1);              // ~1 kbps

// 4. Set the frequency
radio.setFrequency(433.92);          // 433.92 MHz

// 5. Calibrate the PLL (required after any frequency change)
radio.calibrate();
```

The `calibrate()` call is critical. The CC1101's PLL
synthesizer needs to be calibrated for each frequency, and
the calibration takes about 1 ms. Without it, the carrier
will be on the wrong frequency by 10–100 kHz.

After these steps, the chip is in async-serial mode. You're
ready to record or replay.

## Step-by-step: record (RX)

1. Switch to RX mode: `radio.setRxState();`
2. Set GDO0 as an input: `pinMode(GDO0_PIN, INPUT);`
3. Sample GDO0 at the configured bit rate.
4. Pack the bits into bytes (8 bits per byte, MSB first).
5. Write the bytes to a file.

```cpp
constexpr size_t kBufferSize = 32 * 1024;  // 32 KB recording
uint8_t buffer[kBufferSize];
pinMode(GDO0_PIN, INPUT);

for (size_t i = 0; i < kBufferSize; ++i) {
    uint8_t b = 0;
    for (int j = 7; j >= 0; --j) {
        // Sample at the bit rate; assume 1 kbps
        delayMicroseconds(1000);
        bitWrite(b, j, digitalRead(GDO0_PIN));
    }
    buffer[i] = b;
}

// Save to LittleFS
File f = LittleFS.open(filename, "w");
f.write(buffer, kBufferSize);
f.close();
```

The total recording duration is `kBufferSize × 8 / 1000` = 256
seconds for 32 KB at 1 kbps. For higher bit rates, the
recording is shorter.

> :warning: **Sampling timing is everything.** If the
> `delayMicroseconds` is even 10% off, the recorded
> bitstream will be unreadable. Use hardware timers for
> precise sampling in production code.

## Step-by-step: replay (TX)

1. Switch to TX mode: `radio.setTxState();`
2. Set GDO0 as an output: `pinMode(GDO0_PIN, OUTPUT);`
3. Read the recorded file.
4. Bit-bang GDO0 with the recorded bits at the same rate.

```cpp
pinMode(GDO0_PIN, OUTPUT);

for (size_t i = 0; i < kBufferSize; ++i) {
    for (int j = 7; j >= 0; --j) {
        digitalWrite(GDO0_PIN, bitRead(buffer[i], j));
        delayMicroseconds(1000);   // 1 kbps
    }
}
```

The receiver on the other end sees the same bitstream the
keyfob would have sent. It has no way to tell the replay
from the original (for fixed-code devices; rolling-code
devices are immune).

## Frequency configuration

The CC1101's frequency is controlled by the 24-bit FREQ
register:

```
FREQ = (freq_Hz * 65536) / 26_000_000
```

The chip has a 26 MHz crystal, hence the 26_000_000
denominator. The FREQ register is split into three 8-bit
registers:

- `FREQ2` (0x0D) — bits 16–23
- `FREQ1` (0x0E) — bits 8–15
- `FREQ0` (0x0F) — bits 0–7

For 433.92 MHz:

```
FREQ = (433_920_000 * 65536) / 26_000_000
     = 1_093_856
     = 0x10B0E0

FREQ2 = 0x10
FREQ1 = 0xB0
FREQ0 = 0xE0
```

For 868.35 MHz:

```
FREQ = (868_350_000 * 65536) / 26_000_000
     = 2_189_165
     = 0x2168_DD

FREQ2 = 0x21
FREQ1 = 0x68
FREQ0 = 0xDD
```

For 915.0 MHz:

```
FREQ = (915_000_000 * 65536) / 26_000_000
     = 2_306_400
     = 0x2330_60

FREQ2 = 0x23
FREQ1 = 0x30
FREQ0 = 0x60
```

The full table is in [channel-math.md](channel-math.md).

## PA tables

The CC1101's PA (power amplifier) needs a separate
**PA table** for each frequency band. The PA table is a
small lookup at address `0x3E` that maps the desired output
power to the right register values.

The TI reference designs have three standard tables:

| Band | Entries | Use |
|---|---|---|
| 300–348 MHz | 8 | EU 315 MHz keyfobs |
| 387–464 MHz | 8 | EU 433 MHz, US 450 MHz |
| 779–899.99 MHz | 10 | EU 868 MHz, US 900 MHz |
| 900–928 MHz | 10 | US 915 MHz |

Each entry is one byte, written to `0x3E`. The default
tables from TI give output powers from -30 dBm to +12 dBm in
roughly 2 dB steps.

> :warning: **Use the right table for your band.** Loading the
> 433 MHz table when transmitting on 868 MHz gives
> non-compliant output (the harmonics are too high) and
> may damage the chip.

The library handles this automatically when you call
`setFrequency()` — it picks the right table based on the
frequency argument.

## Why this works for keyfobs

A typical 433 MHz keyfob (garage door, gate, car
unlocking):

1. Wakes up on button press.
2. Sets its CC1101 (or equivalent) to a fixed frequency
   (e.g. 433.92 MHz).
3. Transmits a fixed 24- or 32-bit ID, OOK-modulated, at
   ~1 kbps.
4. Repeats the transmission 5–10 times.
5. Goes back to sleep. Total airtime: 100–200 ms.

The receiver (in the garage door opener, for example):

1. Always listening on 433.92 MHz.
2. Looks for the sync pattern (or, in fixed-key devices, the
   ID directly).
3. If the ID matches, acts (open the door, unlock the car).

The transmission is **not encrypted** in the vast majority
of consumer devices. The "security" is that the ID is hard
to guess, not that the signal is hard to forge.

Async-serial replay:

1. Capture the bitstream with PhantomRF.
2. Save it to a `.sub` file.
3. Replay the same bitstream at the same frequency.

The receiver cannot tell the replay from the original. It
sees the same bit pattern on the same frequency, so it acts.

> :warning: **Modern rolling-code devices defeat this.** The
> receiver stores a counter, and each transmission must
> have a higher counter than the last. The replay has the
> same counter, so the receiver rejects it. This is the
> basis of KeeLoq and similar systems. The replay attack
> works on older fixed-code devices, which are still
> extremely common.

## Limitations

- **Bit rate is limited by the symbol rate.** The CC1101
  maxes out at 600 kbaud. For higher rates, you'd need a
  different chip.
- **No error correction.** If the bitstream gets corrupted
  in transit (interference, weak signal), the receiver
  just sees a wrong bit. There's no CRC, no retries.
- **PLL re-calibration is slow.** Every frequency change
  needs a calibration, which takes ≈ 1 ms. The sweep rate
  is therefore limited to ≈ 1000 channels/second.
- **The CC1101 has three supported frequency ranges** with
  two gaps. See [cc1101.md](../hardware/cc1101.md#frequency-bands).
- **No support for frequency-hopping protocols out of the
  box.** Async-serial mode assumes a single fixed frequency.
  A frequency-hopping keyfob would require manual hop
  sequence.

## C++ implementation notes

The PhantomRF code wraps async-serial in two classes in
`src/radio/cc1101/`:

```cpp
class Cc1101AsyncRx {
public:
    void begin(uint8_t csnPin, uint8_t gdo0Pin);
    void setFrequency(float mhz);
    void startRecording();            // GDO0 → input
    void stopRecording();
    size_t readBytes(uint8_t* dst, size_t maxLen);
private:
    SPIClass* spi_;
    uint8_t   csnPin_, gdo0Pin_;
    bool      recording_ = false;
};

class Cc1101AsyncTx {
public:
    void begin(uint8_t csnPin, uint8_t gdo0Pin);
    void setFrequency(float mhz);
    void startReplay();              // GDO0 → output
    void stopReplay();
    void writeBytes(const uint8_t* src, size_t len);
};
```

Both classes share the calibration step. The TX class also
has a `setOutputPower(uint8_t level)` method that writes the
PA table for the current band.

## References

- [CC1101 datasheet (SWRS061I)](https://www.ti.com/lit/ds/symlink/cc1101.pdf) — Texas Instruments
- [SmartRC-CC1101-Driver-Lib](https://github.com/LSatan/SmartRC-CC1101-Driver-Lib) — the open-source driver we use
- DESIGN.md §10.2 — the design doc's version of this page

---

## Next steps

- The Bluetooth jam uses the nRF24, not the CC1101, but the
  underlying idea (force a radio to do something it wasn't
  designed for) is the same. See
  [nrf24-const-carrier.md](nrf24-const-carrier.md).
- The 802.11 deauth is yet another approach: instead of a
  constant carrier, send forged management frames. See
  [802.11-deauth.md](802.11-deauth.md).
- The channel↔register conversion is in
  [channel-math.md](channel-math.md).
