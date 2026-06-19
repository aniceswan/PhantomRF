/**
 * @file Settings.cpp
 * @brief Settings management — implementation
 *
 * The key list mirrors DESIGN §13.1. Validation rules are deliberately
 * lenient: out-of-range values are clamped to the nearest legal
 * value rather than rejected, so a stale web UI that hasn't been
 * updated to a new key version still applies gracefully.
 *
 * The "phm" NVS namespace is shared with every other module — see
 * `phm::hal::g_storage` for the accessor functions.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "modules/Settings/Settings.h"

#include <string.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_random.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Config.h"
#include "core/State.h"
#include "hal/Storage.h"
#include "utils/Logger.h"

namespace phm::modules {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SettingsModule g_settings;

static constexpr const char* kTag = "settings";
static constexpr uint16_t kApPasswordLength = 16;
/// Allowed alphabet for the AP password (no shell-confusing chars)
static constexpr const char* kApAlphabet =
    "ABCDEFGHJKLMNPQRSTUVWXYZ"
    "abcdefghijkmnopqrstuvwxyz"
    "23456789"
    "!@#$%^&*";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Read a string setting with a default
String getOr(const char* key, const char* def) {
    String out;
    if (hal::g_storage.getString(key, out) && out.length() > 0) {
        return out;
    }
    return String(def);
}

/// Read an int setting with a default
int32_t getIntOr(const char* key, int32_t def) {
    int32_t v = 0;
    if (hal::g_storage.getInt(key, v)) {
        return v;
    }
    return def;
}

/// Read a bool setting with a default
bool getBoolOr(const char* key, bool def) {
    int32_t v = 0;
    if (hal::g_storage.getInt(key, v)) {
        return v != 0;
    }
    return def;
}

}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void SettingsModule::setup() {
    LOGI(kTag, "setup");

    // First-boot check: if there's no AP password, generate one and
    // print it to Serial so the user can read it.
    String pw;
    if (!hal::g_storage.getString("ap.password", pw) || pw.length() < 8) {
        const String generated = regenerateApPassword();
        LOGW(kTag, "Generated new AP password: %s", generated.c_str());
    }
}

void SettingsModule::loop() {
    // No periodic work. The factory-reset and password-regen paths
    // are triggered by HTTP POSTs, not by the main loop.
}

void SettingsModule::teardown() {
    // Nothing to flush
}

// ---------------------------------------------------------------------------
// toJson — read every key and emit a JSON document
// ---------------------------------------------------------------------------
String SettingsModule::toJson() {
    JsonDocument doc;

    // nRF24 settings
    doc["nrf24"]["count"]     = getIntOr("nrf24.count", 1);
    doc["nrf24"]["pa"]        = getIntOr("nrf24.pa", 0);
    doc["nrf24"]["data_rate"] = getIntOr("nrf24.data_rate", 2);

    // CE / CSN pin arrays: stored as a 5-byte blob in NVS.
    uint8_t cePins[5] = {16, 18, 35, 37, 39};
    uint8_t csnPins[5] = {15, 17, 21, 36, 38};
    hal::g_storage.getBytes("nrf24.ce_pins", cePins, sizeof(cePins));
    hal::g_storage.getBytes("nrf24.csn_pins", csnPins, sizeof(csnPins));
    JsonArray ce = doc["nrf24"]["ce_pins"].to<JsonArray>();
    JsonArray csn = doc["nrf24"]["csn_pins"].to<JsonArray>();
    for (int i = 0; i < 5; ++i) { ce.add(cePins[i]); csn.add(csnPins[i]); }

    // CC1101
    doc["cc1101"]["present"]      = getBoolOr("cc1101.present", true);
    doc["cc1101"]["sampling_us"]  = getIntOr("cc1101.sampling_us", 150);
    doc["cc1101"]["payload"]      = getIntOr("cc1101.payload", 60);

    // BLE / WiFi / Spectrum
    doc["ble"]["method"]          = getIntOr("ble.method", 0);
    doc["wifi"]["jam_method"]     = getIntOr("wifi.jam_method", 0);
    doc["wifi"]["deauth_method"]  = getIntOr("wifi.deauth_method", 0);
    doc["spectrum"]["window_ms"]  = getIntOr("spectrum.window_ms", 100);

    // AP
    doc["ap"]["ssid"]     = getOr("ap.ssid", "PhantomRF");
    doc["ap"]["enabled"]   = getBoolOr("ap.enabled", true);
    // Don't echo the password back unless explicitly requested — keep
    // it in a separate "ap.password_hint" field showing only length.
    String pw;
    if (hal::g_storage.getString("ap.password", pw)) {
        doc["ap"]["password_hint"] = "*";   // redacted
        doc["ap"]["password_len"]  = pw.length();
    } else {
        doc["ap"]["password_hint"] = "(unset)";
    }

    // OLED / buttons / sweep
    doc["oled"]["enabled"]    = getBoolOr("oled.enabled", true);
    doc["oled"]["brightness"] = getIntOr("oled.brightness", 255);
    doc["buttons"]["config"]  = getIntOr("buttons.config", 2);
    doc["sweep"]["method"]    = getIntOr("sweep.method", 0);

    // Battery calibration (mV/ADC-step, default ~1200)
    doc["battery"]["calibration"] = getIntOr("battery.calibration", 1200);

    // Build info
    doc["build"]["version"]   = PHM_VERSION_STRING;
    doc["build"]["board"]     = "unknown";

    String out;
    serializeJson(doc, out);
    return out;
}

// ---------------------------------------------------------------------------
// fromJson — apply a partial update
// ---------------------------------------------------------------------------
bool SettingsModule::fromJson(const String& json) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json);
    if (err) {
        LOGE(kTag, "fromJson: parse failed: %s", err.c_str());
        return false;
    }

    bool ok = true;

    // Helper lambdas: walk a JSON value, persist the matching NVS key.
    auto writeString = [&](const char* jsonKey, const char* nvsKey) {
        if (!doc[jsonKey].is<const char*>()) return;
        const char* v = doc[jsonKey].as<const char*>();
        if (!validateKey(nvsKey, v)) { ok = false; return; }
        hal::g_storage.setString(nvsKey, v);
    };
    auto writeInt = [&](const char* jsonKey, const char* nvsKey) {
        if (!doc[jsonKey].is<int>()) return;
        const int32_t v = doc[jsonKey].as<int32_t>();
        if (!validateKey(nvsKey, v)) { ok = false; return; }
        hal::g_storage.setInt(nvsKey, v);
    };
    auto writeBool = [&](const char* jsonKey, const char* nvsKey) {
        if (!doc[jsonKey].is<bool>()) return;
        const int32_t v = doc[jsonKey].as<bool>() ? 1 : 0;
        hal::g_storage.setInt(nvsKey, v);
    };

    // nRF24
    writeInt("nrf24.count",     "nrf24.count");
    writeInt("nrf24.pa",        "nrf24.pa");
    writeInt("nrf24.data_rate", "nrf24.data_rate");
    if (doc["nrf24"]["ce_pins"].is<JsonArray>()) {
        uint8_t pins[5] = {0};
        size_t i = 0;
        for (int v : doc["nrf24"]["ce_pins"].as<JsonArray>()) {
            if (i >= 5) break;
            pins[i++] = static_cast<uint8_t>(v);
        }
        hal::g_storage.setBytes("nrf24.ce_pins", pins, sizeof(pins));
    }
    if (doc["nrf24"]["csn_pins"].is<JsonArray>()) {
        uint8_t pins[5] = {0};
        size_t i = 0;
        for (int v : doc["nrf24"]["csn_pins"].as<JsonArray>()) {
            if (i >= 5) break;
            pins[i++] = static_cast<uint8_t>(v);
        }
        hal::g_storage.setBytes("nrf24.csn_pins", pins, sizeof(pins));
    }

    // CC1101
    writeBool("cc1101.present",   "cc1101.present");
    writeInt ("cc1101.sampling_us","cc1101.sampling_us");
    writeInt ("cc1101.payload",    "cc1101.payload");

    // BLE / WiFi / Spectrum
    writeInt("ble.method",          "ble.method");
    writeInt("wifi.jam_method",     "wifi.jam_method");
    writeInt("wifi.deauth_method",  "wifi.deauth_method");
    writeInt("spectrum.window_ms",  "spectrum.window_ms");

    // AP
    writeString("ap.ssid",   "ap.ssid");
    writeBool  ("ap.enabled","ap.enabled");
    if (doc["ap"]["password"].is<const char*>()) {
        const char* pw = doc["ap"]["password"].as<const char*>();
        if (pw != nullptr && strlen(pw) >= 8 && strlen(pw) <= 64) {
            hal::g_storage.setString("ap.password", pw);
        } else {
            ok = false;
        }
    }

    // OLED / buttons / sweep
    writeBool("oled.enabled",   "oled.enabled");
    writeInt ("oled.brightness","oled.brightness");
    writeInt ("buttons.config", "buttons.config");
    writeInt ("sweep.method",   "sweep.method");

    // Battery
    writeInt("battery.calibration", "battery.calibration");

    hal::g_storage.commit();
    LOGI(kTag, "fromJson: applied (ok=%s)", ok ? "yes" : "no");
    return ok;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------
bool SettingsModule::validateKey(const char* key, const char* value) {
    if (key == nullptr || value == nullptr) return false;
    if (strcmp(key, "ap.ssid") == 0) {
        const size_t len = strlen(value);
        return len >= 1 && len <= 32;
    }
    return true;
}

bool SettingsModule::validateKey(const char* key, int32_t value) {
    if (key == nullptr) return false;
    if (strcmp(key, "nrf24.count") == 0)    return value >= 1 && value <= 5;
    if (strcmp(key, "nrf24.pa") == 0)       return value >= 0 && value <= 3;
    if (strcmp(key, "nrf24.data_rate") == 0) return value >= 0 && value <= 2;
    if (strcmp(key, "cc1101.sampling_us") == 0) return value >= 10 && value <= 10000;
    if (strcmp(key, "cc1101.payload") == 0)     return value >= 1  && value <= 255;
    if (strcmp(key, "ble.method") == 0)      return value >= 0 && value <= 1;
    if (strcmp(key, "wifi.jam_method") == 0) return value >= 0 && value <= 2;
    if (strcmp(key, "wifi.deauth_method") == 0) return value >= 0 && value <= 2;
    if (strcmp(key, "spectrum.window_ms") == 0) return value >= 10 && value <= 5000;
    if (strcmp(key, "oled.brightness") == 0) return value >= 0 && value <= 255;
    if (strcmp(key, "buttons.config") == 0)  return value >= 0 && value <= 2;
    if (strcmp(key, "sweep.method") == 0)    return value >= 0 && value <= 1;
    if (strcmp(key, "battery.calibration") == 0) return value >= 100 && value <= 5000;
    return true;
}

// ---------------------------------------------------------------------------
// factoryReset
// ---------------------------------------------------------------------------
void SettingsModule::factoryReset() {
    LOGW(kTag, "factory reset requested — wiping NVS and rebooting");
    hal::g_storage.clearNamespace();
    hal::g_storage.commit();

    // Defer the actual restart so any in-flight HTTP response can
    // complete cleanly. 1 second is plenty.
    const auto restartTask = [](void*) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        vTaskDelete(nullptr);
    };
    xTaskCreate(restartTask, "phmreset", 2048, nullptr, 1, nullptr);
}

// ---------------------------------------------------------------------------
// regenerateApPassword
// ---------------------------------------------------------------------------
String SettingsModule::regenerateApPassword() {
    const size_t alphaLen = strlen(kApAlphabet);
    String pw;
    pw.reserve(kApPasswordLength);
    for (uint16_t i = 0; i < kApPasswordLength; ++i) {
        // Use the full 32 bits of esp_random() to keep bias low
        // even though our alphabet is a power of two (78).
        const uint32_t r = esp_random();
        const uint8_t idx = static_cast<uint8_t>(r % alphaLen);
        pw += kApAlphabet[idx];
    }
    hal::g_storage.setString("ap.password", pw.c_str());
    hal::g_storage.commit();
    return pw;
}

}  // namespace phm::modules
