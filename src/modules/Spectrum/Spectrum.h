/**
 * @file Spectrum.h
 * @brief Live RSSI spectrum analyzer
 *
 * Sweeps either the 2.4 GHz ISM band (nRF24 channels 0..125) or the
 * sub-GHz band (CC1101, 300..928 MHz in 1 MHz steps) and reports
 * per-channel RSSI. The web UI polls `latestResults()` for the
 * waterfall view; this module does not push unsolicited updates.
 *
 * The scan worker runs on Core 1 at default priority (DESIGN §3.3)
 * so it does not compete with the nRF24 / CC1101 jammer workers on
 * Core 0. A scan can run in parallel with an attack on the *other*
 * band.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <stdint.h>
#include <vector>

#include "core/Module.h"

// forward decl removed: use FreeRTOS TaskHandle_t via <freertos/task.h>
// typedef removed: use FreeRTOS TaskHandle_t via <freertos/task.h>

namespace phm::modules {

/**
 * @brief 2.4 GHz + sub-GHz RSSI analyzer
 *
 * Module ID 0x05. Maintains a snapshot of the most recent sweep;
 * the web UI pulls it on its 1 Hz poll. If `onResultReady` is wired
 * up, the module also posts `EventType::SpectrumData` events as the
 * scan progresses.
 */
class Spectrum : public ::phm::IModule {
public:
    static constexpr uint8_t  MODULE_ID   = 0x05;
    static constexpr const char* MODULE_NAME = "spectrum";
    static constexpr const char* MODULE_DESC =
        "Live RSSI spectrum analyzer (2.4 GHz and sub-GHz)";

    uint8_t id() const override { return MODULE_ID; }
    const char* name() const override { return MODULE_NAME; }
    const char* description() const override { return MODULE_DESC; }

    void setup() override;
    void loop() override;
    void teardown() override;

    // ---- One-band scans --------------------------------------------------

    void startScan2_4GHz(uint16_t durationMs = 5000);
    void startScanSubGHz(uint16_t durationMs = 5000);

    /// Stop the in-progress scan (if any). Safe to call when idle.
    void stopScan();

    /// True between start* and the worker exit
    bool isScanning() const { return task_ != nullptr; }

    // ---- Result accessors ------------------------------------------------

    /// One RSSI sample
    struct ScanResult {
        char     band[8];       ///< "2.4ghz" or "subghz"
        uint8_t  channel;       ///< 0..125 for 2.4 GHz, 0..255 for sub-GHz
        float    freqMhz;       ///< Centre frequency, MHz
        int8_t   rssi;          ///< dBm
        uint32_t timestamp;     ///< millis() at sample time
    };

    /// Read-only access to the most recent full sweep's results
    const std::vector<ScanResult>& latestResults() const { return latest_; }

    /// Get a JSON snapshot for the web API: `{ band, samples: [{ch, freq, rssi}] }`
    String toJson() const;

    /// Fill `out` with the RSSI values from the most recent sweep filtered
    /// to the given band ("2.4ghz" or "subghz"). Returns the number of
    /// values written (clamped to `maxLen`).
    size_t getLatestRssi(const char* band, int8_t* out, size_t maxLen) const;

    /// Peek a single channel's last-known RSSI (for nRF24 0..125). Returns
    /// -128 if the channel hasn't been sampled.
    int8_t peekChannelRssi(const char* band, uint8_t ch) const;

    /// True once `latest_` has been populated at least once
    bool hasResults() const { return !latest_.empty(); }

private:
    void workerThread();
    void scan2_4GHz(uint16_t durationMs);
    void scanSubGHz(uint16_t durationMs);

    /// Post a `SpectrumData` event with the current `latest_` snapshot
    void postSnapshotEvent();

    static void taskEntry(void* arg);

    // ---- State -----------------------------------------------------------
    bool         running_      = false;  ///< Worker loop guard
    char         currentBand_[8] = "";    ///< "2.4ghz" or "subghz"
    TaskHandle_t task_         = nullptr;

    std::vector<ScanResult> latest_;   ///< Most recent completed sweep
};

extern Spectrum g_spectrum;

}  // namespace phm::modules
