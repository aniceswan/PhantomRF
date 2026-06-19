/**
 * @file Board.cpp
 * @brief Per-board pin tables and Board implementation
 *
 * This file contains the only `#if PHM_BOARD == ...` cascade. Every
 * other translation unit goes through `g_board.pin(PinRole::X)` to
 * discover the actual GPIO number, so the rest of the code base is
 * board-agnostic.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "hal/Board.h"

#include <Arduino.h>
#include <initializer_list>

#include "core/Config.h"

namespace phm::hal {

// ---------------------------------------------------------------------------
// Singleton instance
// ---------------------------------------------------------------------------
Board g_board;

// ---------------------------------------------------------------------------
// Per-board pin tables
// ---------------------------------------------------------------------------

#if PHM_BOARD == PHM_BOARD_ESP32S3_N16R8

// ESP32-S3-WROOM-1-N16R8 (primary target). 8 MB octal PSRAM, 16 MB flash.
static constexpr int8_t kPins[] = {
    // nRF24 — see DESIGN §8.1
    /* Nrf24Ce0  */ 16,
    /* Nrf24Csn0 */ 15,
    /* Nrf24Ce1  */ 18,
    /* Nrf24Csn1 */ 17,
    /* Nrf24Ce2  */ 35,
    /* Nrf24Csn2 */ 21,
    /* Nrf24Ce3  */ 37,
    /* Nrf24Csn3 */ 36,
    /* Nrf24Ce4  */ 39,
    /* Nrf24Csn4 */ 38,
    /* Nrf24Sck  */ 12,
    /* Nrf24Miso */ 13,
    /* Nrf24Mosi */ 11,

    // CC1101 (VSPI)
    /* Cc1101Sck  */ 36,
    /* Cc1101Miso */ 37,
    /* Cc1101Mosi */ 38,
    /* Cc1101Csn0 */ 39,
    /* Cc1101Gdo0 */ 2,
    /* Cc1101Gdo2 */ 4,
    /* Cc1101Csn1 */ -1,  // M1 feature, not present in M0

    // OLED I2C
    /* OledSda */ 8,
    /* OledScl */ 9,

    // Buttons (active-low, internal pull-up)
    /* ButtonOk   */ 10,
    /* ButtonNext */ 14,
    /* ButtonPrev */ 21,

    // RGB LED (PWM-capable)
    /* LedR */ 5,
    /* LedG */ 6,
    /* LedB */ 7,

    // Battery monitor
    /* VbatAdc */ 1,

    // USB (native, on S3)
    /* UsbDp */ 19,
    /* UsbDm */ 20,
};

static constexpr const char* kBoardName = "ESP32-S3-N16R8";
static constexpr int         kBoardId   = PHM_BOARD_ESP32S3_N16R8;

#elif PHM_BOARD == PHM_BOARD_ESP32S3_N8R2

// ESP32-S3-WROOM-1-N8R2 — same pinout as N16R8, just less flash/PSRAM
static constexpr int8_t kPins[] = {
    /* Nrf24Ce0  */ 16, /* Nrf24Csn0 */ 15,
    /* Nrf24Ce1  */ 18, /* Nrf24Csn1 */ 17,
    /* Nrf24Ce2  */ 35, /* Nrf24Csn2 */ 21,
    /* Nrf24Ce3  */ 37, /* Nrf24Csn3 */ 36,
    /* Nrf24Ce4  */ 39, /* Nrf24Csn4 */ 38,
    /* Nrf24Sck  */ 12, /* Nrf24Miso */ 13, /* Nrf24Mosi */ 11,
    /* Cc1101Sck  */ 36, /* Cc1101Miso */ 37, /* Cc1101Mosi */ 38,
    /* Cc1101Csn0 */ 39, /* Cc1101Gdo0 */ 2, /* Cc1101Gdo2 */ 4, /* Cc1101Csn1 */ -1,
    /* OledSda */ 8,  /* OledScl */ 9,
    /* ButtonOk */ 10, /* ButtonNext */ 14, /* ButtonPrev */ 21,
    /* LedR */ 5, /* LedG */ 6, /* LedB */ 7,
    /* VbatAdc */ 1,
    /* UsbDp */ 19, /* UsbDm */ 20,
};
static constexpr const char* kBoardName = "ESP32-S3-N8R2";
static constexpr int         kBoardId   = PHM_BOARD_ESP32S3_N8R2;

#elif PHM_BOARD == PHM_BOARD_ESP32S3_N8

