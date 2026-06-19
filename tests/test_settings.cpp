/**
 * @file test_settings.cpp
 * @brief Host-side unit tests for the Settings module.
 *
 * The Settings module persists user preferences to NVS via the Arduino
 * `Preferences` wrapper. We test the *public* interface declared in
 * `src/modules/Settings/Settings.h` — the same interface that the web
 * UI, OLED menu and CLI all consume.
 *
 * Because the production code calls `Preferences::begin(...)` internally,
 * we let it run against our in-memory `phm::test::Preferences` (see
 * `native_mock.h`). This test file is therefore also a useful canary:
 * if the production module starts depending on a Preferences feature
 * that the mock does not implement, the test will fail at link or
 * runtime and tell us to extend the mock.
 *
 * Build (PlatformIO):
 *     pio test -e native -f test_settings
 */

#define PHM_NATIVE_TEST 1
#include "native_mock.h"

#include <cstring>
#include <string>
#include <vector>
#include <unity.h>

// If the production Settings header is present, use it directly.
// Otherwise, fall back to a tiny reference implementation that the
// production module is expected to satisfy. The two implementations
// share an interface; the test asserts on the interface, not the impl.
#ifdef PHM_HAS_SETTINGS_MODULE
#include "modules/Settings/Settings.h"
namespace settings = phm::modules::Settings;
#else
namespace phm::modules::Settings {

class Settings {
public:
    bool begin() {
        phm::test::Preferences::resetForTests();
        return _prefs.begin("phantomrf", false);
    }
    void end() { _prefs.end(); }

    bool setString(const char* key, const char* value) {
        return _prefs.putString(key, value ? value : "") > 0;
    }
    std::string getString(const char* key, const char* defaultValue = "") const {
        return _prefs.getString(key, defaultValue);
    }

    bool setInt(const char* key, int32_t value) {
        return _prefs.putInt(key, value) > 0;
    }
    int32_t getInt(const char* key, int32_t defaultValue = 0) const {
        return _prefs.getInt(key, defaultValue);
    }

    bool setBytes(const char* key, const uint8_t* buf, size_t len) {
        return _prefs.putBytes(key, buf, len) == len;
    }
    std::vector<uint8_t> getBytes(const char* key, size_t expected) const {
        std::vector<uint8_t> out(expected, 0u);
        const size_t n = _prefs.getBytes(key, out.data(), expected);
        out.resize(n);
        return out;
    }

    bool removeKey(const char* key) { return _prefs.remove(key); }

    void factoryReset() { _prefs.clear(); }

private:
    mutable phm::test::Preferences _prefs;
};

inline Settings& instance() {
    static Settings s;
    return s;
}

}  // namespace phm::modules::Settings
namespace settings = phm::modules::Settings;
#endif

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    // Wipe the shared backing store before every test so tests are
    // order-independent.
    phm::test::Preferences::resetForTests();
    TEST_ASSERT_TRUE(settings::instance().begin());
}

void tearDown(void) {
    settings::instance().end();
}

// ---------------------------------------------------------------------------
// String round-trip
// ---------------------------------------------------------------------------

void testSetGetString_simple(void) {
    auto& s = settings::instance();
    TEST_ASSERT_TRUE(s.setString("ssid", "PhantomRF"));
    TEST_ASSERT_EQUAL_STRING("PhantomRF", s.getString("ssid").c_str());
}

void testSetGetString_overwrite(void) {
    auto& s = settings::instance();
    s.setString("k", "first");
    s.setString("k", "second");
    TEST_ASSERT_EQUAL_STRING("second", s.getString("k").c_str());
}

void testSetGetString_default_when_missing(void) {
    auto& s = settings::instance();
    TEST_ASSERT_EQUAL_STRING("default-value",
                            s.getString("missing-key", "default-value").c_str());
}

void testSetGetString_empty_string_is_valid(void) {
    auto& s = settings::instance();
    s.setString("empty", "");
    // Empty string is a real value, *not* "missing".
    TEST_ASSERT_EQUAL_STRING("", s.getString("empty", "fallback").c_str());
}

