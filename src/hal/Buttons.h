/**
 * @file Buttons.h
 * @brief Debounced button input (1/2/3-button configurations)
 *
 * Wraps the third-party `GyverButton` library for per-pin debounce,
 * click, double-click and long-press detection. The pins come from
 * `g_board.pin(PinRole::ButtonOk/Next/Prev)`.
 *
 * The OLED menu can run in three modes (DESIGN §12.2):
 *   - 0 = 1-button:  only OK present; short = next, long = select
 *   - 1 = 2-button:  OK + Next
 *   - 2 = 3-button:  OK + Next + Prev (default)
 *
 * `update()` must be called from the main loop so `GyverButton` can
 * sample its pins.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "hal/Board.h"

#include <stdint.h>

// `GButton` (from the `GyverButton` library) is a third-party class
// that lives in the global namespace. Forward-declare it at file scope
// so member pointers can use the global name without dragging in
// <GyverButton.h>.
class GButton;

namespace phm::hal {

/**
 * @brief Logical button identifiers
 *
 * `Ok` is always the centre/select button; `Next` and `Prev` navigate
 * the menu. In 1-button mode, `Ok` is overloaded (short = next, long = select).
 */
enum class ButtonId : uint8_t {
    Ok = 0,
    Next = 1,
    Prev = 2,
};

/**
 * @brief Singleton button handler
 *
 * Use the global `phm::hal::g_buttons` instance.
 */
class Buttons {
public:
    /// Configure pins and underlying debouncers
    void setup();

    /// Sample every pin; call from the main loop
    void update();

    /// True once per short press of the given button
    bool wasPressed(ButtonId id);

    /// True once per long press of the given button
    bool wasLongPressed(ButtonId id);

    /// True once per double press of the given button
    bool wasDoublePressed(ButtonId id);

    /// Set the active button configuration (0/1/2)
    void setConfig(uint8_t n);

    /// Current button configuration (0/1/2)
    uint8_t config() const { return config_; }

    /// True if the given button is mapped to a real GPIO on this board
    bool isWired(ButtonId id) const;

private:
    // The pointers are typed with the global `GButton` (from the
    // AlexGyver GyverButton debounce library).
    GButton* btnOk_ = nullptr;  ///< Owned by this object
    GButton* btnNext_ = nullptr;
    GButton* btnPrev_ = nullptr;

    uint8_t config_ = 2;  ///< 0=1-button, 1=2-button, 2=3-button
};

/// Singleton instance, defined in Buttons.cpp
extern Buttons g_buttons;

}  // namespace phm::hal
