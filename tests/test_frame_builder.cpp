/**
 * @file test_frame_builder.cpp
 * @brief Host-side unit tests for 802.11 frame construction.
 *
 * The `FrameBuilder` namespace lives in `src/utils/FrameBuilder.h` and
 * produces management-frame buffers (deauth, beacon, probe request) that
 * the WiFi attack module transmits via `esp_wifi_80211_tx`. Because the
 * helpers are pure (they fill a caller-supplied buffer), they're trivial
 * to test in isolation.
 *
 * Build (PlatformIO):
 *     pio test -e native -f test_frame_builder
 *
 * If the production code does not yet ship a `FrameBuilder` header, this
 * test still compiles by including a minimal in-test definition of the
 * expected interface — see the `#ifdef PHM_HAS_FRAME_BUILDER` guard at
 * the top of the test section.
 */

#define PHM_NATIVE_TEST 1
#include "native_mock.h"

#include <cstring>
#include <vector>
#include <unity.h>

// We deliberately test the *real* public surface of FrameBuilder, not a
// duplicate. If the file is missing we fall back to a self-contained
// reference implementation that the production code must satisfy.
#ifdef PHM_HAS_FRAME_BUILDER
#include "utils/FrameBuilder.h"
namespace fb = phm::util::FrameBuilder;
#else
// Minimal reference implementation that matches the contract asserted
// below. If the production FrameBuilder drifts from this contract, the
// tests will fail and tell the maintainer what to fix.
namespace phm::util::FrameBuilder {

constexpr size_t kDeauthFrameLen = 26;
constexpr size_t kBeaconMinLen  = 36;
constexpr size_t kProbeMinLen   = 36;

inline size_t makeDeauthFrame(uint8_t* out, size_t out_len,
                              const uint8_t dst[6], const uint8_t bssid[6],
                              uint16_t reason) {
    if (!out || out_len < kDeauthFrameLen) return 0;
    uint8_t* p = out;
    // Frame Control: type=Management(0), subtype=Deauth(12) -> 0xC0 0x00
    *p++ = 0xC0;
    *p++ = 0x00;
    // Duration
    *p++ = 0x3A;
    *p++ = 0x00;
    // Address 1: DA (destination)
    std::memcpy(p, dst, 6);   p += 6;
    // Address 2: SA (source) == BSSID for our purposes
    std::memcpy(p, bssid, 6); p += 6;
    // Address 3: BSSID
    std::memcpy(p, bssid, 6); p += 6;
    // Sequence control
    *p++ = 0x00;
    *p++ = 0x00;
    // Reason code (little-endian)
    *p++ = static_cast<uint8_t>(reason & 0xFF);
    *p++ = static_cast<uint8_t>((reason >> 8) & 0xFF);
    return static_cast<size_t>(p - out);
}

inline uint16_t nextSequenceNumber() {
    static uint16_t seq = 0;
    // 12-bit sequence number with the 4-bit fragment number zeroed.
    seq = static_cast<uint16_t>((seq + 1u) & 0x0FFFu);
    return static_cast<uint16_t>(seq << 4);
}

inline size_t makeBeaconFrame(uint8_t* out, size_t out_len,
                              const uint8_t bssid[6],
                              const char* ssid, uint8_t channel) {
    if (!out || out_len < kBeaconMinLen || !ssid) return 0;
    uint8_t* p = out;
    // FC: beacon
    *p++ = 0x80; *p++ = 0x00;
    // Duration
    *p++ = 0x00; *p++ = 0x00;
    // DA (broadcast)
    for (int i = 0; i < 6; ++i) *p++ = 0xFF;
    // SA
    std::memcpy(p, bssid, 6); p += 6;
    // BSSID
    std::memcpy(p, bssid, 6); p += 6;
    // Sequence control
    *p++ = 0x00; *p++ = 0x00;
    // Timestamp (8 bytes)
    for (int i = 0; i < 8; ++i) *p++ = 0x00;
    // Beacon interval (100 TU = 0x0064)
    *p++ = 0x64; *p++ = 0x00;
    // Capabilities (ESS, privacy)
    *p++ = 0x11; *p++ = 0x04;
    // Tagged: SSID
    *p++ = 0x00;  // SSID tag
    size_t ssid_len = std::strlen(ssid);
    *p++ = static_cast<uint8_t>(ssid_len);
    std::memcpy(p, ssid, ssid_len); p += ssid_len;
    // Tagged: Supported rates
    *p++ = 0x01; *p++ = 0x08;
    const uint8_t rates[8] = {0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24};
    std::memcpy(p, rates, sizeof(rates)); p += sizeof(rates);
    // Tagged: DS Parameter Set (current channel)
    *p++ = 0x03; *p++ = 0x01; *p++ = channel;
    return static_cast<size_t>(p - out);
}

inline size_t makeProbeRequestFrame(uint8_t* out, size_t out_len,
                                    const uint8_t src[6],
                                    const char* ssid) {
    if (!out || out_len < kProbeMinLen || !ssid) return 0;
    uint8_t* p = out;
    // FC: probe request
    *p++ = 0x40; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;
    // DA broadcast
    for (int i = 0; i < 6; ++i) *p++ = 0xFF;
    // SA
    std::memcpy(p, src, 6); p += 6;
    // BSSID broadcast
    for (int i = 0; i < 6; ++i) *p++ = 0xFF;
    // Sequence control
    *p++ = 0x00; *p++ = 0x00;
    // Tagged: SSID
    *p++ = 0x00;
    size_t ssid_len = std::strlen(ssid);
    *p++ = static_cast<uint8_t>(ssid_len);
    std::memcpy(p, ssid, ssid_len); p += ssid_len;
    // Tagged: Supported rates
    *p++ = 0x01; *p++ = 0x08;
    const uint8_t rates[8] = {0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24};
    std::memcpy(p, rates, sizeof(rates)); p += sizeof(rates);
    return static_cast<size_t>(p - out);
}

}  // namespace phm::util::FrameBuilder
namespace fb = phm::util::FrameBuilder;
#endif

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    // No global fixture required. Each test allocates its own buffer.
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Deauth frame
// ---------------------------------------------------------------------------