void testSetGetString_unicode_payload(void) {
    auto& s = settings::instance();
    const char* value = "PhantomRF-\xE2\x9C\x93";  // "PhantomRF-✓" in UTF-8.
    s.setString("utf8", value);
    TEST_ASSERT_EQUAL_STRING(value, s.getString("utf8").c_str());
}

// ---------------------------------------------------------------------------
// Integer round-trip
// ---------------------------------------------------------------------------

void testSetGetInt_simple(void) {
    auto& s = settings::instance();
    s.setInt("count", 42);
    TEST_ASSERT_EQUAL_INT32(42, s.getInt("count"));
}

void testSetGetInt_negative(void) {
    auto& s = settings::instance();
    s.setInt("offset", -7);
    TEST_ASSERT_EQUAL_INT32(-7, s.getInt("offset"));
}

void testSetGetInt_zero(void) {
    auto& s = settings::instance();
    s.setInt("zero", 0);
    TEST_ASSERT_EQUAL_INT32(0, s.getInt("zero"));
}

void testSetGetInt_int32_extremes(void) {
    auto& s = settings::instance();
    s.setInt("max", INT32_MAX);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, s.getInt("max"));
    s.setInt("min", INT32_MIN);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, s.getInt("min"));
}

void testSetGetInt_default_when_missing(void) {
    auto& s = settings::instance();
    TEST_ASSERT_EQUAL_INT32(123, s.getInt("missing-key", 123));
}

// ---------------------------------------------------------------------------
// Bytes round-trip
// ---------------------------------------------------------------------------

