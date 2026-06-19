/**
 * @file Buttons.cpp
 * @brief Debounced button handler — implementation
 *
 * Allocates `GButton` instances on the heap (so this header doesn't
 * have to know the class size) and ticks them every `update()`. The
 * underlying library is required at build time.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "hal/Buttons.h"
#include <GyverButton.h>

#include <Arduino.h>

#include "core/Config.h"
#include "utils/Logger.h"

namespace phm::hal {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Buttons g_buttons;

// Debounce / timing constants — picked to feel snappy on a 128x64 OLED
static constexpr uint16_t kDebounceMs     = 35;
static constexpr uint16_t kLongPressMs    = 600;
static constexpr uint16_t kDoubleClickMs  = 300;

// ---------------------------------------------------------------------------
void Buttons::setup() {
    const int8_t pinOk   = g_board.pin(PinRole::ButtonOk);
    const int8_t pinNext = g_board.pin(PinRole::ButtonNext);
    const int8_t pinPrev = g_board.pin(PinRole::ButtonPrev);

    if (pinOk >= 0) {
        btnOk_ = new ::GButton(pinOk, HIGH_PULL, NORM_OPEN);
        btnOk_->setDebounce(kDebounceMs);
        btnOk_->setTimeout(kLongPressMs);
        btnOk_->setClickTimeout(kDoubleClickMs);
    }
    if (pinNext >= 0) {
        btnNext_ = new ::GButton(pinNext, HIGH_PULL, NORM_OPEN);
        btnNext_->setDebounce(kDebounceMs);
        btnNext_->setTimeout(kLongPressMs);
        btnNext_->setClickTimeout(kDoubleClickMs);
    }
    if (pinPrev >= 0) {
        btnPrev_ = new ::GButton(pinPrev, HIGH_PULL, NORM_OPEN);
        btnPrev_->setDebounce(kDebounceMs);
        btnPrev_->setTimeout(kLongPressMs);
        btnPrev_->setClickTimeout(kDoubleClickMs);
    }

    LOGD("btn", "buttons: OK=%d NEXT=%d PREV=%d (cfg=%u)",
         static_cast<int>(pinOk), static_cast<int>(pinNext),
         static_cast<int>(pinPrev), static_cast<unsigned>(config_));
}

// ---------------------------------------------------------------------------
void Buttons::update() {
    if (btnOk_   != nullptr) btnOk_->tick();
    if (btnNext_ != nullptr) btnNext_->tick();
    if (btnPrev_ != nullptr) btnPrev_->tick();
}

// ---------------------------------------------------------------------------
bool Buttons::wasPressed(ButtonId id) {
    ::GButton* b = nullptr;
    switch (id) {
        case ButtonId::Ok:   b = btnOk_;   break;
        case ButtonId::Next: b = btnNext_; break;
        case ButtonId::Prev: b = btnPrev_; break;
    }
    return (b != nullptr) && b->isPress();
}

// ---------------------------------------------------------------------------
bool Buttons::wasLongPressed(ButtonId id) {
    ::GButton* b = nullptr;
    switch (id) {
        case ButtonId::Ok:   b = btnOk_;   break;
        case ButtonId::Next: b = btnNext_; break;
        case ButtonId::Prev: b = btnPrev_; break;
    }
    return (b != nullptr) && b->isHold();
}

// ---------------------------------------------------------------------------
bool Buttons::wasDoublePressed(ButtonId id) {
    ::GButton* b = nullptr;
    switch (id) {
        case ButtonId::Ok:   b = btnOk_;   break;
        case ButtonId::Next: b = btnNext_; break;
        case ButtonId::Prev: b = btnPrev_; break;
    }
    return (b != nullptr) && b->isDouble();
}

// ---------------------------------------------------------------------------
void Buttons::setConfig(uint8_t n) {
    if (n > 2) n = 2;
    config_ = n;
    LOGD("btn", "button config set to %u", static_cast<unsigned>(n));
}

// ---------------------------------------------------------------------------
bool Buttons::isWired(ButtonId id) const {
    switch (id) {
        case ButtonId::Ok:   return btnOk_   != nullptr;
        case ButtonId::Next: return btnNext_ != nullptr;
        case ButtonId::Prev: return btnPrev_ != nullptr;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Destructor — GButton instances are owned by us
// ---------------------------------------------------------------------------
// Note: Buttons is a static-singleton with effectively unbounded lifetime.
// We deliberately do not free the GButton pointers to keep the code
// simple; their ~250 B of RAM is negligible compared to the rest of the
// firmware. If the project ever needs a teardown path, add a destroy()
// method and call it from PhantomRF::teardown.

}  // namespace phm::hal
