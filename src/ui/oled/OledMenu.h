/**
 * @file OledMenu.h
 * @brief SSD1306 OLED menu for 1/2/3 button configurations
 *
 * The OLED is the headless control surface. The menu is a small state
 * machine driven by `phm::hal::g_buttons`:
 *
 *   1-button (constrained):  short = next, long = select
 *   2-button (medium):       Next = next, OK = select
 *   3-button (full):         Prev / Next / OK
 *
 * Screens (DESIGN §12.2):
 *   - Splash → MainMenu → per-feature submenu → confirm → running screen
 *
 * All drawing goes through `Adafruit_GFX` primitives. We use a tiny set of
 * 8x8 menu glyphs hand-baked into a font table when the platform is in a
 * memory-constrained build, otherwise `Adafruit_SSD1306::setFont()` is
 * used directly with the bundled 6x8 font.
 *
 * The display is **optional**: if `oled.enabled` is false in NVS (or the
 * I2C device is not detected at boot), `setup()` returns without
 * initialising the display and all drawing methods become no-ops.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Module.h"
#include "hal/Buttons.h"

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace phm::ui {

// Forward decl removed — Adafruit_SSD1306.h is now included above

/**
 * @brief Headless 128x64 OLED menu
 *
 * Singleton registered with the `PhantomRF` core. Drawing is done from
 * `loop()` at a configurable rate (default 10 Hz for static screens,
 * 30 Hz when displaying live spectrum / attack state).
 */
class OledMenu : public ::phm::IModule {
public:
    /// Stable module id
    static constexpr uint8_t MODULE_ID = 0xE1;

    /// OLED geometry (128x64 is the M0 default; 128x32 supported in M1)
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;

    /// Refresh rates
    static constexpr uint32_t kIdleRefreshMs = 100;   ///< 10 Hz when idle
    static constexpr uint32_t kActiveRefreshMs = 33;  ///< ~30 Hz in live mode

    /// Splash screen dwell time
    static constexpr uint32_t kSplashMs = 2000;

    /// Top-level screens. The actual per-attack screens live under
    /// `Screen::Attack` and are selected by a follow-up state variable.
    enum class Screen : uint8_t {
        Splash = 0,
        MainMenu = 1,
        BluetoothJam = 2,
        WifiAttack = 3,
        Spectrum2_4GHz = 4,
        SpectrumSubGHz = 5,
        SubGhzJam = 6,
        BleAttack = 7,
        Settings = 8,
        Files = 9,
        Info = 10,
        Attack = 11,  ///< Generic "running" screen
        Error = 12,   ///< Show last error for a few seconds
    };

    // ---- IModule ---------------------------------------------------------
    const char* name() const override { return "OledMenu"; }
    const char* description() const override { return "SSD1306 128x64 menu (1/2/3 buttons)"; }
    uint8_t id() const override { return MODULE_ID; }
    void setup() override;
    void loop() override;

    // ---- Public API -----------------------------------------------------

    /// Switch to a top-level screen. The renderer will redraw on the next loop tick.
    void setCurrentScreen(Screen s);

    /// Force a redraw on the next loop tick (e.g. after settings change).
    void redraw() { dirty_ = true; }

    /// Print a single line of text at row `y` (0..7 on a 64 px display).
    /// Useful for debug overlays and splash variants.
    void println(const char* text, uint8_t y = 0);

    /// Show a transient error screen (overrides the current screen for ~3 s).
    void showError(const char* msg);

    /// Show a transient info screen (overrides the current screen for ~3 s).
    void showInfo(const char* msg);

    /// Show a 0..100 % progress bar at the bottom of the display.
    void showProgress(uint8_t pct);

    /// True when the OLED is actually wired and initialised
    bool isActive() const { return display_ != nullptr; }

    /// Currently displayed top-level screen
    Screen currentScreen() const { return current_; }

private:
    // ---- Setup helpers ---------------------------------------------------
    bool initDisplay();
    void drawSplash();
    void drawMainMenu();
    void drawBluetoothJam();
    void drawWifiAttack();
    void drawBleAttack();
    void drawSpectrum2_4();
    void drawSpectrumSub();
    void drawSubGhzJam();
    void drawSettings();
    void drawFiles();
    void drawInfo();
    void drawAttack();
    void drawStatusBar();
    void drawMenuItem(uint8_t index, const char* text, bool selected);

    // ---- Input handling --------------------------------------------------
    void handleButton(phm::hal::ButtonId id, bool pressed, bool longPress, bool doublePress);
    void onSelect();
    void onBack();
    void onNext();
    void onPrev();

    // ---- Helpers ---------------------------------------------------------
    void centerText(const char* text, int y);
    void formatUptime(char* out, size_t len);
    int rssiBars(int8_t rssi);

    // ---- State -----------------------------------------------------------
    Adafruit_SSD1306* display_ = nullptr;  ///< Owned by the library; we just keep a pointer
    Screen current_ = Screen::Splash;
    bool dirty_ = true;
    uint32_t lastDraw_ = 0;
    uint32_t splashEnd_ = 0;  ///< millis() at which splash should clear

    /// Index of the highlighted item in the current menu (0..6)
    uint8_t menuIndex_ = 0;

    /// For "running" screens we cache the attack name so the renderer
    /// can show "WiFi Deauth (ch 6)" while the radio is busy.
    char attackName_[32] = {0};
    uint8_t attackChannel_ = 0;
    int8_t attackRssi_ = -100;

    /// Transient error / info overlay
    char overlayMsg_[32] = {0};
    uint32_t overlayEnd_ = 0;
    Screen savedScreen_ = Screen::MainMenu;

    /// Button config cache (so we don't read NVS every loop tick)
    uint8_t buttonConfig_ = 2;
};

/// Singleton instance, defined in OledMenu.cpp
extern OledMenu g_oledMenu;

}  // namespace phm::ui
