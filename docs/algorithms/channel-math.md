# Channel math

> All the channel↔frequency conversions in one place, with
> worked examples for every band PhantomRF touches.

This page is a reference for the channel math used by every
radio in the project. The conversions are pure functions (no
state, no I/O) and are implemented in `src/utils/ChannelMath.h`
as `constexpr` so they can be evaluated at compile time.

For the underlying radio tricks, see the other pages in this
section.

---

## Table of contents

- [nRF24 channel ↔ frequency](#nrf24-channel--frequency)
- [CC1101 frequency ↔ register](#cc1101-frequency--register)
- [WiFi channel ↔ nRF24 range](#wifi-channel--nrf24-range)
- [Zigbee channel ↔ nRF24 range](#zigbee-channel--nrf24-range)
- [BLE channel ↔ nRF24 channel](#ble-channel--nrf24-channel)
- [Channel list cheat sheet](#channel-list-cheat-sheet)
- [Worked examples](#worked-examples)
- [C++ helpers](#c-helpers)
- [References](#references)

---

## nRF24 channel ↔ frequency

The nRF24L01 has 126 channels, 0–125, mapping to 2400–2525
MHz in 1 MHz steps.

```
f_MHz = 2400 + ch        (ch in [0, 125])
ch    = f_MHz - 2400
```

| nRF24 channel | Frequency (MHz) | Notes |
|---|---|---|
| 0   | 2400 | Lower edge of 2.4 GHz ISM |
| 12  | 2412 | WiFi channel 1 centre |
| 17  | 2417 | WiFi channel 2 centre |
| 22  | 2422 | WiFi channel 3 centre |
| 27  | 2427 | WiFi channel 4 centre |
| 32  | 2432 | WiFi channel 5 centre |
| 37  | 2437 | WiFi channel 6 centre |
| 42  | 2442 | WiFi channel 7 centre |
| 47  | 2447 | WiFi channel 8 centre |
| 52  | 2452 | WiFi channel 9 centre |
| 57  | 2457 | WiFi channel 10 centre |
| 62  | 2462 | WiFi channel 11 centre |
| 67  | 2467 | WiFi channel 12 centre |
| 72  | 2472 | WiFi channel 13 centre |
| 76  | 2476 | Default PhantomRF drone channel |
| 84  | 2484 | WiFi channel 14 centre (Japan only) |
| 125 | 2525 | Upper edge of nRF24 range |

The nRF24 can technically go up to 2525 MHz, but most
regulatory domains restrict 2.4 GHz operation to 2400–2483.5
MHz. Operating above 2483.5 MHz may be illegal in your
jurisdiction.

## CC1101 frequency ↔ register

The CC1101 has a 24-bit FREQ register, derived from the
target frequency and the 26 MHz crystal.

```
FREQ = (f_Hz * 65536) / 26_000_000
```

The 24-bit value is split into three 8-bit registers:

```
FREQ2 = (FREQ >> 16) & 0xFF
FREQ1 = (FREQ >>  8) & 0xFF
FREQ0 =  FREQ        & 0xFF
```

| Target freq | FREQ register (hex) | FREQ2 | FREQ1 | FREQ0 |
|---|---|---|---|---|
| 315.00 MHz | 0x0C7B96 | 0x0C | 0x7B | 0x96 |
| 433.92 MHz | 0x10B0E0 | 0x10 | 0xB0 | 0xE0 |
| 434.00 MHz | 0x10B20C | 0x10 | 0xB2 | 0x0C |
| 868.35 MHz | 0x2168DD | 0x21 | 0x68 | 0xDD |
| 915.00 MHz | 0x233060 | 0x23 | 0x30 | 0x60 |
| 925.00 MHz | 0x239260 | 0x23 | 0x92 | 0x60 |

> :warning: **Every frequency change needs a PLL
> re-calibration.** This is a separate SPI command
> (`0x3B` followed by `0x3A` for manual calibration) and
> takes ≈ 1 ms. The library handles this when you call
> `setFrequency()`.

## WiFi channel ↔ nRF24 range

WiFi channels are 20 MHz wide (HT20) or 40 MHz wide (HT40).
The PhantomRF nRF24 jammer can only emit a 1-MHz carrier,
so a WiFi channel covers multiple nRF24 channels:

| WiFi channel | Centre freq | nRF24 range (1-MHz carrier) | nRF24 channels |
|---|---|---|---|
| 1  | 2412 MHz | 2401–2423 MHz | 1..23 |
| 2  | 2417 MHz | 2406–2428 MHz | 6..28 |
| 3  | 2422 MHz | 2411–2433 MHz | 11..33 |
| 4  | 2427 MHz | 2416–2438 MHz | 16..38 |
| 5  | 2432 MHz | 2421–2443 MHz | 21..43 |
| 6  | 2437 MHz | 2426–2448 MHz | 26..48 |
| 7  | 2442 MHz | 2431–2453 MHz | 31..53 |
| 8  | 2447 MHz | 2436–2458 MHz | 36..58 |
| 9  | 2452 MHz | 2441–2463 MHz | 41..63 |
| 10 | 2457 MHz | 2446–2468 MHz | 46..68 |
| 11 | 2462 MHz | 2451–2473 MHz | 51..73 |
| 12 | 2467 MHz | 2456–2478 MHz | 56..78 |
| 13 | 2472 MHz | 2461–2483 MHz | 61..83 |
| 14 | 2484 MHz | 2473–2495 MHz | 73..95 |

The "nRF24 range" is `centre ± 11 MHz` (giving a 22-MHz
window that covers the WiFi channel's main lobe and the
adjacent-channel leakage). The nRF24 must sweep through all
of these channels to fully jam a WiFi channel.

For a 40-MHz HT40 WiFi channel, double the range. The sweep
takes 2× as long.

## Zigbee channel ↔ nRF24 range

Zigbee (802.15.4) channels are 2 MHz wide, centred on
`2405 + 5*(N-11)` MHz for N in [11, 26].

| Zigbee channel | Centre freq | nRF24 range | nRF24 channels |
|---|---|---|---|
| 11 | 2405 MHz | 2403–2407 MHz | 3..7 |
| 12 | 2410 MHz | 2408–2412 MHz | 8..12 |
| 13 | 2415 MHz | 2413–2417 MHz | 13..17 |
| 14 | 2420 MHz | 2418–2422 MHz | 18..22 |
| 15 | 2425 MHz | 2423–2427 MHz | 23..27 |
| 16 | 2430 MHz | 2428–2432 MHz | 28..32 |
| 17 | 2435 MHz | 2433–2437 MHz | 33..37 |
| 18 | 2440 MHz | 2438–2442 MHz | 38..42 |
| 19 | 2445 MHz | 2443–2447 MHz | 43..47 |
| 20 | 2450 MHz | 2448–2452 MHz | 48..52 |
| 21 | 2455 MHz | 2453–2457 MHz | 53..57 |
| 22 | 2460 MHz | 2458–2462 MHz | 58..62 |
| 23 | 2465 MHz | 2463–2467 MHz | 63..67 |
| 24 | 2470 MHz | 2468–2472 MHz | 68..72 |
| 25 | 2475 MHz | 2473–2477 MHz | 73..77 |
| 26 | 2480 MHz | 2478–2482 MHz | 78..82 |

Zigbee channels are spaced 5 MHz apart, so nRF24 channels
fall on every other Zigbee channel's edge. The sweep is
fast (5 channels per Zigbee channel).

## BLE channel ↔ nRF24 channel

BLE (Bluetooth Low Energy) uses 40 channels, 0–39, at 2 MHz
spacing from 2402 MHz. The conversion is:

```
bleCh_mHz = 2402 + 2 * bleCh        (bleCh in [0, 39])
nrfCh     = bleCh_mHz - 2400
```

| BLE channel | Frequency (MHz) | nRF24 channel |
|---|---|---|
| 0  | 2402 | 2 |
| 1  | 2404 | 4 |
| 2  | 2406 | 6 |
| ... | ... | ... |
| 37 | 2476 | 76 |
| 38 | 2478 | 78 |
| 39 | 2480 | 80 |

The 3 BLE advertising channels (37, 38, 39) map to nRF24
channels 76, 78, 80.

## Channel list cheat sheet

The PhantomRF firmware ships with a "popular channels" list
for each band. The full list is in `Config.h`:

```cpp
// 2.4 GHz: nRF24 channels for the Bluetooth classic jam
constexpr uint8_t kBtClassicChannels[] = {
    32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8,
    22, 24, 26, 28, 30, 74, 76, 78, 80
};

// 2.4 GHz: nRF24 channels for the WiFi deauth jam
constexpr uint8_t kWifiChannels[] = {
    12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62
};  // WiFi channels 1..11

// Sub-GHz: CC1101 frequencies for the EU ISM jam
constexpr float kEuIsm433Mhz[] = {
    433.92, 434.00, 434.10, 433.05, 433.30
};
```

These lists are "popular" — they cover the most common
devices, not the entire spectrum. For full coverage, the
firmware can be configured to sweep the entire band (slower).

## Worked examples

### Example 1: jam WiFi channel 6

WiFi channel 6 is at 2437 MHz. The nRF24 carrier must
cover 2426–2448 MHz (nRF24 channels 26–48). The sweep:

```cpp
for (uint8_t ch = 26; ch <= 48; ++ch) {
    radio.setChannel(ch);
    delay(2);  // 2 ms dwell
}
```

The full sweep takes 23 × 2 ms = 46 ms.

### Example 2: transmit on 433.92 MHz

The CC1101's FREQ register for 433.92 MHz is 0x10B0E0.
The driver call:

```cpp
radio.setFrequency(433.92);  // M0; chooses the right PA table
```

Internally this computes `(433_920_000 * 65536) / 26_000_000`
= 1_093_856 = 0x10B0E0, then writes it to FREQ2/FREQ1/FREQ0,
then calls `calibrate()`.

### Example 3: convert a BLE advertising channel to nRF24

BLE advertising channel 37 is at 2476 MHz. The nRF24
channel is 76. The driver call:

```cpp
uint8_t nrfCh = bleChannelToNrf24(37);  // returns 76
radio.setChannel(nrfCh);
```

### Example 4: find the nRF24 range for a Zigbee channel

Zigbee channel 15 is at 2425 MHz. The nRF24 range is 23–27.
The driver call:

```cpp
auto range = zigbeeChannelToNrf24Range(15);
// range.start = 23, range.stop = 27
for (uint8_t ch = range.start; ch <= range.stop; ++ch) {
    radio.setChannel(ch);
}
```

## C++ helpers

All of the conversions above are implemented as
`constexpr` functions in `src/utils/ChannelMath.h`. The full
list:

```cpp
namespace phm::util {

// nRF24
constexpr uint16_t nrf24ChannelToFreq(uint8_t ch);
constexpr uint8_t  nrf24FreqToChannel(uint16_t mhz);

// CC1101
constexpr uint32_t cc1101FreqToReg(float mhz);
constexpr float    cc1101RegToFreq(uint32_t reg);

// Cross-radio
constexpr Nrf24Range wifiChannelToNrf24Range(uint8_t wifiCh);
constexpr Nrf24Range zigbeeChannelToNrf24Range(uint8_t zigCh);
constexpr uint8_t    bleChannelToNrf24(uint8_t bleCh);
constexpr uint8_t    nrf24ToBleChannel(uint8_t nrfCh);

}  // namespace phm::util
```

`constexpr` means these can be evaluated at compile time, so
the channel-list arrays in `Config.h` are computed at build
time, not runtime.

## References

- IEEE 802.11-2020 — WiFi channel definitions
- IEEE 802.15.4-2015 — Zigbee channel definitions
- Bluetooth Core Spec 5.3 — BLE channel definitions
- nRF24L01+ Product Spec — nRF24 channel mapping
- CC1101 datasheet (SWRS061I) — CC1101 FREQ register

---

## Next steps

- The constant-carrier trick (the underlying radio abuse
  for 2.4 GHz jamming) is in
  [nrf24-const-carrier.md](nrf24-const-carrier.md).
- The async-serial mode (for sub-GHz record/replay) is in
  [cc1101-async-serial.md](cc1101-async-serial.md).
- The 802.11 deauth frame structure is in
  [802.11-deauth.md](802.11-deauth.md).
