/**
 * @file Cc1101Jammer.h
 * @brief Sub-GHz jammer module — drives 1..2 CC1101 modules
 *
 * Owns the 300-928 MHz jamming operations: spot (single frequency),
 * range (sweep), hopper (user-supplied list), and keyfob (predefined
 * common keyfob bands). Also exposes record/replay hooks for raw
 * OOK/FSK capture and retransmission.
 *
 * The worker task is pinned to Core 0 at priority 3 (DESIGN §3.3) so
 * it does not contend with the web server or main loop.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Module.h"

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// forward decl removed: use FreeRTOS TaskHandle_t via <freertos/task.h>
// typedef removed: use FreeRTOS TaskHandle_t via <freertos/task.h>

namespace phm::modules {

/**
 * @brief Sub-GHz jammer — 300-928 MHz via CC1101
 *
 * Module ID 0x02. Single worker task, one or two radios (only one
 * present in M0; the second is M1 per DESIGN §4.3).
 */
class Cc1101Jammer : public ::phm::IModule {
public:
    static constexpr uint8_t MODULE_ID = 0x02;
    static constexpr const char* MODULE_NAME = "cc1101-jammer";
    static constexpr const char* MODULE_DESC = "Sub-GHz jammer using CC1101 module";

    uint8_t id() const override { return MODULE_ID; }
    const char* name() const override { return MODULE_NAME; }
    const char* description() const override { return MODULE_DESC; }

    void setup() override;
    void loop() override;
    void teardown() override;

    // ---- Attack configuration -------------------------------------------

    /// Which sub-mode are we in?
    enum class Target : uint8_t {
        Spot = 0,    ///< Single frequency
        Range = 1,   ///< Sweep start..stop with given step
        Hopper = 2,  ///< User-supplied frequency list, round-robin
        Keyfob = 3,  ///< Predefined 9 keyfob bands
    };

    /// User-tweakable parameters for `startAttack()`
    struct AttackConfig {
        Target target = Target::Spot;
        float freqMhz = 433.92f;            ///< Spot: single freq
        float rangeStart = 430.0f;          ///< Range: start MHz
        float rangeStop = 450.0f;           ///< Range: stop MHz
        float rangeStep = 1.0f;             ///< Range: step MHz
        float hopperFreqs[16] = {};         ///< Hopper: up to 16 freqs
        uint8_t hopperCount = 0;            ///< Hopper: number of valid entries
        uint8_t keyfobIndex = 8;            ///< Keyfob: index into the table (default 433.92)
        uint8_t payloadSize = 60;           ///< Bytes pushed before CW enable
        uint16_t recordDurationMs = 30000;  ///< Record: cap on capture time
    };

    bool startAttack(const AttackConfig& cfg);
    void stopAttack();
    bool isRunning() const { return task_ != nullptr; }

    const AttackConfig& currentConfig() const { return config_; }

    /// Predefined keyfob frequencies, in MHz (9 bands, 300..433.92)
    static const float KEYFOB_FREQS[9];
    static constexpr uint8_t KEYFOB_COUNT = 9;

private:
    // ---- Per-target worker entry points ----------------------------------
    void workerThread();
    void jamSpot();
    void jamRange();
    void jamHopper();
    void jamKeyfob();

    /// Record a raw OOK/FSK bitstream into a RAM buffer (DESIGN §10.2)
    void recordSignal();

    /// Replay a previously-recorded bitstream
    void replaySignal();

    // ---- Helpers ---------------------------------------------------------
    void postProgress(float freqMhz);
    void setRadioFrequency(float mhz);

    // ---- State -----------------------------------------------------------
    AttackConfig config_{};
    bool running_ = false;

    TaskHandle_t task_ = nullptr;

    static void taskEntry(void* arg);
};

extern Cc1101Jammer g_cc1101Jammer;

}  // namespace phm::modules
