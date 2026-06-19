/**
 * @file native_mock.h
 * @brief Mock implementation of the Arduino / ESP32 API surface
 *
 * Lets us compile and run the production headers (`#include <Arduino.h>`,
 * `Preferences`, `WiFi`, `SPI`, `Wire`, ...) on a host machine (Linux,
 * macOS, Windows + MSYS) for the host-side Unity test suite.
 *
 * Activation:
 *   - Compile with `-DPHM_NATIVE_TEST=1`.
 *   - The mock header **completely replaces** `Arduino.h`, `WiFi.h`,
 *     `SPI.h`, `Wire.h`, `Preferences.h` etc. via a tiny shim
 *     (see `#include` block below). The shim lives in `tests/native/`.
 *
 * Implementation notes:
 *   - `millis()` / `micros()` are derived from `std::chrono::steady_clock`
 *     so they are monotonic and the rest of the firmware code that
 *     subtracts them works.
 *   - `Preferences` is backed by a `std::map<std::string, std::string>`
 *     so we can `setString` / `getString` / `remove` / `clear` without
 *     an actual NVS partition.
 *   - `WiFi` / `SPI` / `Wire` are stub objects that record calls so
 *     tests can assert on the side effects.
 *   - `Serial` writes go to `stderr` with a `[serial] ` prefix.
 *   - `esp_random()` is wired to `std::random_device` for non-determinism.
 */

#ifndef PHM_TESTS_NATIVE_MOCK_H
#define PHM_TESTS_NATIVE_MOCK_H

#ifdef PHM_NATIVE_TEST

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Basic types — the Arduino core would normally provide these via
// <stdint.h>, but we make them explicit so the host build is hermetic.
// ---------------------------------------------------------------------------

using uint8_t  = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using uint64_t = std::uint64_t;
using int8_t   = std::int8_t;
using int16_t  = std::int16_t;
using int32_t  = std::int32_t;
using int64_t  = std::int64_t;

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#ifndef INPUT
#define INPUT 0x01
#endif
#ifndef OUTPUT
#define OUTPUT 0x03
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x05
#endif
#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN 0x09
#endif
#ifndef PI
#define PI 3.14159265358979323846f
#endif

// ---------------------------------------------------------------------------
// GPIO state — a tiny "pin" table so `digitalRead` / `digitalWrite` work
// coherently across calls in the same test.
// ---------------------------------------------------------------------------

namespace phm::test {

inline std::map<int, int>& gpioPinState() {
    static std::map<int, int> m;
    return m;
}

inline std::mutex& gpioPinMutex() {
    static std::mutex m;
    return m;
}

}  // namespace phm::test

// ---------------------------------------------------------------------------
// Timing — monotonic, real-time
// ---------------------------------------------------------------------------

namespace phm::test {

/// @return Milliseconds since the first call to millis() / micros().
inline uint64_t monotonicMillis() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
}

/// @return Microseconds since the first call to millis() / micros().
inline uint64_t monotonicMicros() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
}

}  // namespace phm::test

inline uint32_t millis() { return static_cast<uint32_t>(phm::test::monotonicMillis()); }
inline uint32_t micros() { return static_cast<uint32_t>(phm::test::monotonicMicros()); }

