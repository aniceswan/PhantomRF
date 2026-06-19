/**
 * @file Power.h
 * @brief Power, thermal, and battery management
 *
 * Implements a pragmatic subset of DESIGN §20.4 for the M0 release:
 *
 *   - Brownout detection is tuned via `sdkconfig.defaults`
 *     (`CONFIG_ESP_BROWNOUT_DET_LVL_SEL_7` = 2.8 V). This header does
 *     not change it at runtime.
 *   - The internal die temperature is sampled on every `update()` and
 *     stored in `phm::g_state.internalTempC`.
 *   - The battery voltage is read on the configured ADC pin and
 *     averaged over `PHM_VBAT_ADC_SAMPLES` reads.
 *   - `emergencyShutdown()` is called when the temperature crosses
 *     `PHM_TEMP_CRITICAL_C`; it sets `g_state.state` to `Error` and
 *     asks the device to enter deep sleep on the next main loop pass.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

namespace phm::hal {

/**
 * @brief Singleton power / thermal manager
 *
 * Call `setup()` once during boot; call `update()` from the main loop
 * (PhantomRF does this). When a critical condition is detected, the
 * `shutdownRequested()` flag becomes true and the caller is expected
 * to cooperate (stop attacks, persist state, `esp_deep_sleep_start()`).
 */
class Power {
public:
    /// Initialise the temperature sensor and ADC
    void setup();

    /// Tick — sample temperature + battery; check thresholds
    void update();

    /// Force an emergency shutdown (temperature, brownout, etc.)
    void emergencyShutdown();

    /// True if the manager has asked the device to shut down
    bool shutdownRequested() const { return shutdown_; }

    /// Last temperature reading in °C
    float temperatureC() const { return lastTempC_; }

    /// Last battery voltage in millivolts
    uint16_t vbatMilliVolts() const { return lastVbatMv_; }

    /// True if the last temperature crossed PHM_TEMP_THROTTLE_C
    bool isThrottling() const { return throttling_; }

    /// Reason code for the last emergency shutdown
    uint8_t shutdownReason() const { return shutdownReason_; }

    // ---- Shutdown reason codes -------------------------------------------
    static constexpr uint8_t kReasonNone = 0;
    static constexpr uint8_t kReasonOverTemp = 1;
    static constexpr uint8_t kReasonBrownout = 2;
    static constexpr uint8_t kReasonLowBattery = 3;
    static constexpr uint8_t kReasonUserRequest = 4;

private:
    float readTemperatureC();
    uint16_t readVbatMilliVolts();

    uint32_t lastUpdateMs_ = 0;
    float lastTempC_ = 0.0f;
    uint16_t lastVbatMv_ = 0;
    bool throttling_ = false;
    bool shutdown_ = false;
    uint8_t shutdownReason_ = kReasonNone;
};

/// Singleton instance, defined in Power.cpp
extern Power g_power;

}  // namespace phm::hal
