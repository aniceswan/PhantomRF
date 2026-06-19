/**
 * @file Cc1101.cpp
 * @brief CC1101 sub-GHz driver wrapper — implementation
 *
 * Wraps `SmartRC_CC1101` from `LSatan/SmartRC-CC1101-Driver-Lib@^3.0.2`.
 * The class is named `SmartRC_CC1101` in the V3.0.2 library; older
 * sketches used `ELECHOUSE_CC1101` (a backward-compat typedef) — we
 * forward-declare the real class name in `Cc1101.h` and include the
 * header here.
 * in `Cc1101.h` and include the library header here.
 *
 * Notable differences from the upstream W0rthlessS0ul
 * `CC1101_jammer` project (DESIGN §20.3):
 *
 *   - `setCCMode(2)` was a no-op that left the chip in async mode
 *     after record/replay. We use `setCCMode(1)` (`NormalPacket`)
 *     explicitly.
 *   - The `jamdata` buffer is allocated **once** in `setup()` and
 *     reused, instead of being re-allocated on every attack (which
 *     leaked).
 *   - SetTx / SetRx are called in matched pairs so the chip is left
 *     in a defined state on exit.
 *
 * All jam / record / replay operations are driven by a single worker
 * task pinned to Core 0 at priority 3 (DESIGN §3.3) so the main loop
 * stays non-blocking.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "radio/Cc1101.h"

#include "core/Config.h"
#include "hal/Board.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

#include <cstring>

#include <Arduino.h>
#include <SmartRC_CC1101.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// NOTE: We deliberately do NOT include `core/EventBus.h` here. That
// header in this project still re-declares `BaseType_t` and other
// FreeRTOS types with different (conflicting) definitions — a
// pre-existing project bug. Posting progress events is the
// responsibility of the higher-level `Cc1101Jammer` module, not the
// radio driver; we expose a getter so it can read the current
// frequency without going through the event bus.

namespace phm::radio {

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------
static constexpr const char* kTag = "cc1101";
static constexpr UBaseType_t kTaskPrio = 3;
static constexpr uint32_t kTaskStack = 4096;
static constexpr BaseType_t kTaskCore = 0;
static constexpr size_t kDefaultRecordBytes = 32 * 1024;  ///< 32 KB
static constexpr size_t kDefaultJamBytes = 64;            ///< junk-data buffer
static constexpr uint8_t kKeyfobCount = 9;

// 9 common keyfob bands, MHz — lifted from W0rthlessS0ul options.cpp:4
static const float kKeyfobFreqs[kKeyfobCount] = {303.00f, 310.00f, 315.00f, 330.00f, 350.00f,
                                                 370.00f, 390.00f, 418.00f, 433.92f};

// Singleton
Cc1101Manager g_cc1101;

// ---------------------------------------------------------------------------
// Cc1101 — single module
// ---------------------------------------------------------------------------
Cc1101::Cc1101() = default;

Cc1101::~Cc1101() {
    stop();
    if (radio_ != nullptr) {
        delete radio_;
        radio_ = nullptr;
    }
    freeRecordBuffer();
    if (jamData_ != nullptr) {
        delete[] jamData_;
        jamData_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
bool Cc1101::setup(int8_t sck, int8_t miso, int8_t mosi, int8_t csn, int8_t gdo0, int8_t gdo2) {
    // Any pin == -1 → module not wired on this board
    if (sck < 0 || miso < 0 || mosi < 0 || csn < 0) {
        LOGD(kTag, "cc1101: pins not wired, skipping");
        return false;
    }

    radio_ = new SmartRC_CC1101();
    if (radio_ == nullptr) {
        LOGE(kTag, "SmartRC_CC1101 alloc failed");
        return false;
    }

    // The library V3.0.2 REQUIRES setSpiPin() + setGDO() before Init().
    // Calling Init() first is a no-op (the lib's constructor doesn't
    // touch the bus), but the pin state from a previous run can leak.
    radio_->setSpiPin(static_cast<uint8_t>(sck), static_cast<uint8_t>(miso), static_cast<uint8_t>(mosi),
                      static_cast<uint8_t>(csn));
    radio_->setGDO(static_cast<uint8_t>(gdo0), static_cast<uint8_t>(gdo2));
    radio_->Init();
    radio_->setGDO0(static_cast<uint8_t>(gdo0));

    // Sanity probe: the VERSION register reads 0x04 / 0x17 / 0x18 / 0x19
    // for genuine CC1101. Anything else and we are talking to a hung bus.
    const uint8_t ver = radio_->SpiReadReg(0x31);  // CC1101_VERSION
    if (ver == 0x00 || ver == 0xFF) {
        LOGW(kTag, "cc1101: VERSION register 0x%02X — chip not responding", static_cast<unsigned>(ver));
        delete radio_;
        radio_ = nullptr;
        return false;
    }
    LOGD(kTag, "cc1101: VERSION=0x%02X", static_cast<unsigned>(ver));

    applyJammingDefaults();

    // Allocate the jam-data buffer ONCE (fixes the upstream memory leak
    // where the global `jamdata` was re-allocated on every attack).
    if (jamData_ == nullptr) {
        jamDataSize_ = kDefaultJamBytes;
        jamData_ = new uint8_t[jamDataSize_];
        // Deliberately NOT zeroed — junk data is fine for jamming
    }

    // Allocate the record buffer in PSRAM when available
    allocateRecordBuffer(kDefaultRecordBytes);

    running_ = true;  // chip is present
    currentFreq_ = 433.92f;
    setMode(CcMode::NormalPacket);

    LOGI(kTag, "cc1101: ready (sck=%d miso=%d mosi=%d csn=%d gdo0=%d gdo2=%d)", static_cast<int>(sck),
         static_cast<int>(miso), static_cast<int>(mosi), static_cast<int>(csn), static_cast<int>(gdo0),
         static_cast<int>(gdo2));
    return true;
}

// ---------------------------------------------------------------------------
void Cc1101::applyJammingDefaults() {
    if (radio_ == nullptr)
        return;
    // W0rthlessS0ul `cc1101initialize()` — translated to the library
    // API. Order matches the upstream: mode → modulation → channel →
    // spacing → BW → rate → PA → sync → packet format → housekeeping.
    radio_->setCCMode(1);      // 1 = NormalPacket (NOT 2; 2 is a bug)
    radio_->setModulation(2);  // 2 = ASK/OOK
    radio_->setDeviation(47.60f);
    radio_->setChannel(0);
    radio_->setChsp(199.95f);
    radio_->setRxBW(812.50f);
    radio_->setDRate(9.6f);  // 9.6 kbps
    radio_->setPA(12);       // +12 dBm
    radio_->setSyncMode(2);
    radio_->setSyncWord(211, 145);
    radio_->setAdrChk(0);
    radio_->setAddr(0);
    radio_->setWhiteData(false);
    radio_->setPktFormat(0);
    radio_->setLengthConfig(1);
    radio_->setPacketLength(0);
    radio_->setCrc(false);
    radio_->setCRC_AF(false);
    radio_->setDcFilterOff(false);
    radio_->setManchester(false);
    radio_->setFEC(false);
    radio_->setPRE(0);
    radio_->setPQT(0);
    radio_->setAppendStatus(false);
}

// ---------------------------------------------------------------------------
void Cc1101::setFrequency(float mhz) {
    if (radio_ == nullptr)
        return;
    radio_->setMHZ(mhz);  // lib writes FREQ2/1/0 + calls Calibrate()
    currentFreq_ = mhz;
}

int8_t Cc1101::getRssiDbm() const {
    if (radio_ == nullptr)
        return -128;
    // The SmartRC-CC1101-Driver-Lib exposes `getRssi()` which reads the
    // RSSI status register and converts to dBm using the data-rate
    // dependent offset table. Returns a signed dBm value.
    return static_cast<int8_t>(radio_->getRssi());
}

void Cc1101::setModulation(CcModulation mod) {
    if (radio_ == nullptr)
        return;
    radio_->setModulation(static_cast<uint8_t>(mod));
    currentMod_ = mod;
}

void Cc1101::setMode(CcMode mode) {
    if (radio_ == nullptr)
        return;
    radio_->setCCMode(static_cast<uint8_t>(mode));
    currentMode_ = mode;
}

void Cc1101::setTxPower(int8_t dbm) {
    if (radio_ == nullptr)
        return;
    radio_->setPA(dbm);
    currentPower_ = dbm;
}

void Cc1101::setDataRate(float kbps) {
    if (radio_ == nullptr)
        return;
    radio_->setDRate(kbps);
    currentDataRate_ = kbps;
}

void Cc1101::setRxBandwidth(float khz) {
    if (radio_ == nullptr)
        return;
    radio_->setRxBW(khz);
}

// ---------------------------------------------------------------------------
bool Cc1101::transmit(const uint8_t* data, uint8_t len) {
    if (radio_ == nullptr)
        return false;
    if (currentMode_ == CcMode::AsyncSerial) {
        // Packet TX in async-serial mode is undefined; force the chip
        // into normal mode and restore afterwards.
        radio_->setCCMode(1);
        radio_->SetTx();
        radio_->SendData(const_cast<uint8_t*>(data), len);
        radio_->setCCMode(0);  // back to async
    } else {
        radio_->SetTx();
        radio_->SendData(const_cast<uint8_t*>(data), len);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Internal helper — start a worker task with a configured `op_`.
// Returns false if the radio is missing or the task couldn't be created.
bool Cc1101::launchWorker(const char* taskName) {
    if (radio_ == nullptr)
        return false;
    stop();
    running_ = true;

    const BaseType_t ok = xTaskCreatePinnedToCore(&Cc1101::taskEntry, taskName, kTaskStack, this, kTaskPrio,
                                                  reinterpret_cast<TaskHandle_t*>(&task_), kTaskCore);
    if (ok != pdPASS || task_ == nullptr) {
        running_ = false;
        task_ = nullptr;
        LOGE(kTag, "%s: xTaskCreatePinnedToCore failed", taskName);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
bool Cc1101::startSpotJam(float mhz, uint8_t payloadSize) {
    op_ = Op::Spot;
    opStart_ = mhz;
    opStop_ = mhz;
    opStep_ = 0.0f;
    opPayload_ = (payloadSize == 0) ? 1 : payloadSize;
    return launchWorker("cc1101spot");
}

bool Cc1101::startRangeJam(float start, float stop, float stepMhz, uint8_t payloadSize) {
    op_ = Op::Range;
    opStart_ = start;
    opStop_ = stop;
    opStep_ = (stepMhz <= 0.0f) ? 1.0f : stepMhz;
    opPayload_ = (payloadSize == 0) ? 1 : payloadSize;
    return launchWorker("cc1101range");
}

bool Cc1101::startHopperJam(const float* freqs, uint8_t count, uint8_t payloadSize) {
    if (freqs == nullptr || count == 0)
        return false;
    op_ = Op::Hopper;
    opFreqs_ = freqs;
    opFreqCount_ = count;
    opPayload_ = (payloadSize == 0) ? 1 : payloadSize;
    return launchWorker("cc1101hopper");
}

bool Cc1101::startKeyfobJam(uint8_t index, uint8_t payloadSize) {
    if (index >= kKeyfobCount) {
        LOGW(kTag, "startKeyfobJam: index %u out of range", index);
        return false;
    }
    op_ = Op::Keyfob;
    opIndex_ = index;
    opPayload_ = (payloadSize == 0) ? 1 : payloadSize;
    return launchWorker("cc1101kfob");
}

// ---------------------------------------------------------------------------
bool Cc1101::startRecord(uint16_t durationMs) {
    if (radio_ == nullptr || recordBuf_ == nullptr)
        return false;
    op_ = Op::Record;
    opDeadlineMs_ = millis() + durationMs;
    recordedLen_ = 0;
    recording_ = true;
    return launchWorker("cc1101rec");
}

void Cc1101::stopRecord() {
    if (!recording_)
        return;
    stop();
}

uint16_t Cc1101::recordProgress() const {
    if (recordBufSize_ == 0)
        return 0;
    return static_cast<uint16_t>((recordedLen_ * 100u) / recordBufSize_);
}

bool Cc1101::startReplay(uint16_t durationMs) {
    if (radio_ == nullptr || recordBuf_ == nullptr || recordedLen_ == 0) {
        LOGW(kTag, "startReplay: nothing recorded");
        return false;
    }
    op_ = Op::Replay;
    opDeadlineMs_ = millis() + durationMs;
    replaying_ = true;
    return launchWorker("cc1101play");
}

void Cc1101::stopReplay() {
    if (!replaying_)
        return;
    stop();
}

// ---------------------------------------------------------------------------
void Cc1101::stop() {
    running_ = false;
    recording_ = false;
    replaying_ = false;

    if (task_ != nullptr) {
        auto* handle = static_cast<TaskHandle_t>(task_);
        for (int i = 0; i < 50 && eTaskGetState(handle) != eDeleted; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (eTaskGetState(handle) != eDeleted) {
            LOGW(kTag, "stop: worker did not exit in 500 ms; force-deleting");
            vTaskDelete(handle);
        }
        task_ = nullptr;
    }

    // Put the chip back in a known state. Per the upstream bug fix:
    //   setCCMode(1) for NormalPacket (not 2, which is silently async).
    if (radio_ != nullptr) {
        radio_->setCCMode(1);
        radio_->setPktFormat(0);
        radio_->SetTx();
        pinMode(hal::g_board.pin(hal::PinRole::Cc1101Gdo0), INPUT);
    }
    LOGD(kTag, "stop: done");
}

// ---------------------------------------------------------------------------
// Buffer allocation
// ---------------------------------------------------------------------------
void Cc1101::allocateRecordBuffer(size_t size) {
    if (size == 0)
        size = kDefaultRecordBytes;
    if (recordBuf_ != nullptr && recordBufSize_ >= size)
        return;

    freeRecordBuffer();
    // PSRAM first — it's plentiful on the S3 N16R8 (8 MB). Falls back
    // to internal RAM on boards without PSRAM.
    recordBuf_ = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM));
    if (recordBuf_ == nullptr) {
        recordBuf_ = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_INTERNAL));
    }
    if (recordBuf_ != nullptr) {
        recordBufSize_ = size;
        LOGD(kTag, "record buffer: %u bytes allocated", static_cast<unsigned>(size));
    } else {
        LOGW(kTag, "record buffer: allocation failed");
        recordBufSize_ = 0;
    }
}

void Cc1101::freeRecordBuffer() {
    if (recordBuf_ != nullptr) {
        heap_caps_free(recordBuf_);  // works for both PSRAM and internal
        recordBuf_ = nullptr;
        recordBufSize_ = 0;
    }
    recordedLen_ = 0;
}

// ---------------------------------------------------------------------------
// Worker entry point
// ---------------------------------------------------------------------------
void Cc1101::taskEntry(void* arg) {
    auto* self = static_cast<Cc1101*>(arg);
    if (self != nullptr)
        self->workerLoop();
    vTaskDelete(nullptr);
}

void Cc1101::workerLoop() {
    LOGD(kTag, "worker: entered (op=%u)", static_cast<unsigned>(op_));
    switch (op_) {
    case Op::Spot: workerSpot(); break;
    case Op::Range: workerRange(); break;
    case Op::Hopper: workerHopper(); break;
    case Op::Keyfob: workerKeyfob(); break;
    case Op::Record: workerRecord(); break;
    case Op::Replay: workerReplay(); break;
    case Op::None: break;
    }
    // Tear the chip back down to a known state on every exit path
    if (radio_ != nullptr) {
        radio_->setCCMode(1);
        radio_->setPktFormat(0);
        radio_->SetTx();
    }
    running_ = false;
    recording_ = false;
    replaying_ = false;
    LOGD(kTag, "worker: exited");
}

// ---------------------------------------------------------------------------
// Per-op worker bodies
// ---------------------------------------------------------------------------
void Cc1101::workerSpot() {
    if (radio_ == nullptr)
        return;
    setFrequency(opStart_);
    while (running_) {
        if (jamData_ == nullptr || jamDataSize_ == 0)
            break;
        radio_->SendData(jamData_, opPayload_);
        vTaskDelay(pdMS_TO_TICKS(0));  ///< yield
    }
}

void Cc1101::workerRange() {
    if (radio_ == nullptr)
        return;
    while (running_) {
        for (float f = opStart_; f <= opStop_ && running_; f += opStep_) {
            setFrequency(f);
            if (jamData_ == nullptr || jamDataSize_ == 0)
                break;
            radio_->SendData(jamData_, opPayload_);
            vTaskDelay(pdMS_TO_TICKS(5));  ///< 5 ms dwell per step
        }
    }
}

void Cc1101::workerHopper() {
    if (radio_ == nullptr || opFreqs_ == nullptr || opFreqCount_ == 0)
        return;
    while (running_) {
        for (uint8_t i = 0; i < opFreqCount_ && running_; ++i) {
            setFrequency(opFreqs_[i]);
            if (jamData_ == nullptr || jamDataSize_ == 0)
                break;
            radio_->SendData(jamData_, opPayload_);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void Cc1101::workerKeyfob() {
    if (radio_ == nullptr || opIndex_ >= kKeyfobCount)
        return;
    setFrequency(kKeyfobFreqs[opIndex_]);
    while (running_) {
        if (jamData_ == nullptr)
            break;
        radio_->SendData(jamData_, opPayload_);
        vTaskDelay(pdMS_TO_TICKS(0));  ///< yield
    }
}

void Cc1101::workerRecord() {
    if (radio_ == nullptr || recordBuf_ == nullptr)
        return;
    // Switch to async-serial RX (DESIGN §10.2)
    radio_->setCCMode(0);
    radio_->setPktFormat(3);
    radio_->SetRx();
    const int8_t gdo0 = hal::g_board.pin(hal::PinRole::Cc1101Gdo0);
    if (gdo0 >= 0)
        pinMode(gdo0, INPUT);

    recordedLen_ = 0;
    const int samplePin = (gdo0 >= 0) ? gdo0 : 2;
    for (size_t i = 0; i < recordBufSize_ && running_; ++i) {
        uint8_t receivedbyte = 0;
        for (int8_t j = 7; j >= 0 && running_; --j) {
            bitWrite(receivedbyte, j, digitalRead(samplePin));
            delayMicroseconds(samplingUs_);
        }
        recordBuf_[i] = receivedbyte;
        recordedLen_ = i + 1;
        if ((i & 0x1F) == 0) {
            vTaskDelay(pdMS_TO_TICKS(0));  ///< yield so stop() is observed
            if (millis() >= opDeadlineMs_)
                break;
        }
    }
    recording_ = false;

    // Restore normal-packet mode (NOT setCCMode(2) — that's a no-op bug)
    radio_->setCCMode(1);
    radio_->setPktFormat(0);
    radio_->SetTx();
    if (gdo0 >= 0)
        pinMode(gdo0, INPUT);

    LOGI(kTag, "record: %u bytes captured", static_cast<unsigned>(recordedLen_));
}

void Cc1101::workerReplay() {
    if (radio_ == nullptr || recordBuf_ == nullptr || recordedLen_ == 0)
        return;
    // Switch to async-serial TX
    radio_->setCCMode(0);
    radio_->setPktFormat(3);
    radio_->SetTx();
    const int8_t gdo0 = hal::g_board.pin(hal::PinRole::Cc1101Gdo0);
    if (gdo0 >= 0)
        pinMode(gdo0, OUTPUT);

    const int samplePin = (gdo0 >= 0) ? gdo0 : 2;
    for (size_t i = 0; i < recordedLen_ && running_; ++i) {
        const uint8_t b = recordBuf_[i];
        for (int8_t j = 7; j >= 0 && running_; --j) {
            digitalWrite(samplePin, bitRead(b, j));
            delayMicroseconds(samplingUs_);
        }
        if ((i & 0x1F) == 0) {
            vTaskDelay(pdMS_TO_TICKS(0));
            if (millis() >= opDeadlineMs_)
                break;
        }
    }
    replaying_ = false;

    // Restore
    radio_->setCCMode(1);
    radio_->setPktFormat(0);
    radio_->SetTx();
    if (gdo0 >= 0)
        pinMode(gdo0, INPUT);
}

// ---------------------------------------------------------------------------
// Cc1101Manager
// ---------------------------------------------------------------------------
void Cc1101Manager::setup() {
    if (radio_ != nullptr) {
        teardown();
    }
    radio_ = new Cc1101();
    if (radio_ == nullptr) {
        LOGE(kTag, "manager: Cc1101 alloc failed");
        return;
    }

    const int8_t sck = hal::g_board.pin(hal::PinRole::Cc1101Sck);
    const int8_t miso = hal::g_board.pin(hal::PinRole::Cc1101Miso);
    const int8_t mosi = hal::g_board.pin(hal::PinRole::Cc1101Mosi);
    const int8_t csn = hal::g_board.pin(hal::PinRole::Cc1101Csn0);
    const int8_t gdo0 = hal::g_board.pin(hal::PinRole::Cc1101Gdo0);
    const int8_t gdo2 = hal::g_board.pin(hal::PinRole::Cc1101Gdo2);

    if (!radio_->setup(sck, miso, mosi, csn, gdo0, gdo2)) {
        LOGW(kTag, "manager: chip not responding");
        delete radio_;
        radio_ = nullptr;
    }
}

void Cc1101Manager::teardown() {
    if (radio_ != nullptr) {
        delete radio_;
        radio_ = nullptr;
    }
    LOGD(kTag, "manager: torn down");
}

}  // namespace phm::radio
