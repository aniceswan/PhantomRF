/**
 * @file Board.h
 * @brief Per-board pin assignments and runtime pin lookup
 *
 * Different ESP32 variants expose different GPIOs and have different
 * "safe" pins (e.g. GPIO 2 is a strapping pin on the classic ESP32
 * but not on the S3). `Board` resolves a `PinRole` to a concrete
 * GPIO number for the currently selected board, which is set in
 * build flags as `-DPHM_BOARD=PHM_BOARD_ESP32S3_N16R8`.
 *
 * Pin tables live in `Board.cpp` to keep this header small and to
 * avoid leaking the giant `#if` cascades into every TU that needs
 * `PinRole`.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

namespace phm::hal {

/**
 * @brief Logical role for a pin, decoupled from the GPIO number
 *
 * Use this enum to ask `Board` for the right GPIO rather than
 * hard-coding numbers. The same `PinRole` resolves to different
 * numbers on different boards.
 *
 * Note: values are kept dense (no gaps) so the pin table in
 * `Board.cpp` can be a simple array indexed by the enum.
 */
enum class PinRole : uint8_t {
    // ---- nRF24 (up to 5 modules on shared HSPI) -------------------------
    Nrf24Ce0  = 0,  ///< Chip Enable, nRF24 module 0
    Nrf24Csn0 = 1,  ///< Chip Select (active low), nRF24 module 0
    Nrf24Ce1  = 2,
    Nrf24Csn1 = 3,
    Nrf24Ce2  = 4,
    Nrf24Csn2 = 5,
    Nrf24Ce3  = 6,
    Nrf24Csn3 = 7,
    Nrf24Ce4  = 8,
    Nrf24Csn4 = 9,
    Nrf24Sck  = 10, ///< SPI clock (shared)
    Nrf24Miso = 11, ///< SPI MISO (shared)
    Nrf24Mosi = 12, ///< SPI MOSI (shared)

    // ---- CC1101 (1-2 modules on VSPI) -----------------------------------
    Cc1101Sck  = 13,
    Cc1101Miso = 14,
    Cc1101Mosi = 15,
    Cc1101Csn0 = 16,
    Cc1101Gdo0 = 17, ///< General Digital Output 0 (often used as IRQ)
    Cc1101Gdo2 = 18,
    Cc1101Csn1 = 19, ///< Second CC1101 (M1)

    // ---- OLED (I2C) ------------------------------------------------------
    OledSda = 20,
    OledScl = 21,

    // ---- User input ------------------------------------------------------
    ButtonOk   = 22, ///< Centre / select button
    ButtonNext = 23, ///< Move forward in menus
    ButtonPrev = 24, ///< Move backward in menus

    // ---- Status LED (RGB, PWM-capable) ----------------------------------
    LedR = 25,
    LedG = 26,
    LedB = 27,

    // ---- Battery monitor -------------------------------------------------
    VbatAdc = 28,

    // ---- USB (native on S2/S3, requires bridge on classic) -------------
    UsbDp = 29,
    UsbDm = 30,

    // ---- End sentinel ---------------------------------------------------
    Count = 31
};

/**
 * @brief Singleton — resolves PinRole → GPIO number for the current board
 */
class Board {
public:
    /// Configure all pins (mode + pull). Must be called once during boot.
    void setup();

    /// Look up the GPIO number for a logical pin role
    /// @return GPIO number, or -1 if the role is not used on this board
    int8_t pin(PinRole role) const;

    /// Short name of the current board, e.g. "ESP32-S3-N16R8"
    const char* boardName() const;

    /// Numeric ID of the current board (one of PHM_BOARD_*)
    int boardId() const;

private:
    void setupPins();
};

/// Singleton instance, defined in Board.cpp
extern Board g_board;

}  // namespace phm::hal
