/**
 * @file EventBus.cpp
 * @brief FreeRTOS-backed inter-task event bus — implementation
 *
 * Wraps a FreeRTOS queue of `Event*` pointers. The bus allocates a new
 * `Event` plus payload buffer on `post()` and pushes the pointer; the
 * receiver takes ownership and `delete`s it.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "core/EventBus.h"

#include <new>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace phm {

// Queue size: 32 events deep. Generous enough for fast producers; small
// enough to bound RAM (32 × ~64 B = ~2 KB worst case).
static constexpr UBaseType_t kQueueDepth = 32;

// ---------------------------------------------------------------------------
// Singleton instance
// ---------------------------------------------------------------------------
EventBus g_events;

// ---------------------------------------------------------------------------
void EventBus::init() {
    if (queue_ != nullptr) {
        return;  // already initialised
    }
    queue_ = xQueueCreate(kQueueDepth, sizeof(Event*));
}

// ---------------------------------------------------------------------------
bool EventBus::post(const Event& event, TickType_t wait) {
    if (queue_ == nullptr) {
        return false;
    }

    // Allocate the receiver-owned Event (shallow copy of metadata).
    auto* copy = new (std::nothrow) Event();
    if (copy == nullptr) {
        return false;
    }
    *copy = event;  // copy type/timestamp/sourceId/dataLen and the data pointer

    // Deep-copy the payload so the sender can free its buffer immediately.
    if (event.data != nullptr && event.dataLen > 0) {
        copy->data = new (std::nothrow) uint8_t[event.dataLen];
        if (copy->data == nullptr) {
            delete copy;
            return false;
        }
        memcpy(copy->data, event.data, event.dataLen);
    } else {
        copy->data = nullptr;
        copy->dataLen = 0;
    }

    if (xQueueSend(queue_, &copy, wait) != pdTRUE) {
        // Queue was full: clean up the copy and report failure.
        delete copy;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
Event* EventBus::receive(TickType_t wait) {
    if (queue_ == nullptr) {
        return nullptr;
    }
    Event* evt = nullptr;
    if (xQueueReceive(queue_, &evt, wait) != pdTRUE) {
        return nullptr;
    }
    return evt;  // caller owns the pointer
}

// ---------------------------------------------------------------------------
UBaseType_t EventBus::pending() const {
    if (queue_ == nullptr) {
        return 0;
    }
    return uxQueueMessagesWaiting(queue_);
}

}  // namespace phm
