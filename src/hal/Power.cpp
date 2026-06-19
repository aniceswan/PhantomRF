/**
 * @file Power.cpp
 * @brief Power, thermal, and battery management — implementation
 *
 * Pragmatic M0 subset of DESIGN §20.4. Uses the ESP32-S3 internal
 * temperature sensor when available (ESP-IDF driver/temperature_sensor.h)
 * and falls back to a stub on boards that don't have it. Battery is
 * read via analogRead() on the configured ADC pin and averaged.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "hal/Power.h"

#include "core/Config.h"
#include "core/State.h"
#include "hal/Board.h"
#include "utils/Logger.h"

#include <Arduino.h>

// Try to use the new ESP-IDF temperature sensor driver. It's available
// on ESP32-S3, ESP32-S2, and ESP32-C3 but not on the classic ESP32.
#if defined(ARDUINO_ARCH_ESP32) &&                                                                                     \
    (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3))
#define PHM_HAS_TEMP_SENSOR 1
// arduino-esp32 v3 uses driver/temperature_sensor.h via the Arduino core
// but the exact path varies. Use Arduino's wrapper if available.
#if __has_include(<driver/temperature_sensor.h>)
#include "driver/temperature_sensor.h"
#else
#undef PHM_HAS_TEMP_SENSOR
#define PHM_HAS_TEMP_SENSOR 0
#endif
#else
#define PHM_HAS_TEMP_SENSOR 0
#endif

namespace phm::hal {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Power g_power;

// How often to re-sample (ms). 5 s is plenty for thermal/battery.
static constexpr uint32_t kSamplePeriodMs = 5000;

// ADC reference voltage / scaling. The PhantomRF v1 schematic has a
// 100k/100k divider on VBAT, so V_ADC = V_BAT / 2. ESP32 ADC reference
// is 3.3 V over a 12-bit range.
static constexpr float kAdcRefV = 3.3f;
static constexpr float kAdcMaxCounts = 4095.0f;  // 12-bit
static constexpr float kVbatDivider = 0.5f;      // 1:1 divider
static constexpr uint16_t kLowBatteryMv = 3300;  // warn below 3.3 V

// ---------------------------------------------------------------------------
void Power::setup() {
#if PHM_HAS_TEMP_SENSOR
    static temperature_sensor_handle_t s_handle = nullptr;
    if (s_handle == nullptr) {
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
        if (temperature_sensor_install(&cfg, &s_handle) == ESP_OK) {
            temperature_sensor_enable(s_handle);
            LOGD("power", "internal temperature sensor enabled");
        } else {
            LOGW("power", "temperature sensor install failed");
        }
    }
#endif
    // Prime the first reading
    lastTempC_ = readTemperatureC();
    lastVbatMv_ = readVbatMilliVolts();
    throttling_ = false;
    shutdown_ = false;

    phm::g_state.internalTempC = lastTempC_;
    phm::g_state.vbat_mV = lastVbatMv_;

    LOGD("power", "initial temp=%.1fC vbat=%umV", static_cast<double>(lastTempC_), static_cast<unsigned>(lastVbatMv_));
}

// ---------------------------------------------------------------------------
void Power::update() {
    const uint32_t now = millis();
    if (now - lastUpdateMs_ < kSamplePeriodMs) {
        return;  // not time yet
    }
    lastUpdateMs_ = now;

    lastTempC_ = readTemperatureC();
    lastVbatMv_ = readVbatMilliVolts();

    phm::g_state.internalTempC = lastTempC_;
    phm::g_state.vbat_mV = lastVbatMv_;

    // ---- Thermal logic -------------------------------------------------
    if (lastTempC_ >= PHM_TEMP_CRITICAL_C) {
        LOGE("power", "CRITICAL temperature %.1fC — emergency shutdown", static_cast<double>(lastTempC_));
        emergencyShutdown();
        shutdownReason_ = kReasonOverTemp;
    } else if (lastTempC_ >= PHM_TEMP_THROTTLE_C) {
        if (!throttling_) {
            LOGW("power", "Throttling: temperature %.1fC >= %.1fC", static_cast<double>(lastTempC_),
                 static_cast<double>(PHM_TEMP_THROTTLE_C));
        }
        throttling_ = true;
    } else {
        throttling_ = false;
    }

    // ---- Battery logic -------------------------------------------------
    if (lastVbatMv_ > 0 && lastVbatMv_ < kLowBatteryMv) {
        LOGW("power", "Low battery: %umV", static_cast<unsigned>(lastVbatMv_));
    }
}

// ---------------------------------------------------------------------------
void Power::emergencyShutdown() {
    if (shutdown_) {
        return;  // already shutting down
    }
    shutdown_ = true;
    phm::g_state.state = phm::State::Error;
    LOGE("power", "Emergency shutdown requested (reason=%u)", static_cast<unsigned>(shutdownReason_));
    // The caller (PhantomRF::loop or a dedicated task) is expected to:
    //   1. stop all attacks,
    //   2. flush NVS (g_storage.commit()),
    //   3. call esp_deep_sleep_start().
    // We do NOT call those here so the shutdown is observable by other
    // modules first.
}

// ---------------------------------------------------------------------------
float Power::readTemperatureC() {
#if PHM_HAS_TEMP_SENSOR
    // The single global handle is fine — only one Power instance exists.
    static temperature_sensor_handle_t s_handle = []() -> temperature_sensor_handle_t {
        temperature_sensor_handle_t h = nullptr;
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
        if (temperature_sensor_install(&cfg, &h) == ESP_OK) {
            temperature_sensor_enable(h);
        }
        return h;
    }();
    if (s_handle != nullptr) {
        float c = 0.0f;
        if (temperature_sensor_read_celsius(s_handle, &c) == ESP_OK) {
            return c;
        }
    }
#endif
    // Stub: no sensor available (e.g. classic ESP32, or init failed).
    // Returning 25.0f lets the rest of the firmware run; the throttle
    // / shutdown logic will only trigger if a real sensor reports hot.
    return 25.0f;
}

// ---------------------------------------------------------------------------
uint16_t Power::readVbatMilliVolts() {
    const int8_t pin = g_board.pin(PinRole::VbatAdc);
    if (pin < 0) {
        return 0;
    }
    uint32_t acc = 0;
    for (uint8_t i = 0; i < PHM_VBAT_ADC_SAMPLES; ++i) {
        acc += static_cast<uint32_t>(analogRead(pin));
    }
    const float avgCounts = static_cast<float>(acc) / static_cast<float>(PHM_VBAT_ADC_SAMPLES);
    const float vAdc = (avgCounts / kAdcMaxCounts) * kAdcRefV;
    const float vBat = vAdc / kVbatDivider;
    return static_cast<uint16_t>(vBat * 1000.0f);
}

}  // namespace phm::hal
