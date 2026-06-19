/**
 * @file BleAttack.h
 * @brief BLE attacks — scan, Apple/Samsung/Windows/FastPair spam
 *
 * Uses NimBLE-Arduino 2.x for scan and advertising. The `AdvJam`
 * target is a thin wrapper that delegates to `phm::modules::g_nrf24Jammer`
 * (the nRF24 module does the actual 2.4 GHz carrier on BLE adv
 * channels 2/26/80; NimBLE is not involved).
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Module.h"

#include <vector>

#include <Arduino.h>
#include <stdint.h>

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace phm::modules {

/**
 * @brief BLE attack suite
 *
 * Module ID 0x04. Owns the NimBLE advertising state for the spam
 * targets. Scan and spam use the same radio (NimBLE on the ESP32-S3),
 * so they cannot run concurrently.
 */
class BleAttack : public ::phm::IModule {
public:
    static constexpr uint8_t MODULE_ID = 0x04;
    static constexpr const char* MODULE_NAME = "ble-attack";
    static constexpr const char* MODULE_DESC = "BLE scan, Apple Continuity spam, Samsung/Windows/FastPair spam";

    uint8_t id() const override { return MODULE_ID; }
    const char* name() const override { return MODULE_NAME; }
    const char* description() const override { return MODULE_DESC; }

    void setup() override;
    void loop() override;
    void teardown() override;

    // ---- Attack configuration -------------------------------------------

    /// Which sub-mode are we running?
    enum class Target : uint8_t {
        Scan = 0,          ///< List nearby BLE devices for `scanDurationMs`
        AdvJam = 1,        ///< Jam BLE advertising via nRF24 (delegated)
        AppleSpam = 2,     ///< Apple Continuity popups (M1 per DESIGN)
        SamsungSpam = 3,   ///< Galaxy Buds popups (M1)
        WindowsSpam = 4,   ///< Windows Swift Pair (M1)
        FastPairSpam = 5,  ///< Google Fast Pair (M1)
    };

    struct AttackConfig {
        Target target = Target::Scan;
        uint16_t scanDurationMs = 5000;  ///< Scan: time spent scanning
    };

    /// One entry from the last scan
    struct BleDevice {
        char address[18];  ///< "AA:BB:CC:DD:EE:FF\0"
        char name[32];     ///< Advertised name, may be empty
        int8_t rssi;
        uint16_t manufacturerId = 0;
    };

    bool startAttack(const AttackConfig& cfg);
    void stopAttack();
    bool isRunning() const { return running_; }

    /// Snapshot of the most recent scan results
    std::vector<BleDevice>& lastScanResults() { return lastResults_; }
    const std::vector<BleDevice>& lastScanResults() const { return lastResults_; }

private:
    // ---- Worker entries --------------------------------------------------
    void workerThread();
    void runScan();
    void runAdvJam();  ///< Delegates to g_nrf24Jammer
    void runAppleSpam();
    void runSamsungSpam();
    void runWindowsSpam();
    void runFastPairSpam();

    /// Stop any NimBLE advertising we started
    void stopAdvertising();

    /// Payload builders (return payload bytes + length)
    static void buildApplePayload(uint8_t* out, size_t& outLen);
    static void buildSamsungPayload(uint8_t* out, size_t& outLen);
    static void buildWindowsPayload(uint8_t* out, size_t& outLen);
    static void buildFastPairPayload(uint8_t* out, size_t& outLen);

    // ---- State -----------------------------------------------------------
    AttackConfig config_{};
    bool running_ = false;

    /// Result of the last scan (cleared at the start of each scan)
    std::vector<BleDevice> lastResults_;

    /// Set true when we own NimBLE advertising
    bool owningAdvertising_ = false;

    /// Spam worker (only used for the Apple/Samsung/Windows/FastPair targets)
    TaskHandle_t task_ = nullptr;
    static void taskEntry(void* arg);
};

extern BleAttack g_bleAttack;

}  // namespace phm::modules