// ESP32-S3-WROOM-1-N8 — no PSRAM, smaller flash, same GPIOs as N16R8
static constexpr int8_t kPins[] = {
    /* Nrf24Ce0  */ 16, /* Nrf24Csn0 */ 15,
    /* Nrf24Ce1  */ 18, /* Nrf24Csn1 */ 17,
    /* Nrf24Ce2  */ 35, /* Nrf24Csn2 */ 21,
    /* Nrf24Ce3  */ 37, /* Nrf24Csn3 */ 36,
    /* Nrf24Ce4  */ 39, /* Nrf24Csn4 */ 38,
    /* Nrf24Sck  */ 12, /* Nrf24Miso */ 13, /* Nrf24Mosi */ 11,
    /* Cc1101Sck  */ 36, /* Cc1101Miso */ 37, /* Cc1101Mosi */ 38,
    /* Cc1101Csn0 */ 39, /* Cc1101Gdo0 */ 2, /* Cc1101Gdo2 */ 4, /* Cc1101Csn1 */ -1,
    /* OledSda */ 8,  /* OledScl */ 9,
    /* ButtonOk */ 10, /* ButtonNext */ 14, /* ButtonPrev */ 21,
    /* LedR */ 5, /* LedG */ 6, /* LedB */ 7,
    /* VbatAdc */ 1,
    /* UsbDp */ 19, /* UsbDm */ 20,
};
static constexpr const char* kBoardName = "ESP32-S3-N8";
static constexpr int         kBoardId   = PHM_BOARD_ESP32S3_N8;

#elif PHM_BOARD == PHM_BOARD_ESP32_CLASSIC

// Classic ESP32 (Xtensa LX6) — fewer usable GPIOs and some are strapping
// pins. See DESIGN §8.2 for the rationale behind each assignment.
static constexpr int8_t kPins[] = {
    /* Nrf24Ce0  */ 16, /* Nrf24Csn0 */ 15,
    /* Nrf24Ce1  */ 18, /* Nrf24Csn1 */ 17,
    /* Nrf24Ce2  */ -1, /* Nrf24Csn2 */ -1,
    /* Nrf24Ce3  */ -1, /* Nrf24Csn3 */ -1,
    /* Nrf24Ce4  */ -1, /* Nrf24Csn4 */ -1,
    /* Nrf24Sck  */ 14, /* Nrf24Miso */ 12, /* Nrf24Mosi */ 13,
    /* Cc1101Sck  */ 18, /* Cc1101Miso */ 19, /* Cc1101Mosi */ 23,
    /* Cc1101Csn0 */ 5,  /* Cc1101Gdo0 */ 27, /* Cc1101Gdo2 */ 4, /* Cc1101Csn1 */ -1,
    /* OledSda */ 21, /* OledScl */ 22,
    /* ButtonOk */ 25, /* ButtonNext */ 26, /* ButtonPrev */ 27,
    /* LedR */ 2,  /* LedG */ -1, /* LedB */ -1,  // classic has only a single LED
    /* VbatAdc */ 34,
    /* UsbDp */ -1, /* UsbDm */ -1,  // classic needs a USB-UART bridge
};
static constexpr const char* kBoardName = "ESP32-Classic";
static constexpr int         kBoardId   = PHM_BOARD_ESP32_CLASSIC;

#elif PHM_BOARD == PHM_BOARD_ESP32C3

// ESP32-C3 (RISC-V, single core) — limited features, 2.4 GHz only
static constexpr int8_t kPins[] = {
    /* Nrf24Ce0  */ 10, /* Nrf24Csn0 */ 7,
    /* Nrf24Ce1  */ -1, /* Nrf24Csn1 */ -1,
    /* Nrf24Ce2  */ -1, /* Nrf24Csn2 */ -1,
    /* Nrf24Ce3  */ -1, /* Nrf24Csn3 */ -1,
    /* Nrf24Ce4  */ -1, /* Nrf24Csn4 */ -1,
    /* Nrf24Sck  */ 4,  /* Nrf24Miso */ 5, /* Nrf24Mosi */ 6,
    /* Cc1101Sck  */ -1, /* Cc1101Miso */ -1, /* Cc1101Mosi */ -1,
    /* Cc1101Csn0 */ -1, /* Cc1101Gdo0 */ -1, /* Cc1101Gdo2 */ -1, /* Cc1101Csn1 */ -1,
    /* OledSda */ 8,  /* OledScl */ 9,
    /* ButtonOk */ 3,  /* ButtonNext */ -1, /* ButtonPrev */ -1,
    /* LedR */ 2,  /* LedG */ -1, /* LedB */ -1,
    /* VbatAdc */ 0,
    /* UsbDp */ -1, /* UsbDm */ -1,
};
static constexpr const char* kBoardName = "ESP32-C3";
static constexpr int         kBoardId   = PHM_BOARD_ESP32C3;

