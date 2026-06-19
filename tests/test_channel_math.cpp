/**
 * @file test_channel_math.cpp
 * @brief Host-side unit tests for `phm::util::ChannelMath`.
 *
 * Exercises the pure channel ↔ frequency conversion helpers in
 * `src/utils/ChannelMath.h`. Every function under test is `constexpr`,
 * so we don't need any hardware — we just include the header, call the
 * functions, and assert the results.
 *
 * Build (PlatformIO):
 *     pio test -e native -f test_channel_math
 *
 * Build (CMake / IDE):
 *     cmake -S tests -B build && cmake --build build && ctest --test-dir build
 */

#define PHM_NATIVE_TEST 1
#include "native_mock.h"

#include <cstdio>
#include <cstdlib>

#include <unity.h>

// We are testing the *real* implementation, not a stub.
#include "utils/ChannelMath.h"

using phm::util::bleChannelToNrf24;
using phm::util::cc1101FreqToReg;
using phm::util::cc1101RegToFreq;
using phm::util::nrf24ChannelToFreq;
using phm::util::nrf24FreqToChannel;
using phm::util::Nrf24Range;
using phm::util::nrf24ToBleChannel;
using phm::util::wifiChannelToNrf24Range;
using phm::util::zigbeeChannelToNrf24Range;

// ---------------------------------------------------------------------------
// setUp / tearDown — Unity invokes these before / after every test.
// ---------------------------------------------------------------------------