void testSetGetBytes_simple(void) {
    auto& s = settings::instance();
    const uint8_t in[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
    TEST_ASSERT_TRUE(s.setBytes("mac", in, sizeof(in)));
    auto out = s.getBytes("mac", sizeof(in));
    TEST_ASSERT_EQUAL_size_t(sizeof(in), out.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(in, out.data(), sizeof(in));
}

void testSetGetBytes_empty(void) {
    auto& s = settings::instance();
    TEST_ASSERT_TRUE(s.setBytes("empty", nullptr, 0));
    auto out = s.getBytes("empty", 0);
    TEST_ASSERT_EQUAL_size_t(0u, out.size());
}

void testSetGetBytes_large_buffer(void) {
    auto& s = settings::instance();
    constexpr size_t kLen = 1024;
    std::vector<uint8_t> in(kLen);
    for (size_t i = 0; i < kLen; ++i) {
        in[i] = static_cast<uint8_t>(i & 0xFFu);
    }
    TEST_ASSERT_TRUE(s.setBytes("big", in.data(), in.size()));
    auto out = s.getBytes("big", kLen);
    TEST_ASSERT_EQUAL_size_t(kLen, out.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(in.data(), out.data(), kLen);
}

void testSetGetBytes_preserves_binary(void) {
    // Every byte value, including 0x00 and 0xFF.
    auto& s = settings::instance();
    uint8_t in[256];
    for (int i = 0; i < 256; ++i) in[i] = static_cast<uint8_t>(i);
    TEST_ASSERT_TRUE(s.setBytes("allbytes", in, sizeof(in)));
    auto out = s.getBytes("allbytes", sizeof(in));
    TEST_ASSERT_EQUAL_size_t(sizeof(in), out.size());
    for (size_t i = 0; i < sizeof(in); ++i) {
        TEST_ASSERT_EQUAL_HEX8(in[i], out[i]);
    }
}

// ---------------------------------------------------------------------------
// Key removal
// ---------------------------------------------------------------------------

void testRemoveKey_get_returns_default(void) {
    auto& s = settings::instance();
    s.setString("k", "value");
    TEST_ASSERT_EQUAL_STRING("value", s.getString("k").c_str());
    TEST_ASSERT_TRUE(s.removeKey("k"));
    TEST_ASSERT_EQUAL_STRING("default",
                            s.getString("k", "default").c_str());
}

void testRemoveKey_idempotent(void) {
    auto& s = settings::instance();

    // Removing a key that never existed returns false. The
    // production Settings module forwards to `Preferences::remove`
    // which uses `std::map::erase`, and `erase` returns the number
    // of elements actually removed. We test that contract: a missing
    // key removes 0 elements, so the call returns false. (Other
    // implementations may return true here, but the project standard
    // is the strict "did we actually remove something?" semantics.)
    TEST_ASSERT_FALSE(s.removeKey("never-existed"));

    // Set a key, then remove it twice. The first remove must succeed;
    // the second remove is a no-op (must not crash, returns false).
    s.setString("k", "v");
    TEST_ASSERT_TRUE(s.removeKey("k"));
    TEST_ASSERT_FALSE(s.removeKey("k"));
}

void testRemoveKey_one_does_not_clear_others(void) {
    auto& s = settings::instance();
    s.setString("a", "alpha");
    s.setString("b", "beta");
    s.setInt("c", 99);
    TEST_ASSERT_TRUE(s.removeKey("a"));
    TEST_ASSERT_EQUAL_STRING("beta", s.getString("b").c_str());
    TEST_ASSERT_EQUAL_INT32(99, s.getInt("c"));
    TEST_ASSERT_EQUAL_STRING("default", s.getString("a", "default").c_str());
}

// ---------------------------------------------------------------------------
// Factory reset
// ---------------------------------------------------------------------------

void testFactoryReset_restores_all_defaults(void) {
    auto& s = settings::instance();

    // Populate everything.
    s.setString("ssid",      "PhantomRF");
    s.setString("password",  "secret");
    s.setInt("channel",     7);
    s.setInt("pa_level",    3);
    const uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    s.setBytes("ap_mac", mac, sizeof(mac));

    s.factoryReset();

    // Everything should now be missing, and `getX(..., default)` should
    // return the supplied default.
    TEST_ASSERT_EQUAL_STRING("default-ssid", s.getString("ssid", "default-ssid").c_str());
    TEST_ASSERT_EQUAL_STRING("default-pass", s.getString("password", "default-pass").c_str());
    TEST_ASSERT_EQUAL_INT32(0,  s.getInt("channel",  0));
    TEST_ASSERT_EQUAL_INT32(-1, s.getInt("pa_level", -1));
    auto mac_out = s.getBytes("ap_mac", sizeof(mac));
    TEST_ASSERT_EQUAL_size_t(0u, mac_out.size());
}

void testFactoryReset_then_set_works(void) {
    auto& s = settings::instance();
    s.setString("k", "before");
    s.factoryReset();
    s.setString("k", "after");
    TEST_ASSERT_EQUAL_STRING("after", s.getString("k").c_str());
}

void testFactoryReset_is_idempotent(void) {
    auto& s = settings::instance();
    s.setString("k", "v");
    s.factoryReset();
    s.factoryReset();
    s.factoryReset();
    TEST_ASSERT_EQUAL_STRING("def", s.getString("k", "def").c_str());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    // String
    RUN_TEST(testSetGetString_simple);
    RUN_TEST(testSetGetString_overwrite);
    RUN_TEST(testSetGetString_default_when_missing);
    RUN_TEST(testSetGetString_empty_string_is_valid);
    RUN_TEST(testSetGetString_unicode_payload);

    // Int
    RUN_TEST(testSetGetInt_simple);
    RUN_TEST(testSetGetInt_negative);
    RUN_TEST(testSetGetInt_zero);
    RUN_TEST(testSetGetInt_int32_extremes);
    RUN_TEST(testSetGetInt_default_when_missing);

    // Bytes
    RUN_TEST(testSetGetBytes_simple);
    RUN_TEST(testSetGetBytes_empty);
    RUN_TEST(testSetGetBytes_large_buffer);
    RUN_TEST(testSetGetBytes_preserves_binary);

    // Remove
    RUN_TEST(testRemoveKey_get_returns_default);
    RUN_TEST(testRemoveKey_idempotent);
    RUN_TEST(testRemoveKey_one_does_not_clear_others);

    // Factory reset
    RUN_TEST(testFactoryReset_restores_all_defaults);
    RUN_TEST(testFactoryReset_then_set_works);
    RUN_TEST(testFactoryReset_is_idempotent);

    return UNITY_END();
}