#elif PHM_BOARD == PHM_BOARD_ESP32S2

// ESP32-S2-Saola-1 (USB OTG, no BLE)
static constexpr int8_t kPins[] = {
    /* Nrf24Ce0  */ 16, /* Nrf24Csn0 */ 15,
    /* Nrf24Ce1  */ 18, /* Nrf24Csn1 */ 17,
    /* Nrf24Ce2  */ 35, /* Nrf24Csn2 */ 21,
    /* Nrf24Ce3  */ 37, /* Nrf24Csn3 */ 36,
    /* Nrf24Ce4  */ 39, /* Nrf24Csn4 */ 38,
    /* Nrf24Sck  */ 12, /* Nrf24Miso */ 13, /* Nrf24Mosi */ 11,
    /* Cc1101Sck  */ 36, /* Cc1101Miso */ 37, /* Cc1101Mosi */ 38,
    /* Cc1101Csn0 */ 39, /* Cc1101Gdo0 */ 2, /* Cc1101Gdo2 */ 4, /* Cc1101Csn1 */ -1,
    /* OledSda */ 8,  /* OledScl */ 9,
    /* ButtonOk */ 10, /* ButtonNext */ 14, /* ButtonPrev */ 21,
    /* LedR */ 5,  /* LedG */ 6, /* LedB */ 7,
    /* VbatAdc */ 1,
    /* UsbDp */ 19, /* UsbDm */ 20,
};
static constexpr const char* kBoardName = "ESP32-S2";
static constexpr int         kBoardId   = PHM_BOARD_ESP32S2;

#else
    #error "Unsupported PHM_BOARD value"
#endif

static_assert(sizeof(kPins) / sizeof(kPins[0]) == static_cast<size_t>(PinRole::Count),
              "Pin table size must match PinRole::Count");

// ---------------------------------------------------------------------------
void Board::setup() {
    setupPins();
}

// ---------------------------------------------------------------------------
int8_t Board::pin(PinRole role) const {
    const auto idx = static_cast<size_t>(role);
    if (idx >= sizeof(kPins) / sizeof(kPins[0])) {
        return -1;
    }
    return kPins[idx];
}

// ---------------------------------------------------------------------------
const char* Board::boardName() const {
    return kBoardName;
}

// ---------------------------------------------------------------------------
int Board::boardId() const {
    return kBoardId;
}

// ---------------------------------------------------------------------------
void Board::setupPins() {
    // Buttons: enable internal pull-up. The board wiring is active-low.
    for (PinRole r : {PinRole::ButtonOk, PinRole::ButtonNext, PinRole::ButtonPrev}) {
        const int8_t p = pin(r);
        if (p >= 0) {
            pinMode(p, INPUT_PULLUP);
        }
    }

    // LED pins: outputs, off by default
    for (PinRole r : {PinRole::LedR, PinRole::LedG, PinRole::LedB}) {
        const int8_t p = pin(r);
        if (p >= 0) {
            pinMode(p, OUTPUT);
            digitalWrite(p, LOW);
        }
    }

    // nRF24 control pins: CE default low, CSN default high (de-selected)
    for (PinRole r : {PinRole::Nrf24Ce0, PinRole::Nrf24Ce1, PinRole::Nrf24Ce2,
                      PinRole::Nrf24Ce3, PinRole::Nrf24Ce4}) {
        const int8_t p = pin(r);
        if (p >= 0) {
            pinMode(p, OUTPUT);
            digitalWrite(p, LOW);
        }
    }
    for (PinRole r : {PinRole::Nrf24Csn0, PinRole::Nrf24Csn1, PinRole::Nrf24Csn2,
                      PinRole::Nrf24Csn3, PinRole::Nrf24Csn4}) {
        const int8_t p = pin(r);
        if (p >= 0) {
            pinMode(p, OUTPUT);
            digitalWrite(p, HIGH);
        }
    }

    // CC1101: CSN high, GDO0/GDO2 left as inputs (chip drives them)
    for (PinRole r : {PinRole::Cc1101Csn0, PinRole::Cc1101Csn1}) {
        const int8_t p = pin(r);
        if (p >= 0) {
            pinMode(p, OUTPUT);
            digitalWrite(p, HIGH);
        }
    }
    for (PinRole r : {PinRole::Cc1101Gdo0, PinRole::Cc1101Gdo2}) {
        const int8_t p = pin(r);
        if (p >= 0) {
            pinMode(p, INPUT);
        }
    }

    // Battery ADC pin — set as analog input elsewhere (analogRead)
    if (pin(PinRole::VbatAdc) >= 0) {
        pinMode(pin(PinRole::VbatAdc), INPUT);
    }
}

}  // namespace phm::hal