inline void delay(uint32_t ms) {
    // In a unit-test we *do not* want to actually sleep for ms — most
    // tests don't care about real time and sleeping makes CI slow. We
    // expose a single env-var opt-in.
    static const char* env = std::getenv("PHM_NATIVE_TEST_REAL_DELAY");
    if (env && env[0] != '\0' && env[0] != '0') {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}
inline void delayMicroseconds(uint32_t us) {
    static const char* env = std::getenv("PHM_NATIVE_TEST_REAL_DELAY");
    if (env && env[0] != '\0' && env[0] != '0') {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
}

// ---------------------------------------------------------------------------
// GPIO
// ---------------------------------------------------------------------------

inline void pinMode(int /*pin*/, int /*mode*/) {
    // No-op: pinMode is irrelevant for our tests.
}

inline int digitalRead(int pin) {
    std::lock_guard<std::mutex> _lk(phm::test::gpioPinMutex());
    return phm::test::gpioPinState()[pin];
}

inline void digitalWrite(int pin, int value) {
    std::lock_guard<std::mutex> _lk(phm::test::gpioPinMutex());
    phm::test::gpioPinState()[pin] = value;
}

inline int analogRead(int /*pin*/) { return 0; }
inline void analogWrite(int /*pin*/, int /*value*/) {}

// ---------------------------------------------------------------------------
// Serial — writes to stderr so the test framework can capture it.
// ---------------------------------------------------------------------------

namespace phm::test {

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buf, size_t n) {
        size_t w = 0;
        for (size_t i = 0; i < n; ++i) {
            w += write(buf[i]);
        }
        return w;
    }

    size_t print(const char* s) {
        if (!s) return 0;
        return write(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
    }
    size_t print(const std::string& s) {
        return write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }
    size_t print(int v) {
        char buf[16];
        int n = std::snprintf(buf, sizeof(buf), "%d", v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(unsigned v) {
        char buf[16];
        int n = std::snprintf(buf, sizeof(buf), "%u", v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(long v) {
        char buf[24];
        int n = std::snprintf(buf, sizeof(buf), "%ld", v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(unsigned long v) {
        char buf[24];
        int n = std::snprintf(buf, sizeof(buf), "%lu", v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(long long v) {
        char buf[32];
        int n = std::snprintf(buf, sizeof(buf), "%lld", v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(unsigned long long v) {
        char buf[32];
        int n = std::snprintf(buf, sizeof(buf), "%llu", v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(double v, int decimals = 2) {
        char buf[64];
        int n = std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        return write(reinterpret_cast<const uint8_t*>(buf),
                     static_cast<size_t>(n > 0 ? n : 0));
    }
    size_t print(float v, int decimals = 2) {
        return print(static_cast<double>(v), decimals);
    }
    size_t println(const char* s) {
        size_t w = print(s);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(const std::string& s) {
        size_t w = print(s);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(int v) {
        size_t w = print(v);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(unsigned v) {
        size_t w = print(v);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(long v) {
        size_t w = print(v);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(unsigned long v) {
        size_t w = print(v);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(long long v) {
        size_t w = print(v);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(unsigned long long v) {
        size_t w = print(v);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(double v, int decimals = 2) {
        size_t w = print(v, decimals);
        w += write(static_cast<uint8_t>('\n'));
        return w;
    }
    size_t println(float v, int decimals = 2) {
        return println(static_cast<double>(v), decimals);
    }
    size_t println() {
        return write(static_cast<uint8_t>('\n'));
    }

    size_t printf(const char* fmt, ...) {
        if (!fmt) return 0;
        char buf[512];
        std::va_list ap;
        va_start(ap, fmt);
        int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n < 0) return 0;
        size_t len = static_cast<size_t>(n);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        return write(reinterpret_cast<const uint8_t*>(buf), len);
    }
};

class HostSerial : public Print {
public:
    size_t write(uint8_t c) override {
        std::lock_guard<std::mutex> _lk(_mtx);
        std::fputc(static_cast<int>(c), stderr);
        return 1;
    }
    // Re-export the buffer variant from the base class so callers that
    // use the (buf, len) overload find it. We define it here (instead
    // of inheriting) because C++ name hiding would otherwise shadow it.
    using Print::write;

    void flush() { std::fflush(stderr); }
    void begin(unsigned long /*baud*/) {}
    void end() {}

    // Minimal printf so `Serial.printf("...")` works.
    size_t printf(const char* fmt, ...) {
        char buf[512];
        std::va_list ap;
        va_start(ap, fmt);
        int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n < 0) return 0;
        size_t len = static_cast<size_t>(n);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        // The using-declaration above re-exports the base class's
        // (buf, len) overload so we can call it directly.
        return Print::write(reinterpret_cast<const uint8_t*>(buf), len);
    }

private:
    std::mutex _mtx;
};

inline HostSerial& SerialInstance() {
    static HostSerial s;
    return s;
}

}  // namespace phm::test

// Global handle like the Arduino core.
inline phm::test::HostSerial& Serial = phm::test::SerialInstance();

// ---------------------------------------------------------------------------
// Preferences — a tiny NVS-shaped wrapper over std::map.
// ---------------------------------------------------------------------------

namespace phm::test {

class Preferences {
public:
    bool begin(const char* name, bool /*readOnly*/ = false) {
        if (!name) name = "";
        _ns = name;
        // The map is shared across all "namespaces" — fine for unit
        // tests, which only ever open a single namespace at a time.
        return true;
    }
    void end() { _ns.clear(); }

    void clear() {
        std::lock_guard<std::mutex> _lk(_mtx);
        // Drop every key that belongs to this namespace.
        const std::string prefix = _ns + "::";
        for (auto it = _store.begin(); it != _store.end();) {
            if (it->first.compare(0, prefix.size(), prefix) == 0) {
                it = _store.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool remove(const char* key) {
        if (!key) return false;
        std::lock_guard<std::mutex> _lk(_mtx);
        return _store.erase(_ns + "::" + key) > 0;
    }

    size_t putString(const char* key, const char* value) {
        if (!key || !value) return 0;
        std::lock_guard<std::mutex> _lk(_mtx);
        _store[_ns + "::" + key] = value;
        return std::strlen(value);
    }
    size_t putString(const char* key, const std::string& value) {
        return putString(key, value.c_str());
    }

    std::string getString(const char* key, const std::string& defaultValue = "") const {
        if (!key) return defaultValue;
        std::lock_guard<std::mutex> _lk(_mtx);
        auto it = _store.find(_ns + "::" + key);
        if (it == _store.end()) return defaultValue;
        return it->second;
    }

    template <typename T>
    size_t putBytes(const char* key, const T* buf, size_t len) {
        if (!key || !buf) return 0;
        std::lock_guard<std::mutex> _lk(_mtx);
        std::string encoded;
        encoded.resize(sizeof(T) * len);
        std::memcpy(encoded.data(), buf, sizeof(T) * len);
        _store[_ns + "::" + key] = std::move(encoded);
        return len;
    }

    template <typename T>
    size_t getBytes(const char* key, T* buf, size_t len) const {
        if (!key || !buf) return 0;
        std::lock_guard<std::mutex> _lk(_mtx);
        auto it = _store.find(_ns + "::" + key);
        if (it == _store.end()) return 0;
        const std::string& encoded = it->second;
        size_t max = std::min(len * sizeof(T), encoded.size());
        std::memcpy(buf, encoded.data(), max);
        return max / sizeof(T);
    }

    // 32-bit signed integer helpers (the only ones the Settings module
    // actually uses besides strings/bytes).
    int32_t getInt(const char* key, int32_t defaultValue = 0) const {
        if (!key) return defaultValue;
        std::lock_guard<std::mutex> _lk(_mtx);
        auto it = _store.find(_ns + "::" + key);
        if (it == _store.end()) return defaultValue;
        try {
            return static_cast<int32_t>(std::stol(it->second));
        } catch (...) {
            return defaultValue;
        }
    }
    size_t putInt(const char* key, int32_t value) {
        if (!key) return 0;
        std::lock_guard<std::mutex> _lk(_mtx);
        _store[_ns + "::" + key] = std::to_string(value);
        return sizeof(int32_t);
    }

    bool isKey(const char* key) const {
        if (!key) return false;
        std::lock_guard<std::mutex> _lk(_mtx);
        return _store.count(_ns + "::" + key) > 0;
    }

    /// Test-only: wipe the entire backing store.
    static void resetForTests() {
        std::lock_guard<std::mutex> _lk(_mtx);
        _store.clear();
    }

private:
    std::string _ns;
    static std::map<std::string, std::string>& _store;
    static std::mutex& _mtx;
};

}  // namespace phm::test

// ---------------------------------------------------------------------------
// WiFi — a stub that lets the WebServer module initialise in tests.
// ---------------------------------------------------------------------------

namespace phm::test {

class IPAddress {
public:
    IPAddress() : a_{0, 0, 0, 0} {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        a_[0] = a; a_[1] = b; a_[2] = c; a_[3] = d;
    }
    uint8_t operator[](int i) const { return a_[i]; }
    std::string toString() const {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", a_[0], a_[1], a_[2], a_[3]);
        return buf;
    }
private:
    uint8_t a_[4];
};

struct WifiNetwork {
    std::string ssid;
    int32_t rssi;
    uint8_t encryption;  // 0 = open, others opaque
};

class WiFiClass {
public:
    bool mode(int /*m*/) { return true; }
    bool softAP(const char* ssid, const char* pass = nullptr) {
        _ap_ssid = ssid ? ssid : "";
        _ap_pass = pass ? pass : "";
        _running = true;
        return true;
    }
    bool softAPdisconnect(bool /*wipe*/ = false) { _running = false; return true; }
    bool begin(const char* /*ssid*/, const char* /*pass*/ = nullptr) { return true; }
    void disconnect(bool /*wipe*/ = false) {}
    IPAddress localIP() const { return IPAddress(192, 168, 4, 1); }
    IPAddress softAPIP() const { return IPAddress(192, 168, 4, 1); }
    std::string softAPSSID() const { return _ap_ssid; }

    /// Test hook: prime the scan result list.
    void setScanResults(std::vector<WifiNetwork> nets) {
        std::lock_guard<std::mutex> _lk(_mtx);
        _scan = std::move(nets);
    }
    int scanNetworks(bool /*async*/ = false) {
        std::lock_guard<std::mutex> _lk(_mtx);
        return static_cast<int>(_scan.size());
    }
    std::string SSID(int i) const {
        std::lock_guard<std::mutex> _lk(_mtx);
        return (i >= 0 && static_cast<size_t>(i) < _scan.size()) ? _scan[i].ssid : "";
    }
    int32_t RSSI(int i) const {
        std::lock_guard<std::mutex> _lk(_mtx);
        return (i >= 0 && static_cast<size_t>(i) < _scan.size()) ? _scan[i].rssi : 0;
    }
    uint8_t encryptionType(int i) const {
        std::lock_guard<std::mutex> _lk(_mtx);
        return (i >= 0 && static_cast<size_t>(i) < _scan.size()) ? _scan[i].encryption : 0;
    }

    static WiFiClass& instance() {
        static WiFiClass w;
        return w;
    }

private:
    std::string _ap_ssid;
    std::string _ap_pass;
    bool _running = false;
    std::vector<WifiNetwork> _scan;
    mutable std::mutex _mtx;
};

}  // namespace phm::test

inline phm::test::WiFiClass& WiFi = phm::test::WiFiClass::instance();

// ---------------------------------------------------------------------------
// SPI / Wire — no-op stubs that record the most recent call.
// ---------------------------------------------------------------------------

namespace phm::test {

class SpiBus {
public:
    void begin(int /*sck*/ = -1, int /*miso*/ = -1, int /*mosi*/ = -1, int /*ss*/ = -1) {
        _initialised = true;
    }
    void end() { _initialised = false; }
    uint8_t transfer(uint8_t v) { _last_xfer = v; return v; }
    bool initialised() const { return _initialised; }
    uint8_t lastTransfer() const { return _last_xfer; }

    static SpiBus& instance() {
        static SpiBus b;
        return b;
    }
private:
    bool _initialised = false;
    uint8_t _last_xfer = 0;
};

class TwoWire {
public:
    void begin(int /*sda*/ = -1, int /*scl*/ = -1, uint32_t /*hz*/ = 0) { _initialised = true; }
    void end() { _initialised = false; }
    void beginTransmission(uint8_t /*addr*/) { _tx.clear(); }
    uint8_t endTransmission(bool /*stop*/ = true) { return 0; }
    size_t write(uint8_t v) { _tx.push_back(v); return 1; }
    size_t write(const uint8_t* buf, size_t n) {
        _tx.insert(_tx.end(), buf, buf + n);
        return n;
    }
    int available() { return static_cast<int>(_rx.size()); }
    int read() {
        if (_rx.empty()) return -1;
        int v = _rx.front();
        _rx.erase(_rx.begin());
        return v;
    }
    bool initialised() const { return _initialised; }
    const std::vector<uint8_t>& txBuffer() const { return _tx; }
    void primeRx(std::vector<uint8_t> data) { _rx = std::move(data); }

    static TwoWire& instance() {
        static TwoWire w;
        return w;
    }
private:
    bool _initialised = false;
    std::vector<uint8_t> _tx;
    std::vector<uint8_t> _rx;
};

}  // namespace phm::test

inline phm::test::SpiBus&  SPI  = phm::test::SpiBus::instance();
inline phm::test::TwoWire& Wire = phm::test::TwoWire::instance();

// ---------------------------------------------------------------------------
// esp_random — use std::random_device so seeds are non-deterministic.
// ---------------------------------------------------------------------------

namespace phm::test {
inline uint32_t esp_random() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    return static_cast<uint32_t>(gen());
}
}  // namespace phm::test

// ---------------------------------------------------------------------------
// Arduino.h shim — the production code does `#include <Arduino.h>`. On the
// host, we have to provide a tiny replacement that pulls in our mock.
// ---------------------------------------------------------------------------

// (nothing to do — the headers that include <Arduino.h> on the host will
// just fail to find it. The test file itself should `#include
// "native_mock.h"` *before* any production header.)

// ---------------------------------------------------------------------------
// PROGMEM — irrelevant on host, but some headers reference it. Map to
// nothing meaningful; the F macro is a no-op.
// ---------------------------------------------------------------------------

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef F
#define F(s) (s)
#endif
#ifndef FPSTR
#define FPSTR(p) (p)
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR
#endif
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#endif  // PHM_NATIVE_TEST

#endif  // PHM_TESTS_NATIVE_MOCK_H