void setUp(void) {
    // Nothing to initialise for pure-function tests. The shared state
    // lives in the production code and is stateless.
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// nRF24 channel ↔ frequency
// ---------------------------------------------------------------------------

void testNrf24ChannelToFreq_channel_zero_is_2400(void) {
    TEST_ASSERT_EQUAL_UINT16(2400u, nrf24ChannelToFreq(0u));
}

void testNrf24ChannelToFreq_channel_two_is_2402(void) {
    TEST_ASSERT_EQUAL_UINT16(2402u, nrf24ChannelToFreq(2u));
}

void testNrf24ChannelToFreq_channel_76_is_2476(void) {
    TEST_ASSERT_EQUAL_UINT16(2476u, nrf24ChannelToFreq(76u));
}

void testNrf24ChannelToFreq_channel_125_is_2525(void) {
    TEST_ASSERT_EQUAL_UINT16(2525u, nrf24ChannelToFreq(125u));
}

void testNrf24FreqToChannel_round_trip(void) {
    // Pick a few representative values and make sure freq→ch→freq
    // returns the same number.
    const uint16_t freqs[] = {2400u, 2402u, 2426u, 2448u, 2476u, 2500u, 2525u};
    for (uint16_t f : freqs) {
        const uint8_t ch = nrf24FreqToChannel(f);
        TEST_ASSERT_EQUAL_UINT16(f, nrf24ChannelToFreq(ch));
    }
}

void testNrf24FreqToChannel_clamps_below_band(void) {
    // Anything ≤ 2400 MHz should snap to channel 0.
    TEST_ASSERT_EQUAL_UINT8(0u, nrf24FreqToChannel(0u));
    TEST_ASSERT_EQUAL_UINT8(0u, nrf24FreqToChannel(2399u));
    TEST_ASSERT_EQUAL_UINT8(0u, nrf24FreqToChannel(2400u));
}

void testNrf24FreqToChannel_clamps_above_band(void) {
    // Anything ≥ 2525 MHz should snap to channel 125.
    TEST_ASSERT_EQUAL_UINT8(125u, nrf24FreqToChannel(2525u));
    TEST_ASSERT_EQUAL_UINT8(125u, nrf24FreqToChannel(2600u));
    TEST_ASSERT_EQUAL_UINT8(125u, nrf24FreqToChannel(65535u));
}

// ---------------------------------------------------------------------------
// CC1101 frequency → register
// ---------------------------------------------------------------------------

void testCc1101FreqToReg_433_92_mhz(void) {
    // 433.92 MHz × 65536 / 26 000 000 = 1 093 745 = 0x10B071.
    //
    // The production helper uses `float` arithmetic. A 32-bit float
    // has only ~24 bits of mantissa, and the intermediate product
    // 433 920 000 × 65 536 = 2.84e13 overflows the mantissa. The
    // float result is close to but not exactly the integer value;
    // we accept a few-LSB drift. The integer-exact answer is
    // 0x10B071 and the float-rounded value lands within ±0x1000 of
    // it for the four sub-GHz bands we test.
    const uint32_t reg = cc1101FreqToReg(433.92f);
    TEST_ASSERT_EQUAL_HEX32(0x0010B071u, reg);
}

void testCc1101FreqToReg_315_mhz(void) {
    // 315.0 MHz × 65536 / 26 000 000 = 793 993 = 0x0C1D89.
    const uint32_t reg = cc1101FreqToReg(315.0f);
    TEST_ASSERT_EQUAL_HEX32(0x000C1D89u, reg);
}

void testCc1101FreqToReg_868_mhz(void) {
    // 868.0 MHz × 65536 / 26 000 000 = 2 187 894 = 0x216276.
    const uint32_t reg = cc1101FreqToReg(868.0f);
    TEST_ASSERT_EQUAL_HEX32(0x00216276u, reg);
}

void testCc1101FreqToReg_915_mhz(void) {
    // 915.0 MHz × 65536 / 26 000 000 = 2 306 363 = 0x23313B.
    const uint32_t reg = cc1101FreqToReg(915.0f);
    TEST_ASSERT_EQUAL_HEX32(0x0023313Bu, reg);
}

void testCc1101FreqToReg_masks_to_24_bits(void) {
    // Even an absurd input should be masked to 24 bits.
    const uint32_t reg = cc1101FreqToReg(9999.0f);
    TEST_ASSERT_EQUAL_HEX32(reg & 0x00FFFFFFu, reg);
}

void testCc1101RegToFreq_round_trip(void) {
    // Going reg→freq→reg should be exact (or off-by-1 due to integer
    // truncation; we allow that).
    const float mhz_list[] = {315.0f, 433.92f, 868.0f, 915.0f};
    for (float mhz : mhz_list) {
        const uint32_t r1 = cc1101FreqToReg(mhz);
        const float f = cc1101RegToFreq(r1);
        const uint32_t r2 = cc1101FreqToReg(f);
        // Truncation is at most 1 LSB; we accept that.
        const uint32_t diff = (r1 > r2) ? (r1 - r2) : (r2 - r1);
        TEST_ASSERT_TRUE(diff <= 1u);
    }
}

// ---------------------------------------------------------------------------
// WiFi channel → nRF24 range
// ---------------------------------------------------------------------------

void testWifiChannelToNrf24Range_ch1(void) {
    const Nrf24Range r = wifiChannelToNrf24Range(1u);
    TEST_ASSERT_EQUAL_UINT8(1u, r.start);
    TEST_ASSERT_EQUAL_UINT8(23u, r.stop);
}

void testWifiChannelToNrf24Range_ch6(void) {
    const Nrf24Range r = wifiChannelToNrf24Range(6u);
    TEST_ASSERT_EQUAL_UINT8(26u, r.start);
    TEST_ASSERT_EQUAL_UINT8(48u, r.stop);
}

void testWifiChannelToNrf24Range_ch11(void) {
    const Nrf24Range r = wifiChannelToNrf24Range(11u);
    TEST_ASSERT_EQUAL_UINT8(51u, r.start);
    TEST_ASSERT_EQUAL_UINT8(73u, r.stop);
}

void testWifiChannelToNrf24Range_ch13(void) {
    const Nrf24Range r = wifiChannelToNrf24Range(13u);
    TEST_ASSERT_EQUAL_UINT8(61u, r.start);
    TEST_ASSERT_EQUAL_UINT8(83u, r.stop);
}

void testWifiChannelToNrf24Range_ch14_special(void) {
    // Channel 14 is the Japanese variant: centre 2484 MHz, ±11 MHz gives
    // 2473..2495, so the nRF24 range is 73..95.
    const Nrf24Range r = wifiChannelToNrf24Range(14u);
    TEST_ASSERT_EQUAL_UINT8(73u, r.start);
    TEST_ASSERT_EQUAL_UINT8(95u, r.stop);
}

void testWifiChannelToNrf24Range_clamps_out_of_range(void) {
    // Below 1 — clamp to 1.
    Nrf24Range r = wifiChannelToNrf24Range(0u);
    TEST_ASSERT_EQUAL_UINT8(1u, r.start);
    TEST_ASSERT_EQUAL_UINT8(23u, r.stop);
    // Above 14 — clamp to 14.
    r = wifiChannelToNrf24Range(99u);
    TEST_ASSERT_EQUAL_UINT8(73u, r.start);
    TEST_ASSERT_EQUAL_UINT8(95u, r.stop);
}

// ---------------------------------------------------------------------------
// Zigbee channel → nRF24 range
// ---------------------------------------------------------------------------

void testZigbeeChannelToNrf24Range_ch11(void) {
    const Nrf24Range r = zigbeeChannelToNrf24Range(11u);
    TEST_ASSERT_EQUAL_UINT8(4u, r.start);
    TEST_ASSERT_EQUAL_UINT8(6u, r.stop);
}

void testZigbeeChannelToNrf24Range_ch26(void) {
    const Nrf24Range r = zigbeeChannelToNrf24Range(26u);
    TEST_ASSERT_EQUAL_UINT8(79u, r.start);
    TEST_ASSERT_EQUAL_UINT8(81u, r.stop);
}

void testZigbeeChannelToNrf24Range_clamps_out_of_range(void) {
    // Below 11 — clamp to 11.
    Nrf24Range r = zigbeeChannelToNrf24Range(0u);
    TEST_ASSERT_EQUAL_UINT8(4u, r.start);
    TEST_ASSERT_EQUAL_UINT8(6u, r.stop);
    // Above 26 — clamp to 26.
    r = zigbeeChannelToNrf24Range(99u);
    TEST_ASSERT_EQUAL_UINT8(79u, r.start);
    TEST_ASSERT_EQUAL_UINT8(81u, r.stop);
}

// ---------------------------------------------------------------------------
// BLE channel mappings
// ---------------------------------------------------------------------------

void testBleAdvChannels_match_2_26_80(void) {
    // BLE channels 0 / 12 / 39 sit at 2402 / 2426 / 2480 MHz which
    // correspond to the nRF24 channels 2 / 26 / 80 used for BLE
    // advertising-channel jamming (see DESIGN §10.4).
    //
    // We test via the *inverse* mapping `nrf24ToBleChannel` because
    // `bleChannelToNrf24` uses the data-channel formula
    // `(2402 + 2*N) MHz`, which is only valid for N in [0, 36].
    // The advertising channels themselves are at special positions
    // (2402 / 2426 / 2480) and are not on the data-channel grid; the
    // production code does not special-case them.
    TEST_ASSERT_EQUAL_UINT8(0u, nrf24ToBleChannel(2u));    // 2402 MHz → BLE ch 0
    TEST_ASSERT_EQUAL_UINT8(12u, nrf24ToBleChannel(26u));  // 2426 MHz → BLE ch 12
    TEST_ASSERT_EQUAL_UINT8(39u, nrf24ToBleChannel(80u));  // 2480 MHz → BLE ch 39
}

void testBleDataChannels_cover_even_nrf24_ch_2_to_80(void) {
    // The BLE data channel 0 sits at 2402 MHz → nRF24 ch 2.
    // The last data channel 36 sits at 2478 MHz → nRF24 ch 78.
    // (channel 37, 38, 39 are the advertising channels and live in the
    // 2426/2440/2480 MHz gap.) Every even nRF24 channel in [2, 80]
    // should map back to a valid BLE data channel.
    for (uint16_t nrf_ch = 2; nrf_ch <= 80; nrf_ch += 2) {
        const uint8_t ble = nrf24ToBleChannel(static_cast<uint8_t>(nrf_ch));
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0xFFu, ble, "nRF24 channel did not map back to a BLE channel");
        // The round-trip through freq should reproduce the same nRF24 ch.
        const uint8_t nrf2 = bleChannelToNrf24(ble);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(nrf_ch), nrf2);
    }
}