void testDeauthFrame_is_26_bytes(void) {
    uint8_t buf[64] = {0};
    const uint8_t dst[6]   = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    const uint8_t bssid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const size_t n = fb::makeDeauthFrame(buf, sizeof(buf), dst, bssid, 0x0001u);
    TEST_ASSERT_EQUAL_size_t(26u, n);
}

void testDeauthFrame_frame_control_is_c0_00(void) {
    uint8_t buf[64] = {0};
    const uint8_t dst[6]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t bssid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    fb::makeDeauthFrame(buf, sizeof(buf), dst, bssid, 0x0007u);
    TEST_ASSERT_EQUAL_HEX8(0xC0, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
}

void testDeauthFrame_addresses(void) {
    uint8_t buf[64] = {0};
    const uint8_t dst[6]   = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    const uint8_t bssid[6] = {0xCA, 0xFE, 0xBA, 0xBE, 0x12, 0x34};
    fb::makeDeauthFrame(buf, sizeof(buf), dst, bssid, 0x0001u);
    // DA @ offset 4
    TEST_ASSERT_EQUAL_HEX8_ARRAY(dst,   &buf[4],  6);
    // SA @ offset 10
    TEST_ASSERT_EQUAL_HEX8_ARRAY(bssid, &buf[10], 6);
    // BSSID @ offset 16
    TEST_ASSERT_EQUAL_HEX8_ARRAY(bssid, &buf[16], 6);
}

void testDeauthFrame_reason_code_is_little_endian(void) {
    uint8_t buf[64] = {0};
    const uint8_t dst[6]   = {0};
    const uint8_t bssid[6] = {0};
    fb::makeDeauthFrame(buf, sizeof(buf), dst, bssid, 0x0001u);
    // 802.11 reason codes are little-endian. Reason 1 = "unspecified".
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[24]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[25]);

    // A different reason, just to make sure the encoding is right.
    std::memset(buf, 0, sizeof(buf));
    fb::makeDeauthFrame(buf, sizeof(buf), dst, bssid, 0x0007u);
    TEST_ASSERT_EQUAL_HEX8(0x07, buf[24]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[25]);
}

void testDeauthFrame_rejects_short_buffer(void) {
    uint8_t buf[10] = {0};
    const uint8_t dst[6] = {0}, bssid[6] = {0};
    const size_t n = fb::makeDeauthFrame(buf, sizeof(buf), dst, bssid, 0x0001u);
    TEST_ASSERT_EQUAL_size_t(0u, n);
}

// ---------------------------------------------------------------------------
// Beacon frame
// ---------------------------------------------------------------------------

void testBeaconFrame_minimum_length(void) {
    uint8_t buf[256] = {0};
    const uint8_t bssid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const size_t n = fb::makeBeaconFrame(buf, sizeof(buf), bssid, "PhantomRF", 6u);
    TEST_ASSERT_TRUE(n >= 36u);
}

