/**
 * @file Led.cpp
 * @brief RGB LED driver — implementation
 *
 * Uses the ESP32 LEDC peripheral for hardware PWM. The three channels
 * are allocated to the R/G/B pins during `setup()`. If a pin is
 * missing on the current board (e.g. classic ESP32 with a single
 * indicator LED), the channel allocation is skipped gracefully and
 * the LED is treated as a single-channel "any colour" indicator.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "hal/Led.h"

#include "core/Config.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <math.h>

namespace phm::hal {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Led g_led;

// PWM frequency and resolution — conservative, works on every ESP32 variant
static constexpr uint32_t kPwmFreq = 5000;  // 5 kHz, above flicker threshold
static constexpr uint8_t kPwmResBits = 8;   // 8-bit duty (0..255)
static constexpr uint8_t kPwmMax = 255;

// ---------------------------------------------------------------------------
// Helper: turn a colour enum into a [R, G, B] tuple
// ---------------------------------------------------------------------------
static void colorToRgb(LedColor c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = g = b = 0;
    switch (c) {
    case LedColor::Off: break;
    case LedColor::Red: r = kPwmMax; break;
    case LedColor::Green: g = kPwmMax; break;
    case LedColor::Blue: b = kPwmMax; break;
    case LedColor::Yellow:
        r = kPwmMax;
        g = kPwmMax;
        break;
    case LedColor::Cyan:
        g = kPwmMax;
        b = kPwmMax;
        break;
    case LedColor::Magenta:
        r = kPwmMax;
        b = kPwmMax;
        break;
    case LedColor::White:
        r = kPwmMax;
        g = kPwmMax;
        b = kPwmMax;
        break;
    }
}

// ---------------------------------------------------------------------------
void Led::setup() {
    static constexpr PinRole kChannels[3] = {PinRole::LedR, PinRole::LedG, PinRole::LedB};
    static constexpr uint8_t kIdx[3] = {0, 1, 2};

    bool anyChannel = false;
    for (uint8_t i = 0; i < 3; ++i) {
        const int8_t gpio = g_board.pin(kChannels[i]);
        if (gpio < 0) {
            pwmChannels_[kIdx[i]] = -1;
            continue;
        }
        const int ch = static_cast<int>(i);  // 0, 1, 2 — guaranteed free at boot
        ledcAttachPin(gpio, ch);
        ledcSetup(ch, kPwmFreq, kPwmResBits);
        ledcWrite(ch, 0);
        pwmChannels_[kIdx[i]] = ch;
        pwmDuty_[kIdx[i]] = 0;
        anyChannel = true;
    }
    isPwm_ = anyChannel;
    if (isPwm_) {
        LOGD("led", "LEDC ready on %d channel(s)", anyChannel ? 3 : 0);
    } else {
        LOGW("led", "No LED pins configured for this board");
    }
    current_ = LedColor::Off;
    lastUpdate_ = millis();
}

// ---------------------------------------------------------------------------
void Led::applyColor(LedColor c) {
    uint8_t r, g, b;
    colorToRgb(c, r, g, b);
    const uint8_t duty[3] = {r, g, b};
    for (uint8_t i = 0; i < 3; ++i) {
        if (pwmChannels_[i] >= 0) {
            ledcWrite(pwmChannels_[i], duty[i]);
        }
        pwmDuty_[i] = duty[i];
    }
    current_ = c;
}

// ---------------------------------------------------------------------------
void Led::setColor(LedColor c) {
    anim_ = Anim::None;
    applyColor(c);
}

// ---------------------------------------------------------------------------
void Led::startBlink(LedColor c, uint16_t period_ms, uint8_t count) {
    if (count == 0)
        count = 1;
    anim_ = Anim::Blink;
    animColor_ = c;
    animPeriodMs_ = (period_ms < 20) ? 20 : period_ms;
    animCount_ = count;
    animTotal_ = count;
    animPhaseOn_ = true;
    animStarted_ = millis();
    applyColor(c);
}

// ---------------------------------------------------------------------------
void Led::startBreathe(LedColor c) {
    anim_ = Anim::Breathe;
    animColor_ = c;
    animStarted_ = millis();
    applyColor(c);
}

// ---------------------------------------------------------------------------
void Led::blink(LedColor c, uint16_t period_ms, uint8_t count) {
    startBlink(c, period_ms, count);
}

// ---------------------------------------------------------------------------
void Led::breathe(LedColor c) {
    startBreathe(c);
}

// ---------------------------------------------------------------------------
void Led::update() {
    if (anim_ == Anim::None) {
        return;
    }
    const uint32_t now = millis();
    const uint32_t dt = now - animStarted_;

    if (anim_ == Anim::Blink) {
        const uint32_t halfPeriod = animPeriodMs_ / 2;
        if (dt >= halfPeriod) {
            animPhaseOn_ = !animPhaseOn_;
            animStarted_ = now;
            if (animPhaseOn_) {
                applyColor(animColor_);
            } else {
                applyColor(LedColor::Off);
                if (animCount_ > 0) {
                    --animCount_;
                }
            }
            if (animCount_ == 0) {
                anim_ = Anim::None;
                applyColor(LedColor::Off);
            }
        }
    } else if (anim_ == Anim::Breathe) {
        // 3-second sinusoidal cycle, 0..max
        const float t = static_cast<float>(dt % 3000U) / 3000.0f;
        const float wave = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * t));
        uint8_t r, g, b;
        colorToRgb(animColor_, r, g, b);
        const uint8_t scale = static_cast<uint8_t>(wave * 255.0f);
        r = static_cast<uint8_t>((static_cast<uint16_t>(r) * scale) / 255u);
        g = static_cast<uint8_t>((static_cast<uint16_t>(g) * scale) / 255u);
        b = static_cast<uint8_t>((static_cast<uint16_t>(b) * scale) / 255u);
        const uint8_t duty[3] = {r, g, b};
        for (uint8_t i = 0; i < 3; ++i) {
            if (pwmChannels_[i] >= 0) {
                ledcWrite(pwmChannels_[i], duty[i]);
            }
            pwmDuty_[i] = duty[i];
        }
    }
}

// ---------------------------------------------------------------------------
void Led::indicateAttack(uint8_t attackType) {
    // Map attack type → colour. Numeric values are the IModule::id()s
    // of the canonical attack modules; collisions are fine (we just
    // pick a colour and go).
    switch (attackType) {
    case 1: setColor(LedColor::Red); break;      // nRF24 jam
    case 2: setColor(LedColor::Blue); break;     // CC1101 jam
    case 3: setColor(LedColor::Magenta); break;  // WiFi deauth
    case 4: setColor(LedColor::Cyan); break;     // BLE spam
    case 5: setColor(LedColor::Yellow); break;   // Spectrum
    default: blink(LedColor::White, 400, 1); break;
    }
}

// ---------------------------------------------------------------------------
void Led::indicateError() {
    blink(LedColor::Red, 250, 3);
}

// ---------------------------------------------------------------------------
void Led::indicateIdle() {
    setColor(LedColor::Green);
}

}  // namespace phm::hal
