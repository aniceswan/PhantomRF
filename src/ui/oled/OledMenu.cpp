/**
 * @file OledMenu.cpp
 * @brief SSD1306 OLED menu — implementation
 *
 * Drawing strategy: one full frame per redraw. The SSD1306 has a 1 KB
 * framebuffer on-chip, so partial updates are not faster than a full
 * redraw and the code stays simple. We do rate-limit redraws in `loop()`
 * via `kIdleRefreshMs` / `kActiveRefreshMs`.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "ui/oled/OledMenu.h"

#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Led.h"
#include "hal/Power.h"
#include "hal/Storage.h"
#include "modules/BleAttack/BleAttack.h"
#include "modules/Cc1101Jammer/Cc1101Jammer.h"
#include "modules/Nrf24Jammer/Nrf24Jammer.h"
#include "modules/Spectrum/Spectrum.h"
#include "modules/WifiAttack/WifiAttack.h"
#include "utils/Logger.h"

#include <Adafruit_SSD1306.h>

using phm::hal::g_board;
using phm::hal::g_buttons;
using phm::hal::g_led;
using phm::hal::g_power;
using phm::hal::g_storage;

#include "core/Config.h"
#include "core/EventBus.h"
#include "core/PhantomRF.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Storage.h"
#include "ui/cli/Console.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

namespace phm::ui {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
OledMenu g_oledMenu;

static constexpr const char* kTag = "oled";

// ---- Main menu definition -----------------------------------------------
struct MenuItem {
    const char* label;
    OledMenu::Screen screen;
};
static const MenuItem kMainMenu[] = {
    {"BT Jam", OledMenu::Screen::BluetoothJam},   {"WiFi Attack", OledMenu::Screen::WifiAttack},
    {"BLE Attack", OledMenu::Screen::BleAttack},  {"2.4 GHz Jam", OledMenu::Screen::Spectrum2_4GHz},
    {"Sub-GHz Jam", OledMenu::Screen::SubGhzJam}, {"Spectrum", OledMenu::Screen::SpectrumSubGHz},
    {"Settings", OledMenu::Screen::Settings},
};
static constexpr uint8_t kMainMenuCount = sizeof(kMainMenu) / sizeof(kMainMenu[0]);

// ===========================================================================
// Lifecycle
// ===========================================================================
void OledMenu::setup() {
    // Honour the "oled.enabled" NVS key — let the user disable the display
    // without reflashing.
    bool enabled = true;
    g_storage.getBool("oled.enabled", enabled);
    if (!enabled) {
        LOGI(kTag, "OLED disabled in NVS; skipping init");
        return;
    }

    // Cache the button config; the user can change it from the web UI and
    // we re-read on demand from the Settings screen.
    int32_t cfg = 2;
    g_storage.getInt("buttons.config", cfg);
    if (cfg < 0 || cfg > 2)
        cfg = 2;
    buttonConfig_ = static_cast<uint8_t>(cfg);
    g_buttons.setConfig(buttonConfig_);

    if (!initDisplay()) {
        LOGW(kTag, "OLED init failed; running headless");
        return;
    }

    // Splash screen
    current_ = Screen::Splash;
    splashEnd_ = millis() + kSplashMs;
    dirty_ = true;
    LOGI(kTag, "OLED ready (%d-button config)", buttonConfig_);
}

void OledMenu::loop() {
    if (!isActive()) {
        return;
    }

    // ---- 1. Drain button events ---------------------------------------
    // Each call to was* returns true exactly once per detected event, so
    // a single check per loop tick is enough.
    if (g_buttons.wasPressed(phm::hal::ButtonId::Ok)) {
        handleButton(phm::hal::ButtonId::Ok, true, false, false);
    } else if (g_buttons.wasLongPressed(phm::hal::ButtonId::Ok)) {
        handleButton(phm::hal::ButtonId::Ok, true, true, false);
    } else if (g_buttons.wasDoublePressed(phm::hal::ButtonId::Ok)) {
        handleButton(phm::hal::ButtonId::Ok, true, false, true);
    }
    if (g_buttons.wasPressed(phm::hal::ButtonId::Next)) {
        handleButton(phm::hal::ButtonId::Next, true, false, false);
    } else if (g_buttons.wasLongPressed(phm::hal::ButtonId::Next)) {
        handleButton(phm::hal::ButtonId::Next, true, true, false);
    }
    if (g_buttons.wasPressed(phm::hal::ButtonId::Prev)) {
        handleButton(phm::hal::ButtonId::Prev, true, false, false);
    } else if (g_buttons.wasLongPressed(phm::hal::ButtonId::Prev)) {
        handleButton(phm::hal::ButtonId::Prev, true, true, false);
    }

    // ---- 2. Auto-progress the splash screen ---------------------------
    if (current_ == Screen::Splash && static_cast<int32_t>(millis() - splashEnd_) >= 0) {
        current_ = Screen::MainMenu;
        dirty_ = true;
    }

    // ---- 3. Clear the transient overlay -------------------------------
    if (overlayEnd_ != 0 && static_cast<int32_t>(millis() - overlayEnd_) >= 0) {
        overlayEnd_ = 0;
        current_ = savedScreen_;
        dirty_ = true;
    }

    // ---- 4. Rate-limited redraw ---------------------------------------
    const uint32_t interval = (current_ == Screen::Attack) ? kActiveRefreshMs : kIdleRefreshMs;

    if (dirty_ || (millis() - lastDraw_) >= interval) {
        lastDraw_ = millis();
        dirty_ = false;

        display_->clearDisplay();

        // Transient overlay trumps the normal renderer
        if (overlayEnd_ != 0) {
            display_->setTextSize(1);
            display_->setTextColor(SSD1306_WHITE);
            centerText(overlayMsg_, 28);
        } else {
            switch (current_) {
            case Screen::Splash: drawSplash(); break;
            case Screen::MainMenu: drawMainMenu(); break;
            case Screen::BluetoothJam: drawBluetoothJam(); break;
            case Screen::WifiAttack: drawWifiAttack(); break;
            case Screen::BleAttack: drawBleAttack(); break;
            case Screen::Spectrum2_4GHz: drawSpectrum2_4(); break;
            case Screen::SpectrumSubGHz: drawSpectrumSub(); break;
            case Screen::SubGhzJam: drawSubGhzJam(); break;
            case Screen::Settings: drawSettings(); break;
            case Screen::Files: drawFiles(); break;
            case Screen::Info: drawInfo(); break;
            case Screen::Attack: drawAttack(); break;
            case Screen::Error: break;  // unused
            }
            drawStatusBar();
        }

        display_->display();
    }
}

// ===========================================================================
// Public mutators
// ===========================================================================
void OledMenu::setCurrentScreen(Screen s) {
    if (current_ == s)
        return;
    current_ = s;
    menuIndex_ = 0;
    dirty_ = true;
}

void OledMenu::println(const char* text, uint8_t y) {
    if (!isActive() || text == nullptr)
        return;
    display_->setTextSize(1);
    display_->setTextColor(SSD1306_WHITE);
    display_->setCursor(0, y * 8);
    display_->println(text);
    display_->display();
}

void OledMenu::showError(const char* msg) {
    if (!isActive() || msg == nullptr)
        return;
    savedScreen_ = current_;
    std::strncpy(overlayMsg_, msg, sizeof(overlayMsg_) - 1);
    overlayMsg_[sizeof(overlayMsg_) - 1] = '\0';
    overlayEnd_ = millis() + 3000;
    dirty_ = true;
    LOGW(kTag, "ERR: %s", msg);
}

void OledMenu::showInfo(const char* msg) {
    if (!isActive() || msg == nullptr)
        return;
    savedScreen_ = current_;
    std::strncpy(overlayMsg_, msg, sizeof(overlayMsg_) - 1);
    overlayMsg_[sizeof(overlayMsg_) - 1] = '\0';
    overlayEnd_ = millis() + 2000;
    dirty_ = true;
    LOGI(kTag, "INFO: %s", msg);
}

void OledMenu::showProgress(uint8_t pct) {
    if (!isActive())
        return;
    if (pct > 100)
        pct = 100;
    const int x = 0;
    const int y = kHeight - 8;
    const int w = kWidth;
    const int h = 8;
    display_->drawRect(x, y, w, h, SSD1306_WHITE);
    display_->fillRect(x + 1, y + 1, ((w - 2) * pct) / 100, h - 2, SSD1306_WHITE);
    display_->display();
}

// ===========================================================================
// Setup helpers
// ===========================================================================
bool OledMenu::initDisplay() {
    const int8_t sda = g_board.pin(phm::hal::PinRole::OledSda);
    const int8_t scl = g_board.pin(phm::hal::PinRole::OledScl);
    if (sda < 0 || scl < 0) {
        LOGW(kTag, "OLED SDA/SCL not mapped on this board");
        return false;
    }

    // The SSD1306 constructor only stores the pins; the real I2C begin()
    // happens in the library's begin() call.
    display_ = new Adafruit_SSD1306(kWidth, kHeight, &Wire, -1);
    if (display_ == nullptr) {
        return false;
    }
    Wire.begin(sda, scl);
    if (!display_->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        LOGW(kTag, "SSD1306 not found at 0x3C");
        delete display_;
        display_ = nullptr;
        return false;
    }
    display_->clearDisplay();
    display_->display();
    return true;
}

// ===========================================================================
// Drawing
// ===========================================================================
void OledMenu::drawSplash() {
    display_->setTextSize(2);
    display_->setTextColor(SSD1306_WHITE);
    centerText("PhantomRF", 4);

    display_->setTextSize(1);
    char line[24];
    std::snprintf(line, sizeof(line), "v%s", PHM_VERSION_STRING);
    centerText(line, 28);
    centerText(phm::hal::g_board.boardName(), 40);
}

void OledMenu::drawMainMenu() {
    display_->setTextSize(1);
    display_->setTextColor(SSD1306_WHITE);
    display_->setCursor(0, 0);
    display_->println("Main menu");

    // 6 items per page; we scroll if there are more
    constexpr uint8_t kPage = 6;
    const uint8_t pageStart = (menuIndex_ / kPage) * kPage;
    const uint8_t pageEnd = std::min<uint8_t>(pageStart + kPage, kMainMenuCount);

    int y = 10;
    for (uint8_t i = pageStart; i < pageEnd; ++i) {
        drawMenuItem(i - pageStart, kMainMenu[i].label, i == menuIndex_);
        y += 9;
    }
    // Scroll indicators
    if (pageStart > 0) {
        display_->setCursor(kWidth - 6, 10);
        display_->print('^');
    }
    if (pageEnd < kMainMenuCount) {
        display_->setCursor(kWidth - 6, 10 + (kPage - 1) * 9);
        display_->print('v');
    }
}

void OledMenu::drawBluetoothJam() {
    display_->setTextSize(2);
    display_->setTextColor(SSD1306_WHITE);
    centerText("BT Jam", 0);
    display_->setTextSize(1);
    display_->setCursor(0, 22);
    display_->println("Disrupts BT classic on");
    display_->println("all 79 channels.");
    display_->println();
    display_->println("OK = start");
    display_->println("Long OK = back");
}

void OledMenu::drawWifiAttack() {
    display_->setTextSize(2);
    centerText("WiFi", 0);
    display_->setTextSize(1);
    display_->setCursor(0, 22);
    display_->println("Deauth / beacon flood");
    display_->println("on selected channel.");
    display_->println();
    display_->println("OK = start");
    display_->println("Long OK = back");
}

void OledMenu::drawBleAttack() {
    display_->setTextSize(2);
    centerText("BLE", 0);
    display_->setTextSize(1);
    display_->setCursor(0, 22);
    display_->println("Apple / Samsung popup");
    display_->println("spam via advertisements.");
    display_->println();
    display_->println("OK = start");
    display_->println("Long OK = back");
}

void OledMenu::drawSpectrum2_4() {
    display_->setTextSize(1);
    display_->setCursor(0, 0);
    display_->println("2.4 GHz Sweep");
    // Simple bar graph placeholder
    const int baseY = kHeight - 10;
    for (int x = 0; x < kWidth; x += 4) {
        const int h = (millis() / 50 + x) % 20;
        display_->drawLine(x, baseY, x, baseY - h, SSD1306_WHITE);
    }
    char buf[16];
    const uint8_t ch = g_state.currentModuleId;
    if (ch == 0) {
        std::snprintf(buf, sizeof(buf), "idle");
    } else {
        std::snprintf(buf, sizeof(buf), "ch %u", static_cast<unsigned>(ch));
    }
    display_->setCursor(0, kHeight - 9);
    display_->print(buf);
}

void OledMenu::drawSpectrumSub() {
    display_->setTextSize(1);
    display_->setCursor(0, 0);
    display_->println("Sub-GHz Spectrum");
    const int baseY = kHeight - 10;
    for (int x = 0; x < kWidth; x += 4) {
        const int h = (millis() / 70 + x * 2) % 18;
        display_->drawLine(x, baseY, x, baseY - h, SSD1306_WHITE);
    }
    display_->setCursor(0, kHeight - 9);
    display_->print("idle");
}

void OledMenu::drawSubGhzJam() {
    display_->setTextSize(2);
    centerText("SubGHz", 0);
    display_->setTextSize(1);
    display_->setCursor(0, 22);
    display_->println("Spot / range / hopper");
    display_->println("433/868/915 MHz");
    display_->println();
    display_->println("OK = start");
    display_->println("Long OK = back");
}

void OledMenu::drawSettings() {
    display_->setTextSize(1);
    display_->setCursor(0, 0);
    display_->println("Settings");

    // Show a few NVS values. Use small line height (8 px) so we fit 6 rows.
    char buf[24];
    int32_t v = 0;

    display_->setCursor(0, 12);
    display_->print("AP: ");
    String s;
    if (g_storage.getString("ap.ssid", s))
        display_->print(s);
    else
        display_->print("(default)");

    display_->setCursor(0, 20);
    if (g_storage.getInt("buttons.config", v)) {
        std::snprintf(buf, sizeof(buf), "Buttons: %u-btn", static_cast<unsigned>(v));
    } else {
        std::snprintf(buf, sizeof(buf), "Buttons: 3-btn");
    }
    display_->print(buf);

    display_->setCursor(0, 28);
    if (g_storage.getInt("nrf24.pa", v)) {
        std::snprintf(buf, sizeof(buf), "nRF24 PA: %u", static_cast<unsigned>(v));
    } else {
        std::snprintf(buf, sizeof(buf), "nRF24 PA: max");
    }
    display_->print(buf);

    display_->setCursor(0, 36);
    if (g_storage.getInt("cc1101.sampling_us", v)) {
        std::snprintf(buf, sizeof(buf), "CC samp: %u us", static_cast<unsigned>(v));
    } else {
        std::snprintf(buf, sizeof(buf), "CC samp: 150 us");
    }
    display_->print(buf);

    display_->setCursor(0, 52);
    display_->print("Hold OK to reset");
}

void OledMenu::drawFiles() {
    display_->setTextSize(1);
    display_->setCursor(0, 0);
    display_->println("Recordings");
    const auto names = g_storage.listDir("/recordings");
    int y = 10;
    for (const auto& n : names) {
        if (y > kHeight - 12)
            break;
        display_->setCursor(0, y);
        display_->println(n);
        y += 9;
    }
    if (names.empty()) {
        display_->setCursor(0, 20);
        display_->println("(no recordings)");
    }
}

void OledMenu::drawInfo() {
    display_->setTextSize(1);
    char buf[24];
    std::snprintf(buf, sizeof(buf), "v%s", PHM_VERSION_STRING);
    display_->setCursor(0, 0);
    display_->print(PHM_NAME);
    display_->setCursor(0, 10);
    display_->print(buf);
    display_->setCursor(0, 20);
    display_->print(phm::hal::g_board.boardName());

    formatUptime(buf, sizeof(buf));
    display_->setCursor(0, 30);
    display_->print(buf);

    std::snprintf(buf, sizeof(buf), "Heap: %u", static_cast<unsigned>(ESP.getFreeHeap()));
    display_->setCursor(0, 40);
    display_->print(buf);
    std::snprintf(buf, sizeof(buf), "Bat: %u mV", static_cast<unsigned>(g_state.vbat_mV));
    display_->setCursor(0, 50);
    display_->print(buf);
}

void OledMenu::drawAttack() {
    display_->setTextSize(2);
    centerText(attackName_, 0);
    display_->setTextSize(1);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "ch %u", attackChannel_);
    display_->setCursor(0, 22);
    display_->print(buf);

    // Tiny RSSI bar
    const int barX = 0;
    const int barY = 36;
    const int barW = kWidth;
    const int barH = 8;
    display_->drawRect(barX, barY, barW, barH, SSD1306_WHITE);
    const int fill = rssiBars(attackRssi_);
    display_->fillRect(barX + 1, barY + 1, fill, barH - 2, SSD1306_WHITE);
    display_->setCursor(0, 50);
    std::snprintf(buf, sizeof(buf), "RSSI %d dBm", static_cast<int>(attackRssi_));
    display_->print(buf);
    display_->setCursor(0, 58);
    display_->print("Long OK = stop");
}

void OledMenu::drawStatusBar() {
    // Top 8 px reserved for the status bar. If the renderer drew text in
    // that area, undraw it by drawing a black rectangle first.
    display_->fillRect(0, 0, kWidth, 8, SSD1306_BLACK);
    display_->setTextSize(1);
    display_->setTextColor(SSD1306_WHITE);
    display_->setCursor(0, 0);

    // WiFi icon: number of clients
    char buf[16];
    std::snprintf(buf, sizeof(buf), "AP:%u", g_state.apActive ? 1 : 0);
    display_->print(buf);

    // Center: uptime
    char ubuf[16];
    formatUptime(ubuf, sizeof(ubuf));
    const int w = std::strlen(ubuf) * 6;
    display_->setCursor((kWidth - w) / 2, 0);
    display_->print(ubuf);

    // Right: temperature
    display_->setCursor(kWidth - 30, 0);
    std::snprintf(buf, sizeof(buf), "%.0fC", g_state.internalTempC);
    display_->print(buf);
}

void OledMenu::drawMenuItem(uint8_t index, const char* text, bool selected) {
    const int y = 10 + index * 9;
    if (selected) {
        display_->fillRect(0, y - 1, kWidth, 9, SSD1306_WHITE);
        display_->setTextColor(SSD1306_BLACK);
    } else {
        display_->setTextColor(SSD1306_WHITE);
    }
    display_->setCursor(2, y);
    display_->print(text);
    if (selected) {
        display_->setTextColor(SSD1306_WHITE);
    }
}

// ===========================================================================
// Input
// ===========================================================================
void OledMenu::handleButton(phm::hal::ButtonId id, bool pressed, bool longPress, bool doublePress) {
    if (!pressed)
        return;

    if (buttonConfig_ == 0) {
        // 1-button: short = next, long = select, double = back
        if (id == phm::hal::ButtonId::Ok) {
            if (longPress)
                onSelect();
            else if (doublePress)
                onBack();
            else
                onNext();
        }
    } else if (buttonConfig_ == 1) {
        // 2-button: Next = next, OK short = select, OK long = back
        if (id == phm::hal::ButtonId::Next) {
            onNext();
        } else if (id == phm::hal::ButtonId::Ok) {
            if (longPress)
                onBack();
            else
                onSelect();
        }
    } else {
        // 3-button: Prev / Next / OK (long OK = back)
        if (id == phm::hal::ButtonId::Prev)
            onPrev();
        else if (id == phm::hal::ButtonId::Next)
            onNext();
        else if (id == phm::hal::ButtonId::Ok) {
            if (longPress)
                onBack();
            else
                onSelect();
        }
    }
    dirty_ = true;
}

void OledMenu::onSelect() {
    if (current_ == Screen::MainMenu) {
        current_ = kMainMenu[menuIndex_].screen;
        menuIndex_ = 0;
        return;
    }
    if (current_ == Screen::Settings) {
        // The full settings UI is on the web; from here we just toggle
        // a couple of toggles for the headless user.
        showInfo("See web UI for full settings");
        return;
    }
    // For all submenus, OK starts the matching attack
    const char* cmd = nullptr;
    switch (current_) {
    case Screen::BluetoothJam: cmd = "attack bt"; break;
    case Screen::WifiAttack: cmd = "attack wifi"; break;
    case Screen::BleAttack: cmd = "attack ble"; break;
    case Screen::Spectrum2_4GHz: cmd = "attack 2_4"; break;
    case Screen::SubGhzJam: cmd = "attack subghz"; break;
    case Screen::SpectrumSubGHz: cmd = "attack spec"; break;
    default: break;
    }
    if (cmd != nullptr) {
        std::strncpy(attackName_, kMainMenu[menuIndex_].label, sizeof(attackName_) - 1);
        attackName_[sizeof(attackName_) - 1] = '\0';
        // The attack module will set attackChannel_/attackRssi_ via the
        // event bus; we just switch to the "running" screen.
        const String r = g_console.processLine(cmd);
        current_ = Screen::Attack;
        attackChannel_ = 0;
        attackRssi_ = -100;
        showInfo(r.c_str());
    }
}

void OledMenu::onBack() {
    if (current_ == Screen::MainMenu) {
        // From the main menu, long-pressing OK resets to the Info screen
        current_ = Screen::Info;
        return;
    }
    if (current_ == Screen::Attack) {
        // Stop the current attack
        g_console.processLine("attack stop");
        current_ = Screen::MainMenu;
        return;
    }
    current_ = Screen::MainMenu;
    menuIndex_ = 0;
}

void OledMenu::onNext() {
    if (current_ == Screen::MainMenu) {
        menuIndex_ = (menuIndex_ + 1) % kMainMenuCount;
    } else if (current_ == Screen::Settings) {
        // Cycle button config 0/1/2
        buttonConfig_ = (buttonConfig_ + 1) % 3;
        g_storage.setInt("buttons.config", buttonConfig_);
        g_buttons.setConfig(buttonConfig_);
        g_storage.commit();
        showInfo("Button config changed");
    } else {
        // In submenus, Next cycles a parameter (channel, etc.)
        attackChannel_ = (attackChannel_ + 1) % 125;
    }
}

void OledMenu::onPrev() {
    if (current_ == Screen::MainMenu) {
        menuIndex_ = (menuIndex_ == 0) ? (kMainMenuCount - 1) : (menuIndex_ - 1);
    } else if (current_ == Screen::Settings) {
        buttonConfig_ = (buttonConfig_ == 0) ? 2 : (buttonConfig_ - 1);
        g_storage.setInt("buttons.config", buttonConfig_);
        g_buttons.setConfig(buttonConfig_);
        g_storage.commit();
        showInfo("Button config changed");
    } else {
        attackChannel_ = (attackChannel_ == 0) ? 124 : (attackChannel_ - 1);
    }
}

// ===========================================================================
// Misc
// ===========================================================================
void OledMenu::centerText(const char* text, int y) {
    if (text == nullptr)
        return;
    // We assume the caller has set the text size before calling.
    // For the default size 1, each char is 6 px wide; for size 2, 12 px.
    // The drawSplash() caller sets size 2 explicitly; everything else
    // uses size 1.
    const int charW = 6;  // size 1
    const int w = std::strlen(text) * charW;
    int x = (kWidth - w) / 2;
    if (x < 0)
        x = 0;
    display_->setCursor(x, y);
    display_->print(text);
}

void OledMenu::formatUptime(char* out, size_t len) {
    const uint32_t ms = millis();
    const uint32_t s = ms / 1000UL;
    const uint32_t h = s / 3600UL;
    const uint32_t m = (s / 60UL) % 60UL;
    const uint32_t ss = s % 60UL;
    std::snprintf(out, len, "%lu:%02lu:%02lu", static_cast<unsigned long>(h), static_cast<unsigned long>(m),
                  static_cast<unsigned long>(ss));
}

int OledMenu::rssiBars(int8_t rssi) {
    // Map -100..-40 dBm to 0..(kWidth-2)
    if (rssi < -100)
        rssi = -100;
    if (rssi > -40)
        rssi = -40;
    return ((rssi + 100) * (kWidth - 2)) / 60;
}

}  // namespace phm::ui
