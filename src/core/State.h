/**
 * @file State.h
 * @brief Global state machine and shared runtime variables
 *
 * The single `phm::GlobalState` struct is exposed to all modules so that
 * "what is the device doing right now" can be queried cheaply. A small
 * `phm::State` enum captures the high-level mode.
 *
 * The struct is defined as a single global instance (`phm::g_state`) so
 * that the C-style FreeRTOS code paths can read it without C++ ceremony.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

namespace phm {

/**
 * @brief High-level operating state of the device
 *
 * The state machine is intentionally tiny — most behaviour is decided by
 * which `IModule` is currently active, not by `g_state.state`.
 */
enum class State : uint8_t {
    Booting = 0,    ///< Firmware is initialising; modules not yet setup
    Idle = 1,       ///< All modules ready, no attack running
    Running = 2,    ///< At least one attack is active
    Stopping = 3,   ///< Attack is in the process of being torn down
    Error = 4,      ///< A fatal error was hit; emergency shutdown pending
    DeepSleep = 5,  ///< The device is in deep sleep; will wake on button
};

/**
 * @brief Shared runtime state
 *
 * Single-instance struct updated by various subsystems. The state is
 * read by the web UI, the OLED menu, and the serial CLI to report
 * "what is going on right now" without re-querying each module.
 */
struct GlobalState {
    State state = State::Booting;  ///< High-level mode
    uint32_t bootTime = 0;         ///< millis() at boot
    uint32_t lastErrorTime = 0;    ///< millis() of last error
    char lastError[64] = {0};      ///< Last error string
    uint8_t currentModuleId = 0;   ///< Active attack module
    float internalTempC = 0.0f;    ///< Last temp sensor reading
    uint16_t vbat_mV = 0;          ///< Battery voltage (mV)
    bool apActive = true;          ///< WiFi AP is broadcasting
    bool webConnected = false;     ///< At least one web client
    bool usbConnected = false;     ///< USB CDC host connected
};

/// Single shared instance, defined in PhantomRF.cpp
extern GlobalState g_state;

}  // namespace phm
