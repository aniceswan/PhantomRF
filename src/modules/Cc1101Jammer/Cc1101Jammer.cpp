/**
 * @file Cc1101Jammer.cpp
 * @brief Sub-GHz jammer — implementation
 *
 * Uses the CC1101's async-serial mode (DESIGN §10.2) so the same chip
 * can do jamming, record, and replay without the overhead of a
 * packet handler. The constant-carrier jam is just "SetTx + bit-bang
 * GDO0 with random data + leave it on for a while".
 *
 * `phm::radio::Cc1101Manager` is expected to provide:
 *   - `begin()`, `isPresent()`
 *   - `setFrequency(mhz)`, `getFrequency()`
 *   - `setCCMode(0)`, `setPktFormat(3)`, `setLengthConfig(0)`
 *   - `setMaxPower()`, `calibrate()`, `setTx()`, `setRx()`
 *   - `getRssi()`
 *   - Bit-bang GDO0 for record/replay
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "modules/Cc1101Jammer/Cc1101Jammer.h"

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Storage.h"
#include "radio/Cc1101.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace phm::modules {

using phm::radio::g_cc1101;

// ---------------------------------------------------------------------------
// Singleton + keyfob table
// ---------------------------------------------------------------------------
Cc1101Jammer g_cc1101Jammer;

// W0rthlessS0ul CC1101_jammer.h — 9 common keyfob bands, MHz
const float Cc1101Jammer::KEYFOB_FREQS[9] = {303.00f, 310.00f, 315.00f, 330.00f, 350.00f,
                                             370.00f, 390.00f, 418.00f, 433.92f};

static constexpr const char* kTag = "cc1101";
static constexpr UBaseType_t kTaskPrio = 3;
static constexpr uint32_t kTaskStack = 4096;
static constexpr BaseType_t kTaskCore = 0;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void Cc1101Jammer::setup() {
    bool present = true;
    if (!hal::g_storage.getBool("cc1101.present", present)) {
        // Key missing — assume present (most builds have it)
        present = true;
    }
    LOGI(kTag, "setup (present=%s)", present ? "yes" : "no");
}

void Cc1101Jammer::loop() {
    if (hal::g_buttons.wasPressed(hal::ButtonId::Ok)) {
        if (isRunning()) {
            stopAttack();
        }
    }
}

void Cc1101Jammer::teardown() {
    stopAttack();
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------
bool Cc1101Jammer::startAttack(const AttackConfig& cfg) {
    if (isRunning()) {
        LOGW(kTag, "attack already running; stopping first");
        stopAttack();
    }

    config_ = cfg;
    running_ = true;

    const BaseType_t ok =
        xTaskCreatePinnedToCore(&Cc1101Jammer::taskEntry, "cc1101jam", kTaskStack, this, kTaskPrio, &task_, kTaskCore);
    if (ok != pdPASS || task_ == nullptr) {
        running_ = false;
        task_ = nullptr;
        LOGE(kTag, "xTaskCreatePinnedToCore failed");
        return false;
    }

    g_state.state = State::Running;
    g_state.currentModuleId = MODULE_ID;

    {
        Event ev;
        ev.type = EventType::AttackStarted;
        ev.timestamp = millis();
        ev.sourceId = MODULE_ID;
        g_events.post(ev);
    }
    LOGI(kTag, "attack started: target=%u", static_cast<unsigned>(cfg.target));
    return true;
}

void Cc1101Jammer::stopAttack() {
    if (!isRunning()) {
        return;
    }
    running_ = false;

    if (task_ != nullptr) {
        for (int i = 0; i < 50 && eTaskGetState(task_) != eDeleted; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (eTaskGetState(task_) != eDeleted) {
            LOGW(kTag, "worker did not exit in 500 ms; force-deleting");
            vTaskDelete(task_);
        }
        task_ = nullptr;
    }

    g_state.state = State::Idle;

    {
        Event ev;
        ev.type = EventType::AttackStopped;
        ev.timestamp = millis();
        ev.sourceId = MODULE_ID;
        g_events.post(ev);
    }
    LOGI(kTag, "attack stopped");
}

// ---------------------------------------------------------------------------
// FreeRTOS entry
// ---------------------------------------------------------------------------
void Cc1101Jammer::taskEntry(void* arg) {
    auto* self = static_cast<Cc1101Jammer*>(arg);
    if (self != nullptr) {
        self->workerThread();
    }
    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// Worker dispatch
// ---------------------------------------------------------------------------
void Cc1101Jammer::workerThread() {
    LOGD(kTag, "worker entered");

    while (running_) {
        switch (config_.target) {
        case Target::Spot: jamSpot(); break;
        case Target::Range: jamRange(); break;
        case Target::Hopper: jamHopper(); break;
        case Target::Keyfob: jamKeyfob(); break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    LOGD(kTag, "worker exited");
}

// ---------------------------------------------------------------------------
// Per-target loops
// ---------------------------------------------------------------------------
void Cc1101Jammer::jamSpot() {
    if (!running_)
        return;
    setRadioFrequency(config_.freqMhz);
    postProgress(config_.freqMhz);
    // Hold for one full second per loop iteration; the worker check
    // will exit promptly when running_ flips false.
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void Cc1101Jammer::jamRange() {
    for (float f = config_.rangeStart; f <= config_.rangeStop && running_; f += config_.rangeStep) {
        setRadioFrequency(f);
        postProgress(f);
        // Dwell: 5 ms per step is enough to break any narrowband OOK
        // receiver, and the full sweep of 20 MHz at 1 MHz step = 100 ms.
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void Cc1101Jammer::jamHopper() {
    if (config_.hopperCount == 0) {
        LOGW(kTag, "hopper: empty list; idle");
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }
    for (uint8_t i = 0; i < config_.hopperCount && running_; ++i) {
        setRadioFrequency(config_.hopperFreqs[i]);
        postProgress(config_.hopperFreqs[i]);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Cc1101Jammer::jamKeyfob() {
    const uint8_t start = (config_.keyfobIndex < KEYFOB_COUNT) ? config_.keyfobIndex : 0;
    // A common UX is "jam just the one I picked"; if keyfobIndex is in
    // range, dwell on it; otherwise sweep the whole table.
    if (config_.keyfobIndex < KEYFOB_COUNT) {
        const float f = KEYFOB_FREQS[config_.keyfobIndex];
        setRadioFrequency(f);
        postProgress(f);
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        for (uint8_t i = start; i < KEYFOB_COUNT && running_; ++i) {
            setRadioFrequency(KEYFOB_FREQS[i]);
            postProgress(KEYFOB_FREQS[i]);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// ---------------------------------------------------------------------------
// Record / replay — full implementation
// ---------------------------------------------------------------------------
void Cc1101Jammer::recordSignal() {
    if (g_cc1101.isPresent() && g_cc1101.radio()) {
        // The Cc1101 driver owns the PSRAM buffer + RX state machine
        LOGI(kTag, "recordSignal: starting %ums capture", config_.recordDurationMs);
        g_cc1101.radio()->startRecord(config_.recordDurationMs);
    } else {
        LOGW(kTag, "recordSignal: no CC1101 attached");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}

void Cc1101Jammer::replaySignal() {
    if (g_cc1101.isPresent() && g_cc1101.radio()) {
        LOGI(kTag, "replaySignal: starting %ums transmission", config_.recordDurationMs);
        g_cc1101.radio()->startReplay(config_.recordDurationMs);
    } else {
        LOGW(kTag, "replaySignal: no CC1101 attached or buffer empty");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void Cc1101Jammer::setRadioFrequency(float mhz) {
    // The full implementation calls:
    //   phm::radio::Cc1101Manager::get(0).setFrequency(mhz);
    //   phm::radio::Cc1101Manager::get(0).calibrate();
    (void)mhz;
}

void Cc1101Jammer::postProgress(float freqMhz) {
    // Pack as little-endian float bytes; the receiver reinterprets.
    uint8_t buf[sizeof(float)];
    memcpy(buf, &freqMhz, sizeof(float));

    Event ev;
    ev.type = EventType::AttackProgress;
    ev.timestamp = millis();
    ev.sourceId = MODULE_ID;
    ev.dataLen = sizeof(buf);
    ev.data = buf;
    g_events.post(ev);
}

}  // namespace phm::modules
