/**
 * @file main.cpp
 * @brief PhantomRF top-level entry point
 *
 * Provides the Arduino `setup()` and `loop()` hooks. Creates the
 * `PhantomRF` singleton, brings it up, then ticks it every loop.
 *
 * Module registration is intentionally empty in M0 — attack modules
 * (Nrf24Jammer, WifiAttack, …) are added in their own tasks. The
 * PhantomRF core does not require any module to be registered.
 *
 * @author PhantomRF Project
 * @date 2026
 */

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "core/Config.h"
#include "core/Module.h"
#include "core/PhantomRF.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Led.h"
#include "hal/Power.h"
#include "hal/Storage.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

namespace phm {

// g_app is defined in core/PhantomRF.cpp

}  // namespace phm

// ---------------------------------------------------------------------------
// Arduino lifecycle
// ---------------------------------------------------------------------------
void setup() {
    using namespace phm;

    // ---- Disable the task watchdog timers -------------------------------
    // Jammer workers intentionally block for many milliseconds during a
    // sweep; the default 5-second TWDT would kill them. See DESIGN §3.3.
    disableCore0WDT();
#if !defined(PHM_BOARD_ESP32C3) && !defined(PHM_BOARD_ESP32S2)
    // C3 and S2 are single-core; no core 1 to disable
    disableCore1WDT();
#endif

    // ---- Serial ---------------------------------------------------------
    // On boards with native USB (ESP32-S2/S3) we use the USB CDC port
    // directly. On classic ESP32 and C3 the first UART (Serial0) is the
    // programming port — but arduino-esp32 v3 still calls it `Serial`.
    Serial.begin(115200);
    delay(500);  // let the host-side serial settle

    Serial.println();
    Serial.println();
    Serial.print("=== ");
    Serial.print(PHM_NAME);
    Serial.print(" v");
    Serial.print(PHM_VERSION_STRING);
    Serial.println(" ===");
    Serial.print("Board: ");
    Serial.println(hal::g_board.boardName());
    Serial.println("Initializing...");

    // ---- Build the application ------------------------------------------
    g_app = new PhantomRF();
    g_app->setup();

    Serial.println();
    Serial.println("Ready. Connect to the AP and open the web UI.");
    Serial.println();
}

// ---------------------------------------------------------------------------
void loop() {
    using namespace phm;

    if (g_app == nullptr) {
        // setup() never ran; nothing to do
        return;
    }
    g_app->loop();
}