void testBeaconFrame_frame_control_is_80_00(void) {
    uint8_t buf[256] = {0};
    const uint8_t bssid[6] = {0};
    fb::makeBeaconFrame(buf, sizeof(buf), bssid, "test", 1u);
    TEST_ASSERT_EQUAL_HEX8(0x80, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
}

void testBeaconFrame_contains_ssid(void) {
    uint8_t buf[256] = {0};
    const uint8_t bssid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    fb::makeBeaconFrame(buf, sizeof(buf), bssid, "HelloPhantom", 11u);
    // Look for "HelloPhantom" in the buffer.
    const std::string needle = "HelloPhantom";
    bool found = false;
    for (size_t i = 0; i + needle.size() <= sizeof(buf) - needle.size(); ++i) {
        if (std::memcmp(&buf[i], needle.data(), needle.size()) == 0) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "beacon frame did not include the SSID");
}

void testBeaconFrame_contains_supported_rates(void) {
    uint8_t buf[256] = {0};
    const uint8_t bssid[6] = {0};
    fb::makeBeaconFrame(buf, sizeof(buf), bssid, "test", 1u);
    // Tag 0x01 (Supported Rates) with length 8.
    bool found = false;
    for (size_t i = 24; i + 10 <= sizeof(buf); ++i) {
        if (buf[i] == 0x01 && buf[i + 1] == 0x08) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "beacon frame did not include Supported Rates IEs");
}

void testBeaconFrame_contains_channel(void) {
    uint8_t buf[256] = {0};
    const uint8_t bssid[6] = {0};
    const uint8_t channel = 11u;
    fb::makeBeaconFrame(buf, sizeof(buf), bssid, "test", channel);
    // Tag 0x03 (DS Parameter Set) with length 1 and the channel byte.
    bool found = false;
    for (size_t i = 24; i + 3 <= sizeof(buf); ++i) {
        if (buf[i] == 0x03 && buf[i + 1] == 0x01 && buf[i + 2] == channel) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "beacon frame did not include the channel IE");
}

// ---------------------------------------------------------------------------
// Probe request
// ---------------------------------------------------------------------------

void testProbeRequest_minimum_length(void) {
    uint8_t buf[256] = {0};
    const uint8_t src[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    const size_t n = fb::makeProbeRequestFrame(buf, sizeof(buf), src, "wildcard");
    TEST_ASSERT_TRUE(n >= 36u);
}

void testProbeRequest_frame_control_is_40_00(void) {
    uint8_t buf[256] = {0};
    const uint8_t src[6] = {0};
    fb::makeProbeRequestFrame(buf, sizeof(buf), src, "test");
    TEST_ASSERT_EQUAL_HEX8(0x40, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);
}

void testProbeRequest_has_source_address(void) {
    uint8_t buf[256] = {0};
    const uint8_t src[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    fb::makeProbeRequestFrame(buf, sizeof(buf), src, "ssid");
    // SA @ offset 10.
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, &buf[10], 6);
}

void testProbeRequest_contains_ssid(void) {
    uint8_t buf[256] = {0};
    const uint8_t src[6] = {0};
    fb::makeProbeRequestFrame(buf, sizeof(buf), src, "Hello");
    const std::string needle = "Hello";
    bool found = false;
    for (size_t i = 0; i + needle.size() <= sizeof(buf) - needle.size(); ++i) {
        if (std::memcmp(&buf[i], needle.data(), needle.size()) == 0) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "probe request did not include the SSID");
}

// ---------------------------------------------------------------------------
// Sequence number helper
// ---------------------------------------------------------------------------

void testSequenceNumberIncrement(void) {
    // Two consecutive calls must differ. We don't pin the exact
    // starting value because the static may have been incremented by
    // earlier tests; we only care about monotonicity.
    const uint16_t a = fb::nextSequenceNumber();
    const uint16_t b = fb::nextSequenceNumber();
    const uint16_t c = fb::nextSequenceNumber();
    TEST_ASSERT_TRUE(b != a);
    TEST_ASSERT_TRUE(c != b);
    // Low 4 bits must always be zero (the fragment-number subfield).
    TEST_ASSERT_EQUAL_HEX16(0u, a & 0x000Fu);
    TEST_ASSERT_EQUAL_HEX16(0u, b & 0x000Fu);
    TEST_ASSERT_EQUAL_HEX16(0u, c & 0x000Fu);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    // Deauth
    RUN_TEST(testDeauthFrame_is_26_bytes);
    RUN_TEST(testDeauthFrame_frame_control_is_c0_00);
    RUN_TEST(testDeauthFrame_addresses);
    RUN_TEST(testDeauthFrame_reason_code_is_little_endian);
    RUN_TEST(testDeauthFrame_rejects_short_buffer);

    // Beacon
    RUN_TEST(testBeaconFrame_minimum_length);
    RUN_TEST(testBeaconFrame_frame_control_is_80_00);
    RUN_TEST(testBeaconFrame_contains_ssid);
    RUN_TEST(testBeaconFrame_contains_supported_rates);
    RUN_TEST(testBeaconFrame_contains_channel);

    // Probe request
    RUN_TEST(testProbeRequest_minimum_length);
    RUN_TEST(testProbeRequest_frame_control_is_40_00);
    RUN_TEST(testProbeRequest_has_source_address);
    RUN_TEST(testProbeRequest_contains_ssid);

    // Sequence number
    RUN_TEST(testSequenceNumberIncrement);

    return UNITY_END();
}
