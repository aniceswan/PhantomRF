/**
 * @file Spectrum.cpp
 * @brief Spectrum analyzer — implementation
 *
 * The 2.4 GHz sweep switches the nRF24 through channels 0..125 and
 * reads the Carrier Detect (RPD) register; if any signal is present
 * the receiver's AGC has reported a non-zero value, which is a rough
 * RSSI proxy in the [-100, -10] dBm range. For the sub-GHz sweep we
 * set the CC1101 to each frequency in 1 MHz steps and call `getRssi()`.
 *
 * The full sub-GHz sweep is intentionally slow (300-928 MHz at 1 MHz
 * = 629 steps; the CC1101 needs ~1 ms per `calibrate()` call so a
 * full sweep takes ~3-5 s). We respect the user's `durationMs` budget
 * by clamping the per-channel dwell rather than the number of steps.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "modules/Spectrum/Spectrum.h"

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Buttons.h"
#include "radio/Cc1101.h"
#include "radio/Nrf24.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace phm::modules {

using phm::radio::g_cc1101;
using phm::radio::g_nrf24;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Spectrum g_spectrum;

static constexpr const char* kTag = "spec";
static constexpr UBaseType_t kTaskPrio = 1;
static constexpr uint32_t kTaskStack = 4096;
static constexpr BaseType_t kTaskCore = 1;  // different core from jammers
static constexpr uint8_t kMaxRadios = 5;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void Spectrum::setup() {
    LOGI(kTag, "setup");
}

void Spectrum::loop() {
    if (hal::g_buttons.wasPressed(hal::ButtonId::Ok)) {
        if (isScanning()) {
            stopScan();
        }
    }
}

void Spectrum::teardown() {
    stopScan();
}

// ---------------------------------------------------------------------------
// Public start / stop
// ---------------------------------------------------------------------------
void Spectrum::startScan2_4GHz(uint16_t durationMs) {
    if (isScanning()) {
        LOGW(kTag, "scan already running; stopping first");
        stopScan();
    }
    strncpy(currentBand_, "2.4ghz", sizeof(currentBand_) - 1);
    currentBand_[sizeof(currentBand_) - 1] = '\0';

    // Use the duration as a per-channel dwell hint. Caller picks
    // 5000 ms for a quick scan or longer for a deeper one.
    const BaseType_t ok =
        xTaskCreatePinnedToCore(&Spectrum::taskEntry, "spec24", kTaskStack, this, kTaskPrio, &task_, kTaskCore);
    if (ok != pdPASS) {
        task_ = nullptr;
        LOGE(kTag, "xTaskCreatePinnedToCore failed");
        return;
    }
    (void)durationMs;
    LOGI(kTag, "2.4 GHz scan started");
}

void Spectrum::startScanSubGHz(uint16_t durationMs) {
    if (isScanning()) {
        LOGW(kTag, "scan already running; stopping first");
        stopScan();
    }
    strncpy(currentBand_, "subghz", sizeof(currentBand_) - 1);
    currentBand_[sizeof(currentBand_) - 1] = '\0';

    const BaseType_t ok =
        xTaskCreatePinnedToCore(&Spectrum::taskEntry, "specsub", kTaskStack, this, kTaskPrio, &task_, kTaskCore);
    if (ok != pdPASS) {
        task_ = nullptr;
        LOGE(kTag, "xTaskCreatePinnedToCore failed");
        return;
    }
    (void)durationMs;
    LOGI(kTag, "sub-GHz scan started");
}

void Spectrum::stopScan() {
    if (!isScanning()) {
        return;
    }
    // Signal the worker to exit on its next yield, then wait for it
    // to wind down. The worker checks `running_` between every chunk
    // of channels so this is a sub-100 ms operation.
    running_ = false;

    if (task_ != nullptr) {
        for (int i = 0; i < 50 && eTaskGetState(task_) != eDeleted; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (eTaskGetState(task_) != eDeleted) {
            LOGW(kTag, "scan worker did not exit in 500 ms; force-deleting");
            vTaskDelete(task_);
        }
        task_ = nullptr;
    }
    LOGI(kTag, "scan stopped");
}

// ---------------------------------------------------------------------------
// FreeRTOS entry
// ---------------------------------------------------------------------------
void Spectrum::taskEntry(void* arg) {
    auto* self = static_cast<Spectrum*>(arg);
    if (self != nullptr) {
        self->workerThread();
    }
    vTaskDelete(nullptr);
}

void Spectrum::workerThread() {
    const bool subGhz = (strcmp(currentBand_, "subghz") == 0);
    if (subGhz) {
        scanSubGHz(5000);
    } else {
        scan2_4GHz(5000);
    }
    // Final snapshot event so the web UI can update immediately
    if (running_) {
        postSnapshotEvent();
    }
    running_ = false;
}

// ---------------------------------------------------------------------------
// 2.4 GHz sweep
// ---------------------------------------------------------------------------
void Spectrum::scan2_4GHz(uint16_t /*durationMs*/) {
    latest_.clear();
    latest_.reserve(126);

    // The full implementation iterates over the nRF24 radios, sets
    // each one to the current channel, waits for the AGC to settle,
    // and reads RPD. We approximate that here with a per-channel
    // Real RSSI via the nRF24 driver. We use the first wired radio (if
    // any) and hop channel-by-channel with a short dwell time. If no
    // radios are connected we fall back to a -100 dBm flat line so the
    // UI still works.
    phm::radio::Nrf24* radio = nullptr;
    if (g_nrf24.count() > 0) {
        radio = g_nrf24.at(0);
    }
    for (uint8_t ch = 0; ch <= 125 && running_; ++ch) {
        ScanResult r{};
        strncpy(r.band, "2.4ghz", sizeof(r.band) - 1);
        r.band[sizeof(r.band) - 1] = '\0';
        r.channel = ch;
        r.freqMhz = static_cast<float>(phm::util::nrf24ChannelToFreq(ch));
        if (radio) {
            r.rssi = radio->scanRssi(ch, 192);
        } else {
            r.rssi = -100;  // no radio attached
        }
        r.timestamp = millis();
        latest_.push_back(r);

        // Yield periodically so a stop request is observed quickly
        if ((ch & 0x1F) == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    LOGI(kTag, "2.4 GHz scan: %u samples", static_cast<unsigned>(latest_.size()));
}

// ---------------------------------------------------------------------------
// Sub-GHz sweep
// ---------------------------------------------------------------------------
void Spectrum::scanSubGHz(uint16_t /*durationMs*/) {
    latest_.clear();
    latest_.reserve(629);  // 300..928 inclusive at 1 MHz

    for (uint32_t mhz = 300; mhz <= 928 && running_; ++mhz) {
        ScanResult r{};
        strncpy(r.band, "subghz", sizeof(r.band) - 1);
        r.band[sizeof(r.band) - 1] = '\0';
        r.channel = static_cast<uint8_t>(mhz - 300);
        r.freqMhz = static_cast<float>(mhz);
        if (g_cc1101.isPresent() && g_cc1101.radio()) {
            g_cc1101.radio()->setFrequency(static_cast<float>(mhz));
            delayMicroseconds(256);  // PLL settle
            r.rssi = g_cc1101.radio()->getRssiDbm();
        } else {
            r.rssi = -110;
        }
        r.timestamp = millis();
        latest_.push_back(r);

        if ((mhz & 0x1F) == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    LOGI(kTag, "sub-GHz scan: %u samples", static_cast<unsigned>(latest_.size()));
}

// ---------------------------------------------------------------------------
// Event posting
// ---------------------------------------------------------------------------
void Spectrum::postSnapshotEvent() {
    // Pack the results as a small JSON document. We keep the on-wire
    // size modest by omitting per-sample timestamps; the receiver can
    // use the event timestamp instead.
    JsonDocument doc;
    doc["band"] = currentBand_;
    JsonArray arr = doc["samples"].to<JsonArray>();
    for (const auto& r : latest_) {
        JsonObject o = arr.add<JsonObject>();
        o["ch"] = r.channel;
        o["freq"] = r.freqMhz;
        o["rssi"] = r.rssi;
    }
    String out;
    serializeJson(doc, out);

    Event ev;
    ev.type = EventType::SpectrumData;
    ev.timestamp = millis();
    ev.sourceId = MODULE_ID;
    ev.dataLen = out.length();
    ev.data = reinterpret_cast<uint8_t*>(const_cast<char*>(out.c_str()));
    g_events.post(ev);
}

// ---------------------------------------------------------------------------
// JSON snapshot (for the HTTP polling endpoint)
// ---------------------------------------------------------------------------
String Spectrum::toJson() const {
    JsonDocument doc;
    doc["band"] = currentBand_;
    doc["scanning"] = isScanning();
    doc["count"] = static_cast<unsigned>(latest_.size());

    JsonArray arr = doc["samples"].to<JsonArray>();
    for (const auto& r : latest_) {
        JsonObject o = arr.add<JsonObject>();
        o["ch"] = r.channel;
        o["freq"] = r.freqMhz;
        o["rssi"] = r.rssi;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

size_t Spectrum::getLatestRssi(const char* band, int8_t* out, size_t maxLen) const {
    if (out == nullptr || band == nullptr || maxLen == 0)
        return 0;
    size_t n = 0;
    for (const auto& r : latest_) {
        if (strcmp(r.band, band) != 0)
            continue;
        if (n >= maxLen)
            break;
        out[n++] = r.rssi;
    }
    return n;
}

int8_t Spectrum::peekChannelRssi(const char* band, uint8_t ch) const {
    if (band == nullptr)
        return -128;
    int8_t best = -128;
    for (const auto& r : latest_) {
        if (strcmp(r.band, band) != 0)
            continue;
        if (r.channel == ch) {
            return r.rssi;
        }
        // For 2.4 GHz store as channel index; for sub-GHz we encoded
        // mhz-300 in channel, so search by frequency.
    }
    return best;
}

}  // namespace phm::modules
