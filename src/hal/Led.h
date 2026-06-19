/**
 * @file Led.h
 * @brief RGB status LED with simple effects
 *
 * A single common-anode or common-cathode RGB LED is driven by three
 * PWM-capable GPIOs (R, G, B). The `Led` class supports:
 *   - Solid colour
 *   - Timed blink (N pulses, then return to off)
 *   - Slow breathing fade
 *
 * `update()` must be called from the main loop (PhantomRF does this) to
 * advance blink and breathing animations.
 *
 * Attack types are mapped to a colour via `indicateAttack()`, so the
 * web UI can show the user "we are jamming WiFi right now" without
 * having to remember the colour table.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

#include "hal/Board.h"

namespace phm::hal {

/**
 * @brief Predefined LED colours
 *
 * The `Off` value is special: it forces all channels to 0% duty.
 */
enum class LedColor : uint8_t {
    Off     = 0,
    Red     = 1,
    Green   = 2,
    Blue    = 3,
    Yellow  = 4,  ///< R + G
    Cyan    = 5,  ///< G + B
    Magenta = 6,  ///< R + B
    White   = 7,  ///< R + G + B
};

/**
 * @brief Singleton LED controller
 *
 * Use the global `phm::hal::g_led` instance.
 */
class Led {
public:
    /// Attach LEDC channels to the configured GPIOs
    void setup();

    /// Set a solid colour (cancels any running animation)
    void setColor(LedColor c);

    /// Blink `count` times at `period_ms` total period (50% duty)
    void blink(LedColor c, uint16_t period_ms, uint8_t count = 1);

    /// Slow sinusoidal breathing on the given colour
    void breathe(LedColor c);

    /// Must be called from the main loop to advance animations
    void update();

    // ---- Convenience indicators -----------------------------------------
    /// Map a known attack type to a colour and set it solid
    void indicateAttack(uint8_t attackType);

    /// Flashing red — something is wrong
    void indicateError();

    /// Soft green — device is idle
    void indicateIdle();

    /// Currently-displayed colour (does not account for blink/breathe phase)
    LedColor currentColor() const { return current_; }

private:
    void applyColor(LedColor c);
    void startBlink(LedColor c, uint16_t period_ms, uint8_t count);
    void startBreathe(LedColor c);

    LedColor  current_    = LedColor::Off;
    uint32_t  lastUpdate_  = 0;
    bool      isPwm_       = false;

    // LEDC channels and current duty values for [R, G, B]
    int       pwmChannels_[3] = { -1, -1, -1 };
    uint8_t   pwmDuty_[3]     = {   0,   0,   0 };

    // Animation state
    enum class Anim : uint8_t { None, Blink, Breathe };
    Anim      anim_           = Anim::None;
    LedColor  animColor_      = LedColor::Off;
    uint16_t  animPeriodMs_   = 0;
    uint8_t   animCount_      = 0;     ///< remaining pulses (Blink only)
    uint8_t   animTotal_      = 0;     ///< total pulses for bookkeeping
    uint32_t  animStarted_    = 0;     ///< millis() when last phase started
    bool      animPhaseOn_    = false; ///< Blink: true while LED is on
};

/// Singleton instance, defined in Led.cpp
extern Led g_led;

}  // namespace phm::hal
