/**
 * @file Nrf24Jammer.h
 * @brief 2.4 GHz jammer module — drives 1..5 nRF24L01 modules
 *
 * This module owns all 2.4 GHz jamming operations. It reads the
 * user-configured number of nRF24 modules and their pin assignments
 * from NVS (via `phm::hal::g_storage`), then runs a FreeRTOS worker
 * task that streams a constant carrier (DESIGN §10.1) through the
 * channel list / random / brute-force pattern that the user chose.
 *
 * The module is a single `IModule` subclass, so the orchestrator
 * (`phm::PhantomRF`) drives `setup()` / `loop()` / `teardown()` without
 * needing to know what is happening inside the worker task.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Module.h"

#include <stdint.h>

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace phm::modules {

/**
 * @brief 2.4 GHz jammer — Bluetooth, BLE, drone, WiFi overlap, Zigbee
 *
 * Module ID 0x01 (lowest of the attack-module range; 0x00 is reserved).
 * Holds a single FreeRTOS worker task that drives one channel at a
 * time (or, with multiple nRF24s, the channels round-robin in
 * `SweepSeparate` mode). All heavy work happens in the worker so
 * `loop()` can stay non-blocking.
 */
class Nrf24Jammer : public ::phm::IModule {
public:
    static constexpr uint8_t MODULE_ID = 0x01;
    static constexpr const char* MODULE_NAME = "nrf24-jammer";
    static constexpr const char* MODULE_DESC = "2.4 GHz jammer using nRF24L01 modules";

    /// IModule: stable id used by the event bus and web API
    uint8_t id() const override { return MODULE_ID; }

    /// IModule: short, human-readable name
    const char* name() const override { return MODULE_NAME; }

    /// IModule: one-line description for help screens
    const char* description() const override { return MODULE_DESC; }

    // ---- Lifecycle -------------------------------------------------------
    void setup() override;
    void loop() override;
    void teardown() override;

    // ---- Attack configuration -------------------------------------------

    /// What protocol/family are we jamming?
    enum class Target : uint8_t {
        Bluetooth = 0,  ///< BT classic AFH set (21 channels) + random + brute
        BleAdv = 1,     ///< BLE advertising (3 channels: 2, 26, 80)
        BleData = 2,    ///< BLE data channels (0..39 every other channel)
        Drone = 3,      ///< Full ISM sweep 0..125 (catches drone RC)
        Wifi = 4,       ///< WiFi overlap (nRF24 channels covering ch 1..14)
        Zigbee = 5,     ///< 802.15.4 overlap (nRF24 channels covering ch 11..26)
        Misc = 6,       ///< User-supplied channel range
    };

    /// How do we pick the next channel each iteration?
    enum class Method : uint8_t {
        List = 0,        ///< Walk a predefined list (BT AFH, BLE adv, etc.)
        Random = 1,      ///< Pick a random channel per loop per radio
        BruteForce = 2,  ///< Step sequentially through the full range
    };

    /// How do multiple nRF24s cooperate?
    enum class SweepMode : uint8_t {
        Together = 0,  ///< All radios on the same channel (max localised punch)
        Separate = 1,  ///< Round-robin `i = ch % radioCount` (max coverage)
    };

    /// User-tweakable parameters for `startAttack()`
    struct AttackConfig {
        Target target = Target::Bluetooth;      ///< What to jam
        Method method = Method::List;           ///< Channel-selection strategy
        SweepMode sweep = SweepMode::Separate;  ///< Multi-radio cooperation
        uint8_t miscStartCh = 0;                ///< `Misc` only
        uint8_t miscStopCh = 80;                ///< `Misc` only, inclusive
    };

    // ---- Public API ------------------------------------------------------

    /**
     * @brief Start a jamming attack
     *
     * Spawns the worker task pinned to Core 0 at priority 3 (see
     * DESIGN §3.3). If a previous attack is still running it is stopped
     * first.
     *
     * @return true on successful task creation, false otherwise
     */
    bool startAttack(const AttackConfig& cfg);

    /// Stop the worker (if any) and wait for it to exit
    void stopAttack();

    /// True between `startAttack()` and the worker observing `running_ = false`
    bool isRunning() const { return task_ != nullptr; }

    /// Read-only access to the currently-active config
    const AttackConfig& currentConfig() const { return config_; }

private:
    // ---- Worker entry points (per target) --------------------------------
    void workerThread();  ///< Top-level loop, dispatches to a jam*()
    void jamBluetooth();  ///< `Target::Bluetooth`
    void jamBleAdv();     ///< `Target::BleAdv`
    void jamBleData();    ///< `Target::BleData`
    void jamDrone();      ///< `Target::Drone`
    void jamWifi();       ///< `Target::Wifi`
    void jamZigbee();     ///< `Target::Zigbee`
    void jamMisc();       ///< `Target::Misc`

    // ---- Helpers ---------------------------------------------------------
    /// Post an `AttackProgress` event with the channel currently being jammed
    void postProgress(uint8_t channel);

    /// Stop the radio cleanly (used by `stopAttack()` and the per-target helpers)
    void radioStopAll();

    // ---- State -----------------------------------------------------------
    AttackConfig config_{};
    bool running_ = false;        ///< Worker loop guard
    uint8_t currentChannel_ = 0;  ///< Last channel posted as progress

    TaskHandle_t task_ = nullptr;  ///< Owned by `startAttack()` / `stopAttack()`

    /// FreeRTOS task entry — trampoline to the C++ instance method
    static void taskEntry(void* arg);

    // ---- Predefined channel tables (from W0rthlessS0ul config.h:39-41) ---
    /// Bluetooth classic AFH set (21 channels)
    static const uint8_t BT_CHANNELS[21];
    /// BLE advertising channels mapped to nRF24 channels
    static const uint8_t BLE_ADV_CHANNELS[3];
};

/// Process-wide instance; defined in Nrf24Jammer.cpp
extern Nrf24Jammer g_nrf24Jammer;

}  // namespace phm::modules
