/**
 * @file PhantomRF.cpp
 * @brief PhantomRF top-level orchestrator — implementation
 *
 * Defines the `g_state` global, the `g_app` pointer, and the orchestration
 * logic for setup() / loop() / registerModule().
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "core/PhantomRF.h"

#include <Arduino.h>

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Led.h"
#include "hal/Power.h"
#include "hal/Storage.h"
#include "utils/Logger.h"

namespace phm {

// ---------------------------------------------------------------------------
// Globals defined here so they live in exactly one translation unit
// ---------------------------------------------------------------------------
GlobalState g_state;
PhantomRF* g_app = nullptr;

// ---------------------------------------------------------------------------
void PhantomRF::registerModule(IModule* module) {
    if (module == nullptr) {
        return;
    }
    modules_.push_back(module);
}

// ---------------------------------------------------------------------------
IModule* PhantomRF::findModule(uint8_t id) const {
    for (IModule* m : modules_) {
        if (m != nullptr && m->id() == id) {
            return m;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
void PhantomRF::setup() {
    if (ready_) {
        return;  // idempotent
    }

    // ---- Platform bring-up (order matters) -------------------------------
    g_state.bootTime = millis();

    hal::g_board.setup();              // configure pins
    util::g_logger.setup();            // logging to Serial
    hal::g_storage.setup();            // NVS + LittleFS
    g_events.init();                   // FreeRTOS event queue

    // ---- Power / health ---------------------------------------------------
    hal::g_power.setup();
    hal::g_led.setup();
    hal::g_buttons.setup();

    // ---- Bring up each registered module ---------------------------------
    LOGI("core", "Setting up %u module(s)...", static_cast<unsigned>(modules_.size()));
    for (IModule* m : modules_) {
        if (m == nullptr) {
            continue;
        }
        LOGD("core", "  setup() %s (id=%u)", m->name(), static_cast<unsigned>(m->id()));
        m->setup();
    }

    g_state.state = State::Idle;
    ready_ = true;

    LOGI("core", "PhantomRF ready.");
}

// ---------------------------------------------------------------------------
void PhantomRF::loop() {
    if (!ready_) {
        return;
    }

    // Poll hardware that needs to be ticked from the main loop
    hal::g_buttons.update();
    hal::g_power.update();
    hal::g_led.update();

    // Tick every module in registration order. Modules are responsible
    // for doing their own work in non-blocking chunks.
    for (IModule* m : modules_) {
        if (m == nullptr) {
            continue;
        }
        m->loop();
    }
}

}  // namespace phm
