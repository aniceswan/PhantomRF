/**
 * @file WifiAttack.h
 * @brief ESP32-S3 built-in WiFi attacks
 *
 * Implements the 802.11 management-frame injection attacks documented
 * in DESIGN §5.3:
 *   - Deauth       (broadcast + targeted, rotates reason codes)
 *   - Disassoc     (same machinery as deauth, different frame type)
 *   - BeaconFlood  (random SSID, configurable rate)
 *   - ProbeFlood   (probe request to every channel)
 *
 * M1 will add: PMKID capture (EAPOL monitor), evil-twin AP, Karma.
 *
 * The implementation uses `esp_wifi_80211_tx()` (the Espressif-provided
 * raw 802.11 TX path) — this is the only legal way to send arbitrary
 * management frames on an ESP32 without resorting to monitor mode.
 *
 * The AP interface is paused while an attack is in progress (the
 * SoftAP stops beaconing) so the radio can use the channel freely.
 * `teardown()` always restores the AP.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/Module.h"

namespace phm::modules {

/**
 * @brief Built-in WiFi attack suite
 *
 * Module ID 0x03. Uses the Espressif `esp_wifi_80211_tx()` API; no
 * monitor mode required.
 */
class WifiAttack : public ::phm::IModule {
public:
    static constexpr uint8_t  MODULE_ID   = 0x03;
    static constexpr const char* MODULE_NAME = "wifi-attack";
    static constexpr const char* MODULE_DESC =
        "WiFi deauth, disassoc, beacon flood, probe flood";

    uint8_t id() const override { return MODULE_ID; }
    const char* name() const override { return MODULE_NAME; }
    const char* description() const override { return MODULE_DESC; }

    void setup() override;
    void loop() override;
    void teardown() override;

    enum class Target : uint8_t {
        Deauth       = 0,
        Disassoc     = 1,
        BeaconFlood  = 2,
        ProbeFlood   = 3,
    };

    struct AttackConfig {
        Target target = Target::Deauth;
        uint8_t wifiChannel = 6;     ///< 1..13
        bool targetAllChannels = true;
        uint8_t targetBssid[6] = {0};  ///< zero = broadcast
        uint8_t targetClient[6] = {0}; ///< zero = broadcast
        uint16_t ratePerSec = 50;     ///< frames per second
    };

    bool startAttack(const AttackConfig& cfg);
    void stopAttack();
    bool isRunning() const { return running_; }

    /// Snapshot as JSON string (caller must free with free())
    char* toJson() const;

    /// Frame counters (for the web UI)
    uint32_t framesSent() const { return framesSent_; }
    uint32_t framesFailed() const { return framesFailed_; }

    static constexpr const char* kTag = "wifi";

private:
    void workerLoop();
    static void taskEntry(void* arg);

    // Frame builders
    size_t buildDeauthFrame(uint8_t* out, size_t maxLen,
                            const uint8_t* bssid, const uint8_t* client,
                            bool disassoc, uint16_t reason);
    size_t buildBeaconFrame(uint8_t* out, size_t maxLen,
                            const uint8_t* bssid, const char* ssid,
                            uint8_t channel, uint16_t seq);
    size_t buildProbeReqFrame(uint8_t* out, size_t maxLen,
                              const uint8_t* src, const char* ssid);
    void randomMac(uint8_t* out);
    void randomSsid(char* out, size_t len);

    void enterRawMode();
    void leaveRawMode();

    AttackConfig config_{};
    bool         running_      = false;
    bool         rawMode_      = false;
    uint8_t      currentCh_    = 1;
    void*        task_         = nullptr;  /// FreeRTOS task handle (void* to avoid header chain)

    uint32_t framesSent_   = 0;
    uint32_t framesFailed_ = 0;
};

extern WifiAttack g_wifiAttack;

}  // namespace phm::modules