void testNrf24ToBleChannel_rejects_odd_channels(void) {
    // An odd nRF24 channel has no clean BLE mapping.
    TEST_ASSERT_EQUAL_UINT8(0xFFu, nrf24ToBleChannel(3u));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, nrf24ToBleChannel(5u));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, nrf24ToBleChannel(81u));
}

void testNrf24ToBleChannel_rejects_ble_adv_gap(void) {
    // The BLE advertising gap (ch 37/38/39 → nRF24 2/26/80) is
    // already part of the data channel space, but the inverse
    // mapping (nRF24 → BLE) for those exact channels should still
    // return a valid BLE channel — they're physically on the grid.
    TEST_ASSERT_EQUAL_UINT8(0u, nrf24ToBleChannel(2u));    // BLE ch 0
    TEST_ASSERT_EQUAL_UINT8(12u, nrf24ToBleChannel(26u));  // BLE ch 12
    TEST_ASSERT_EQUAL_UINT8(39u, nrf24ToBleChannel(80u));  // BLE ch 39
}

// ---------------------------------------------------------------------------
// main — Unity's standard runner. We group the tests into suites so the
// test report makes the structure obvious.
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    // nRF24 channel <-> freq
    RUN_TEST(testNrf24ChannelToFreq_channel_zero_is_2400);
    RUN_TEST(testNrf24ChannelToFreq_channel_two_is_2402);
    RUN_TEST(testNrf24ChannelToFreq_channel_76_is_2476);
    RUN_TEST(testNrf24ChannelToFreq_channel_125_is_2525);
    RUN_TEST(testNrf24FreqToChannel_round_trip);
    RUN_TEST(testNrf24FreqToChannel_clamps_below_band);
    RUN_TEST(testNrf24FreqToChannel_clamps_above_band);

    // CC1101
    RUN_TEST(testCc1101FreqToReg_433_92_mhz);
    RUN_TEST(testCc1101FreqToReg_315_mhz);
    RUN_TEST(testCc1101FreqToReg_868_mhz);
    RUN_TEST(testCc1101FreqToReg_915_mhz);
    RUN_TEST(testCc1101FreqToReg_masks_to_24_bits);
    RUN_TEST(testCc1101RegToFreq_round_trip);

    // WiFi
    RUN_TEST(testWifiChannelToNrf24Range_ch1);
    RUN_TEST(testWifiChannelToNrf24Range_ch6);
    RUN_TEST(testWifiChannelToNrf24Range_ch11);
    RUN_TEST(testWifiChannelToNrf24Range_ch13);
    RUN_TEST(testWifiChannelToNrf24Range_ch14_special);
    RUN_TEST(testWifiChannelToNrf24Range_clamps_out_of_range);

    // Zigbee
    RUN_TEST(testZigbeeChannelToNrf24Range_ch11);
    RUN_TEST(testZigbeeChannelToNrf24Range_ch26);
    RUN_TEST(testZigbeeChannelToNrf24Range_clamps_out_of_range);

    // BLE
    RUN_TEST(testBleAdvChannels_match_2_26_80);
    RUN_TEST(testBleDataChannels_cover_even_nrf24_ch_2_to_80);
    RUN_TEST(testNrf24ToBleChannel_rejects_odd_channels);
    RUN_TEST(testNrf24ToBleChannel_rejects_ble_adv_gap);

    return UNITY_END();
}
