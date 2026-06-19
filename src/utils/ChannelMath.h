/**
 * @file ChannelMath.h
 * @brief Pure channel ↔ frequency conversion helpers
 *
 * All functions in this header are `constexpr` (or trivial) and have
 * no side effects. They are shared by every radio module so the
 * mapping tables live in exactly one place.
 *
 * Conventions:
 *   - nRF24 channel N → (2400 + N) MHz, N in [0, 125]
 *   - CC1101 frequency F (Hz) → register = F * 65536 / 26_000_000 (26 MHz crystal)
 *   - WiFi channel N → (2412 + 5*(N-1)) MHz, N in [1, 14] (ch 14 = 2484 MHz)
 *   - Zigbee channel N → (2405 + 5*(N-11)) MHz, N in [11, 26]
 *   - BLE channel N → (2402 + 2*N) MHz, N in [0, 39]
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

namespace phm::util {

/**
 * @brief Inclusive range of nRF24 channels that covers a given band
 *
 * Returned by `wifiChannelToNrf24Range()` and `zigbeeChannelToNrf24Range()`.
 * `start` is always <= `stop`; both are clamped to [0, 125].
 */
struct Nrf24Range {
    uint8_t start;  ///< First channel in the range (inclusive)
    uint8_t stop;   ///< Last channel in the range (inclusive)
};

/**
 * @brief Convert an nRF24 channel to its centre frequency
 *
 * @param ch Channel number, 0..125
 * @return Centre frequency in MHz (2400..2525)
 */
constexpr uint16_t nrf24ChannelToFreq(uint8_t ch) {
    return static_cast<uint16_t>(2400u + static_cast<uint16_t>(ch));
}

/**
 * @brief Convert a frequency to the nearest nRF24 channel
 *
 * @param mhz Centre frequency in MHz
 * @return Nearest channel in 0..125, clamped
 */
constexpr uint8_t nrf24FreqToChannel(uint16_t mhz) {
    return (mhz <= 2400u) ? 0 : (mhz >= 2525u) ? 125 : static_cast<uint8_t>(mhz - 2400u);
}

/**
 * @brief Compute the CC1101 FREQ register value for a given frequency
 *
 * The CC1101 has a 26 MHz crystal and a 24-bit FREQ register:
 *   FREQ = (freq_Hz * 65536) / 26_000_000
 *
 * @param mhz Centre frequency in MHz
 * @return 24-bit FREQ register value
 */
constexpr uint32_t cc1101FreqToReg(float mhz) {
    // 65536 / 26000000 ≈ 0.00252
    return static_cast<uint32_t>(mhz * 1000000.0f * 65536.0f / 26000000.0f) & 0x00FFFFFFu;
}

/**
 * @brief Convert a CC1101 FREQ register value back to MHz
 *
 * @param reg 24-bit FREQ register value
 * @return Centre frequency in MHz
 */
constexpr float cc1101RegToFreq(uint32_t reg) {
    return (static_cast<float>(reg & 0x00FFFFFFu) * 26000000.0f) / (65536.0f * 1000000.0f);
}

/**
 * @brief Range of nRF24 channels that covers a WiFi channel
 *
 * WiFi channels are 22 MHz wide (per DESIGN §20.5 test expectations).
 * The returned range is clamped to the valid nRF24 band.
 *
 * @param wifiCh WiFi channel 1..14
 * @return Inclusive range of nRF24 channels
 */
inline Nrf24Range wifiChannelToNrf24Range(uint8_t wifiCh) {
    if (wifiCh < 1u)
        wifiCh = 1u;
    if (wifiCh > 14u)
        wifiCh = 14u;
    uint16_t center = (wifiCh == 14u) ? uint16_t(2484u) : uint16_t(2412u + 5u * (wifiCh - 1u));
    int start = int(center) - 11 - 2400;
    int stop = int(center) + 11 - 2400;
    if (start < 0)
        start = 0;
    if (stop > 125)
        stop = 125;
    return {uint8_t(start), uint8_t(stop)};
}

/**
 * @brief Range of nRF24 channels that covers a Zigbee channel
 *
 * Zigbee (802.15.4) channels are 2 MHz wide, centred on
 * `2405 + 5*(N-11)` MHz for N in [11, 26].
 *
 * @param zigCh Zigbee channel 11..26
 * @return Inclusive range of nRF24 channels
 */
inline Nrf24Range zigbeeChannelToNrf24Range(uint8_t zigCh) {
    if (zigCh < 11u)
        zigCh = 11u;
    if (zigCh > 26u)
        zigCh = 26u;
    uint16_t center = uint16_t(2405u + 5u * (zigCh - 11u));
    int start = int(center) - 1 - 2400;
    int stop = int(center) + 1 - 2400;
    if (start < 0)
        start = 0;
    if (stop > 125)
        stop = 125;
    return {uint8_t(start), uint8_t(stop)};
}

/**
 * @brief Convert a BLE advertising/data channel to its nRF24 equivalent
 *
 * BLE channel N is at (2402 + 2N) MHz. nRF24 channel = freq - 2400.
 *
 * @param bleCh BLE channel 0..39
 * @return nRF24 channel (always in [0, 125])
 */
constexpr uint8_t bleChannelToNrf24(uint8_t bleCh) {
    return (bleCh > 39u) ? uint8_t(80) : uint8_t(2402u + 2u * uint16_t(bleCh) - 2400u);
}

/**
 * @brief Convert an nRF24 channel to the nearest BLE channel
 *
 * Returns 0xFF if there is no clean mapping (i.e. the nRF24 channel
 * does not land on a 2-MHz BLE grid).
 *
 * @param nrfCh nRF24 channel 0..125
 * @return BLE channel 0..39, or 0xFF if not on grid
 */
inline uint8_t nrf24ToBleChannel(uint8_t nrfCh) {
    if (nrfCh > 125u)
        nrfCh = 125u;
    int offset = int(nrfCh) - 2;
    if (offset < 0 || (offset % 2) != 0)
        return 0xFFu;
    int ble = offset / 2;
    if (ble < 0 || ble > 39)
        return 0xFFu;
    return uint8_t(ble);
}

}  // namespace phm::util
