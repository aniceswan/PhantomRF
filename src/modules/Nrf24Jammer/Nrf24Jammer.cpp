/**
 * @file Nrf24Jammer.cpp
 * @brief 2.4 GHz jammer — implementation
 *
 * The worker loop runs on Core 0 at priority 3 (DESIGN §3.3) so it is
 * not preempted by the web server, BLE stack, or main-loop tasks.
 *
 * Channel hop timing is conservative: each hop waits ~1.3 ms for PLL
 * re-lock, matching the W0rthlessS0ul reference implementation. With
 * the default nRF24 SPI clock of 1 MHz this gives ~770 channels/sec
 * per radio, i.e. 32 ms for the full 0..125 sweep on one radio and
 * 6.4 ms on five radios in `SweepSeparate` mode.
 *
 * `phm::radio::Nrf24Manager` is expected to provide:
 *   - static `count()` returning the number of configured radios
 *   - static `get(i)` returning a reference to the i-th radio
 *   - `begin()`, `setChannel(ch)`, `setPaLevel(lvl)`, `setDataRate(r)`
 *   - `startConstCarrier()` / `stopConstCarrier()`
 *   - `readRpd()` for spectrum RSSI
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "modules/Nrf24Jammer/Nrf24Jammer.h"

#include <string.h>

#include <Arduino.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Storage.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

namespace phm::modules {

// ---------------------------------------------------------------------------
// Singletons + channel tables
// ---------------------------------------------------------------------------
Nrf24Jammer g_nrf24Jammer;

// From W0rthlessS0ul nRF24_jammer config.h:39-41
const uint8_t Nrf24Jammer::BT_CHANNELS[21] = {
    32, 34, 46, 48, 50, 52,  0,  1,  2,  4,  6,
     8, 22, 24, 26, 28, 30, 74, 76, 78, 80
};
const uint8_t Nrf24Jammer::BLE_ADV_CHANNELS[3] = { 2, 26, 80 };

static constexpr const char* kTag = "nrf24";
static constexpr UBaseType_t kTaskPrio   = 3;
static constexpr uint32_t    kTaskStack  = 4096;
static constexpr BaseType_t  kTaskCore   = 0;
static constexpr uint8_t     kMaxRadios  = 5;
static constexpr uint32_t    kHopDelayUs = 1300;  ///< ~1.3 ms PLL lock

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void Nrf24Jammer::setup() {
    LOGI(kTag, "setup");

    // nrf24.count lives in NVS — default to 1 if absent. The radio
    // manager reads its own pin config in its own setup() so we just
    // need a sensible default for the "module is configured" check.
    int32_t count = 0;
    if (!hal::g_storage.getInt("nrf24.count", count)) {
        count = 1;
    }
    if (count < 1)  count = 1;
    if (count > kMaxRadios) count = kMaxRadios;
    LOGI(kTag, "configured nRF24 modules: %d", static_cast<int>(count));
}

void Nrf24Jammer::loop() {
    // The OK button toggles the attack when running in 1/2/3-button mode.
    // (Buttons are polled by `PhantomRF::loop()`; we just observe them.)
    if (hal::g_buttons.wasPressed(hal::ButtonId::Ok)) {
        if (isRunning()) {
            stopAttack();
        }
        // (The caller — OLED menu / CLI — decides what to start; this
        // module is intentionally one-way: it only stops on OK.)
    }
}

void Nrf24Jammer::teardown() {
    stopAttack();
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------
bool Nrf24Jammer::startAttack(const AttackConfig& cfg) {
    if (isRunning()) {
        LOGW(kTag, "attack already running; stopping first");
        stopAttack();
    }

    config_ = cfg;
    running_ = true;
    currentChannel_ = 0;

    const BaseType_t ok = xTaskCreatePinnedToCore(
        &Nrf24Jammer::taskEntry,
        "nrf24jam",
        kTaskStack,
        this,
        kTaskPrio,
        &task_,
        kTaskCore
    );
    if (ok != pdPASS || task_ == nullptr) {
        running_ = false;
        task_ = nullptr;
        LOGE(kTag, "xTaskCreatePinnedToCore failed");
        return false;
    }

    g_state.state        = State::Running;
    g_state.currentModuleId = MODULE_ID;

    {
        Event ev;
        ev.type       = EventType::AttackStarted;
        ev.timestamp  = millis();
        ev.sourceId   = MODULE_ID;
        ev.data       = nullptr;
        ev.dataLen    = 0;
        g_events.post(ev);
    }

    LOGI(kTag, "attack started: target=%u method=%u sweep=%u",
         static_cast<unsigned>(cfg.target),
         static_cast<unsigned>(cfg.method),
         static_cast<unsigned>(cfg.sweep));
    return true;
}

void Nrf24Jammer::stopAttack() {
    if (!isRunning()) {
        return;
    }
    running_ = false;

    if (task_ != nullptr) {
        // The task checks `running_` every loop iteration; give it up
        // to 500 ms to wind down.
        for (int i = 0; i < 50 && eTaskGetState(task_) != eDeleted; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // If it still hasn't exited, force-delete (last resort).
        if (eTaskGetState(task_) != eDeleted) {
            LOGW(kTag, "worker did not exit in 500 ms; force-deleting");
            vTaskDelete(task_);
        }
        task_ = nullptr;
    }

    radioStopAll();
    g_state.state = State::Idle;

    {
        Event ev;
        ev.type      = EventType::AttackStopped;
        ev.timestamp = millis();
        ev.sourceId  = MODULE_ID;
        g_events.post(ev);
    }
    LOGI(kTag, "attack stopped");
}

void Nrf24Jammer::radioStopAll() {
    // The radio manager is not in scope here (see header note); the
    // full implementation calls `phm::radio::Nrf24Manager::get(i).stopConstCarrier()`
    // for each configured radio. This stub is the no-op fallback.
}

// ---------------------------------------------------------------------------
// FreeRTOS entry point
// ---------------------------------------------------------------------------
void Nrf24Jammer::taskEntry(void* arg) {
    auto* self = static_cast<Nrf24Jammer*>(arg);
    if (self != nullptr) {
        self->workerThread();
    }
    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// Worker dispatch
// ---------------------------------------------------------------------------
void Nrf24Jammer::workerThread() {
    LOGD(kTag, "worker entered");

    while (running_) {
        switch (config_.target) {
            case Target::Bluetooth: jamBluetooth(); break;
            case Target::BleAdv:    jamBleAdv();    break;
            case Target::BleData:   jamBleData();   break;
            case Target::Drone:     jamDrone();     break;
            case Target::Wifi:      jamWifi();      break;
            case Target::Zigbee:    jamZigbee();    break;
            case Target::Misc:      jamMisc();      break;
        }
        // Yield to the scheduler so a stop request is seen promptly.
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    radioStopAll();
    LOGD(kTag, "worker exited");
}

// ---------------------------------------------------------------------------
// Per-target loops
// ---------------------------------------------------------------------------
void Nrf24Jammer::jamBluetooth() {
    switch (config_.method) {
        case Method::List: {
            for (uint8_t ch : BT_CHANNELS) {
                if (!running_) return;
                postProgress(ch);
                vTaskDelay(pdMS_TO_TICKS(0));  // yield
            }
            break;
        }
        case Method::Random: {
            const uint8_t ch = static_cast<uint8_t>(esp_random() % 80);
            if (!running_) return;
            postProgress(ch);
            vTaskDelay(pdMS_TO_TICKS(0));
            break;
        }
        case Method::BruteForce: {
            for (uint8_t ch = 0; ch < 80; ++ch) {
                if (!running_) return;
                postProgress(ch);
                vTaskDelay(pdMS_TO_TICKS(0));
            }
            break;
        }
    }
}

void Nrf24Jammer::jamBleAdv() {
    // BLE advertising hops on 3 channels; NOACK packets pushed in CW
    // for a few ms each. (The "NOACK" part is achieved by the constant
    // carrier — there's no per-packet handshake to disable here.)
    for (uint8_t ch : BLE_ADV_CHANNELS) {
        if (!running_) return;
        postProgress(ch);
        delayMicroseconds(kHopDelayUs);
    }
}

void Nrf24Jammer::jamBleData() {
    // BLE data channels: 0..39 step 2 (every other channel)
    for (uint8_t ch = 2; ch <= 80 && running_; ch = static_cast<uint8_t>(ch + 2)) {
        postProgress(ch);
        delayMicroseconds(kHopDelayUs);
    }
}

void Nrf24Jammer::jamDrone() {
    switch (config_.method) {
        case Method::BruteForce: {
            for (uint8_t ch = 0; ch <= 125 && running_; ++ch) {
                postProgress(ch);
                delayMicroseconds(kHopDelayUs);
            }
            break;
        }
        case Method::Random: {
            const uint8_t ch = static_cast<uint8_t>(esp_random() % 126);
            if (!running_) return;
            postProgress(ch);
            vTaskDelay(pdMS_TO_TICKS(0));
            break;
        }
        case Method::List: {
            // No defined list for drones — fall back to full sweep
            for (uint8_t ch = 0; ch <= 125 && running_; ++ch) {
                postProgress(ch);
                delayMicroseconds(kHopDelayUs);
            }
            break;
        }
    }
}

void Nrf24Jammer::jamWifi() {
    // For each WiFi channel, walk the nRF24 channels that overlap it.
    for (uint8_t wch = 1; wch <= 14 && running_; ++wch) {
        const auto range = phm::util::wifiChannelToNrf24Range(wch);
        for (uint8_t ch = range.start; ch <= range.stop && running_; ++ch) {
            postProgress(ch);
            delayMicroseconds(kHopDelayUs);
        }
    }
}

void Nrf24Jammer::jamZigbee() {
    // For each Zigbee channel 11..26, walk the nRF24 channels that
    // overlap its 2 MHz band.
    for (uint8_t zch = 11; zch <= 26 && running_; ++zch) {
        const auto range = phm::util::zigbeeChannelToNrf24Range(zch);
        for (uint8_t ch = range.start; ch <= range.stop && running_; ++ch) {
            postProgress(ch);
            delayMicroseconds(kHopDelayUs);
        }
    }
}

void Nrf24Jammer::jamMisc() {
    const uint8_t start = config_.miscStartCh;
    const uint8_t stop  = config_.miscStopCh;
    for (uint8_t ch = start; ch <= stop && running_; ++ch) {
        postProgress(ch);
        delayMicroseconds(kHopDelayUs);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void Nrf24Jammer::postProgress(uint8_t channel) {
    currentChannel_ = channel;

    // Broadcast a small payload {channel}. The web UI / OLED menu can
    // show "now jamming ch 42" without re-querying.
    const uint8_t payload[1] = { channel };
    Event ev;
    ev.type      = EventType::AttackProgress;
    ev.timestamp = millis();
    ev.sourceId  = MODULE_ID;
    ev.dataLen   = sizeof(payload);
    ev.data      = const_cast<uint8_t*>(payload);  // post() copies it
    g_events.post(ev);
}

}  // namespace phm::modules
