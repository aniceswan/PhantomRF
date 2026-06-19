/**
 * @file EventBus.h
 * @brief FreeRTOS-backed inter-task event bus
 *
 * Modules running on different cores (or in different FreeRTOS tasks)
 * communicate by posting `Event` objects to a single queue. The bus
 * copies the payload so the sender can free its buffer immediately
 * after `post()` returns.
 *
 * The receiver owns the popped `Event*` and must `delete` it (which
 * also frees the payload). Use `bus.receive()` from a dedicated
 * consumer task or from the main loop.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Config.h"

#include <stdint.h>

#ifdef ESP32
// Pull in the real FreeRTOS types when compiling for ESP32.
// The native-test build (PHM_NATIVE_TEST) does not have FreeRTOS,
// so the .cpp file provides its own minimal shim.
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace phm {

/**
 * @brief All event types that can travel across the bus
 *
 * Add new event types here as modules grow. The numeric values are
 * stable once assigned — never reorder or remove entries, only append.
 */
enum class EventType : uint8_t {
    AttackStarted = 0,    ///< A module began an attack
    AttackStopped = 1,    ///< A module finished an attack cleanly
    AttackFailed = 2,     ///< A module could not start the attack
    AttackProgress = 3,   ///< Periodic attack progress update
    SpectrumData = 4,     ///< RSSI sample for one or more channels
    LogMessage = 5,       ///< A log entry to be forwarded to web/UI
    SystemError = 6,      ///< A non-fatal error
    ButtonPress = 7,      ///< A button was pressed (data = ButtonId)
    WebCommand = 8,       ///< A command arrived via the web UI
    BleCommand = 9,       ///< A command arrived via BLE Serial
    UsbCommand = 10,      ///< A command arrived via USB CDC
    SettingChanged = 11,  ///< A persistent setting was modified
};

/**
 * @brief One queued event
 *
 * `data` is heap-allocated and owned by the Event; the bus copies the
 * sender's payload into a new buffer, and the receiver `delete`s the
 * Event (which frees the data buffer).
 */
struct Event {
    EventType type;      ///< What happened
    uint32_t timestamp;  ///< millis() at post() time
    uint8_t sourceId;    ///< IModule::id() of the sender
    uint16_t dataLen;    ///< Number of bytes in `data` (may be 0)
    uint8_t* data;       ///< Owned by the Event; deleted by `delete event`

    /// Default ctor leaves fields zero-initialised
    Event() : type(EventType::SystemError), timestamp(0), sourceId(0), dataLen(0), data(nullptr) {}

    /// Frees the payload buffer (the Event itself is owned by the caller)
    ~Event() {
        delete[] data;
        data = nullptr;
    }
};

/**
 * @brief Singleton event bus
 *
 * Use `phm::g_events` from anywhere. Call `init()` once during `setup()`.
 */
class EventBus {
public:
    /// Create the underlying FreeRTOS queue; safe to call once
    void init();

    /// Push a copy of `event` onto the queue. Returns false if the queue is full.
    /// The sender's `event.data` is copied; the caller may free it immediately.
    bool post(const Event& event, TickType_t wait = 0);

    /// Pop an event. Returns nullptr on timeout. The caller owns the
    /// returned pointer and must `delete` it.
    Event* receive(TickType_t wait = portMAX_DELAY);

    /// Number of events currently waiting in the queue
    UBaseType_t pending() const;

    /// True if the bus is initialised and ready
    bool ready() const { return queue_ != nullptr; }

private:
    QueueHandle_t queue_ = nullptr;  ///< Owned by this object
};

/// Single shared instance, defined in EventBus.cpp
extern EventBus g_events;

}  // namespace phm
