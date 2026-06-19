/**
 * @file Nrf24.cpp
 * @brief nRF24L01+ driver wrapper — implementation
 *
 * All jamming primitives build on top of the third-party `RF24`
 * library; this file adds:
 *
 *   - A shared `SPIClass` instance on the user SPI bus (HSPI on
 *     classic ESP32, bus 1 on ESP32-S2/S3).
 *   - A `startConstCarrier()` wrapper that calls
 *     `RF24::startConstCarrier()` (the library already implements
 *     DESIGN §10.1's `CONT_WAVE | PLL_LOCK` trick internally — see
 *     `RF24.cpp:1997`).
 *   - A `startNoAckFlood()` that primes the radio for unacknowledged
 *     2-byte packet flood (BLE advertising style).
 *   - A worker task that drives all five radios through a channel
 *     sweep at priority 3 on Core 0.
 *
 * The worker task is started by `Nrf24Manager::jamSweep()` and torn
 * down by `Nrf24Manager::stopAll()` / `teardown()`.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "radio/Nrf24.h"

#include "core/Config.h"
#include "hal/Board.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace phm::radio {

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------
static constexpr const char* kTag = "nrf24";
static constexpr UBaseType_t kTaskPrio = 3;
static constexpr uint32_t kTaskStack = 4096;
static constexpr BaseType_t kTaskCore = 0;
static constexpr uint32_t kHopDelayUs = 1300;    ///< ~1.3 ms PLL re-lock
static constexpr uint32_t kSpiClock = 10000000;  ///< 10 MHz SPI clock
static constexpr uint32_t kJamPayload = 2;       ///< NOACK flood payload size

// Singleton
Nrf24Manager g_nrf24;

// ---------------------------------------------------------------------------
// Nrf24 — single module
// ---------------------------------------------------------------------------
Nrf24::Nrf24(uint8_t id, const Nrf24Pins& pins, uint8_t paLevel)
    : id_(id),
      pins_(pins),
      paLevel_(paLevel > 3 ? 3 : paLevel),
      dataRate_(1),  // 2 Mbps default for jamming
      channel_(76),  // 2.476 GHz, popular drone band
      running_(false),
      connected_(false),
      radio_(nullptr),
      spi_(nullptr) {}

Nrf24::~Nrf24() {
    if (radio_ != nullptr) {
        if (running_) {
            stop();
        }
        delete radio_;
        radio_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
bool Nrf24::setup(SPIClass* spi) {
    if (pins_.ce < 0 || pins_.csn < 0) {
        LOGD(kTag, "nrf24[%u]: pins not wired, skipping", id_);
        return false;
    }
    if (spi == nullptr) {
        LOGE(kTag, "nrf24[%u]: null SPI bus", id_);
        return false;
    }

    spi_ = spi;

    // The library takes its own reference to (ce, csn); heap-allocating
    // a `RF24` keeps the rest of the object POD-friendly and lets us
    // probe `isChipConnected()` before committing.
    radio_ = new RF24(static_cast<uint8_t>(pins_.ce), static_cast<uint8_t>(pins_.csn), kSpiClock);
    if (radio_ == nullptr) {
        LOGE(kTag, "nrf24[%u]: RF24 alloc failed", id_);
        return false;
    }

    if (!radio_->begin(spi)) {
        LOGW(kTag, "nrf24[%u]: chip not responding (CE=%d CSN=%d)", id_, static_cast<int>(pins_.ce),
             static_cast<int>(pins_.csn));
        connected_ = false;
        delete radio_;
        radio_ = nullptr;
        return false;
    }
    connected_ = true;

    // Default configuration: ready for jamming
    radio_->setAutoAck(false);
    radio_->setRetries(0, 0);
    radio_->setPALevel(static_cast<rf24_pa_dbm_e>(paLevel_), /*lnaEnable=*/true);
    radio_->setDataRate(static_cast<rf24_datarate_e>(dataRate_));
    radio_->setCRCLength(RF24_CRC_DISABLED);
    radio_->disableAckPayload();
    radio_->disableDynamicPayloads();
    radio_->setPayloadSize(kJamPayload);  ///< 2-byte NOACK
    radio_->setAddressWidth(2);
    radio_->setChannel(channel_);
    radio_->stopListening();  ///< we only ever TX from this driver

    LOGD(kTag, "nrf24[%u]: ready (CE=%d CSN=%d ch=%u pa=%u)", id_, static_cast<int>(pins_.ce),
         static_cast<int>(pins_.csn), static_cast<unsigned>(channel_), static_cast<unsigned>(paLevel_));
    return true;
}

// ---------------------------------------------------------------------------
void Nrf24::setChannel(uint8_t ch) {
    if (ch > PHM_NRF24_CHANNEL_MAX) {
        ch = PHM_NRF24_CHANNEL_MAX;
    }
    channel_ = ch;
    if (radio_ != nullptr) {
        radio_->setChannel(ch);
    }
}

// ---------------------------------------------------------------------------
void Nrf24::setPALevel(uint8_t level) {
    if (level > 3)
        level = 3;
    paLevel_ = level;
    if (radio_ != nullptr) {
        radio_->setPALevel(static_cast<rf24_pa_dbm_e>(level), /*lnaEnable=*/true);
    }
}

// ---------------------------------------------------------------------------
void Nrf24::setDataRate(uint8_t rate) {
    if (rate > 2)
        rate = 2;
    dataRate_ = rate;
    if (radio_ != nullptr) {
        radio_->setDataRate(static_cast<rf24_datarate_e>(rate));
    }
}

// ---------------------------------------------------------------------------
bool Nrf24::transmit(const uint8_t* buf, uint8_t len) {
    if (radio_ == nullptr || !connected_)
        return false;
    if (running_) {
        // Caller asked for a one-shot TX while a carrier/flood is up;
        // stop the carrier first so the new packet can go out.
        stop();
    }
    return radio_->write(buf, len, /*multicast=*/false);
}

// ---------------------------------------------------------------------------
bool Nrf24::startConstCarrier(uint8_t channel) {
    if (radio_ == nullptr || !connected_)
        return false;
    setChannel(channel);
    radio_->startConstCarrier(static_cast<rf24_pa_dbm_e>(paLevel_), channel);
    running_ = true;
    LOGD(kTag, "nrf24[%u]: const-carrier on ch %u", id_, static_cast<unsigned>(channel));
    return true;
}

// ---------------------------------------------------------------------------
bool Nrf24::startNoAckFlood(uint8_t channel) {
    if (radio_ == nullptr || !connected_)
        return false;
    // Re-prime the chip for unacknowledged 2-byte packet flood.
    radio_->setAutoAck(false);
    radio_->setCRCLength(RF24_CRC_DISABLED);
    radio_->setAddressWidth(2);
    radio_->setPayloadSize(kJamPayload);
    setChannel(channel);
    radio_->stopListening();
    running_ = true;
    LOGD(kTag, "nrf24[%u]: NOACK flood primed on ch %u", id_, static_cast<unsigned>(channel));
    return true;
}

// ---------------------------------------------------------------------------
void Nrf24::stop() {
    if (radio_ == nullptr)
        return;
    if (running_) {
        // The library's `stopConstCarrier()` clears CONT_WAVE | PLL_LOCK
        // and drops CE. Then we power-down to keep the amp cool.
        radio_->stopConstCarrier();
        radio_->powerDown();
    }
    running_ = false;
    LOGD(kTag, "nrf24[%u]: stopped", id_);
}

// ---------------------------------------------------------------------------
int Nrf24::readRssi() const {
    if (radio_ == nullptr)
        return -120;
    // `RF24::testRPD()` is the cheapest "is there any energy here" check;
    // a proper RSSI read requires the chip to be in RX which we don't do
    // for the jamming driver — return a sentinel indicating "unknown".
    return radio_->testRPD() ? -60 : -100;
}

int8_t Nrf24::scanRssi(uint8_t ch, uint16_t dwellUs) {
    if (radio_ == nullptr || ch > 125)
        return -128;
    // Save current channel
    const uint8_t prev = radio_->getChannel();

    // Hop to channel, switch to RX, dwell, read
    radio_->setChannel(ch);
    radio_->startListening();
    delayMicroseconds(dwellUs);

    int8_t rssi = -128;
    if (radio_->testRPD()) {
        // RPD trips at about -64 dBm; treat as "energy present" at -60.
        // We don't have a real RSSI register here without a full RX cycle
        // for a known packet; the value is still useful for the waterfall
        // heatmap (it lights up wherever energy is present).
        rssi = -60;
    } else {
        rssi = -100;
    }

    // Restore
    radio_->stopListening();
    radio_->setChannel(prev);
    return rssi;
}

// ---------------------------------------------------------------------------
// Nrf24 — private helpers
// ---------------------------------------------------------------------------
void Nrf24::writeContWaveReg(bool on) {
    // Exposed as a no-op stub: the library's startConstCarrier/stopConstCarrier
    // perform the actual RF_SETUP read-modify-write. We keep this method in
    // case a future caller needs to flip the bits without going through
    // the chip's full constant-carrier sequence.
    (void)on;
}

void Nrf24::powerUp() {
    if (radio_ != nullptr)
        radio_->powerUp();
}

void Nrf24::powerDown() {
    if (radio_ != nullptr)
        radio_->powerDown();
}

// ---------------------------------------------------------------------------
// Nrf24Manager
// ---------------------------------------------------------------------------
void Nrf24Manager::setup(uint8_t count, const Nrf24Pins* pins) {
    if (count > PHM_MAX_NRF24_RADIOS)
        count = PHM_MAX_NRF24_RADIOS;

    // Create the shared SPI bus. On classic ESP32 HSPI = 2; on
    // ESP32-S2/S3 the user SPI bus is 1 (the chip's "HSPI").
    // `HSPI` is defined by the core to the right number on each
    // variant, so we use the macro for portability.
    if (spi_ == nullptr) {
        spi_ = new SPIClass(HSPI);
        if (spi_ != nullptr) {
            spi_->begin();
            spi_->setFrequency(kSpiClock);
            spi_->setBitOrder(MSBFIRST);
            spi_->setDataMode(SPI_MODE0);
        } else {
            LOGE(kTag, "SPIClass alloc failed");
            return;
        }
    }

    count_ = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (radios_[i] != nullptr) {
            delete radios_[i];
            radios_[i] = nullptr;
        }
        const Nrf24Pins& p = (pins != nullptr) ? pins[i] : Nrf24Pins{-1, -1};
        radios_[i] = new Nrf24(i, p, /*paLevel=*/3);
        if (radios_[i] != nullptr && radios_[i]->setup(spi_)) {
            count_++;
        }
    }

    LOGI(kTag, "manager: %u/%u nRF24 modules wired", count_, static_cast<unsigned>(count));
}

void Nrf24Manager::teardown() {
    stopAll();
    for (uint8_t i = 0; i < PHM_MAX_NRF24_RADIOS; ++i) {
        if (radios_[i] != nullptr) {
            delete radios_[i];
            radios_[i] = nullptr;
        }
    }
    if (spi_ != nullptr) {
        spi_->end();
        delete spi_;
        spi_ = nullptr;
    }
    count_ = 0;
    LOGD(kTag, "manager: torn down");
}

// ---------------------------------------------------------------------------
bool Nrf24Manager::jamSweep(uint8_t startCh, uint8_t stopCh, uint8_t method) {
    if (count_ == 0) {
        LOGW(kTag, "jamSweep: no radios wired");
        return false;
    }
    if (running_) {
        LOGW(kTag, "jamSweep: already running; stopping first");
        stopAll();
    }

    if (startCh > stopCh) {
        const uint8_t t = startCh;
        startCh = stopCh;
        stopCh = t;
    }
    if (stopCh > PHM_NRF24_CHANNEL_MAX)
        stopCh = PHM_NRF24_CHANNEL_MAX;

    startCh_ = startCh;
    stopCh_ = stopCh;
    method_ = method & 0x03;  ///< low 2 bits = Method
    separate_ = (method & kNrf24SweepSeparate) != 0;
    noAck_ = (method_ == static_cast<uint8_t>(Nrf24SweepMethod::NoAck));
    running_ = true;

    const BaseType_t ok = xTaskCreatePinnedToCore(&Nrf24Manager::workerEntry, "nrf24sweep", kTaskStack, this, kTaskPrio,
                                                  reinterpret_cast<TaskHandle_t*>(&workerTask_), kTaskCore);
    if (ok != pdPASS || workerTask_ == nullptr) {
        running_ = false;
        workerTask_ = nullptr;
        LOGE(kTag, "jamSweep: xTaskCreatePinnedToCore failed");
        return false;
    }

    LOGI(kTag, "jamSweep: ch %u..%u method=%u %s", static_cast<unsigned>(startCh_), static_cast<unsigned>(stopCh_),
         static_cast<unsigned>(method_), separate_ ? "separate" : "together");
    return true;
}

// ---------------------------------------------------------------------------
void Nrf24Manager::stopAll() {
    if (!running_ && workerTask_ == nullptr) {
        return;
    }
    running_ = false;

    if (workerTask_ != nullptr) {
        auto* handle = static_cast<TaskHandle_t>(workerTask_);
        for (int i = 0; i < 50 && eTaskGetState(handle) != eDeleted; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (eTaskGetState(handle) != eDeleted) {
            LOGW(kTag, "stopAll: worker did not exit in 500 ms; force-deleting");
            vTaskDelete(handle);
        }
        workerTask_ = nullptr;
    }

    for (uint8_t i = 0; i < PHM_MAX_NRF24_RADIOS; ++i) {
        if (radios_[i] != nullptr) {
            radios_[i]->stop();
        }
    }
    LOGD(kTag, "stopAll: done");
}

// ---------------------------------------------------------------------------
void Nrf24Manager::applySweepStep(uint8_t ch) {
    if (count_ == 0)
        return;

    if (noAck_) {
        // NOACK flood: prime each radio for unacknowledged packet TX,
        // then push a 2-byte buffer as fast as we can.
        for (uint8_t i = 0; i < PHM_MAX_NRF24_RADIOS; ++i) {
            Nrf24* r = radios_[i];
            if (r == nullptr)
                continue;
            if (separate_) {
                ch = static_cast<uint8_t>(ch + i);  ///< rough round-robin
                if (ch > stopCh_)
                    ch = startCh_;
            }
            r->startNoAckFlood(ch);
        }
        // The actual flood loop happens in workerLoop(); this just primes
        // the registers.
    } else {
        for (uint8_t i = 0; i < PHM_MAX_NRF24_RADIOS; ++i) {
            Nrf24* r = radios_[i];
            if (r == nullptr)
                continue;
            if (separate_) {
                ch = static_cast<uint8_t>(ch + i);
                if (ch > stopCh_)
                    ch = startCh_;
            }
            r->startConstCarrier(ch);
        }
    }
}

// ---------------------------------------------------------------------------
void Nrf24Manager::workerEntry(void* arg) {
    auto* self = static_cast<Nrf24Manager*>(arg);
    if (self != nullptr) {
        self->workerLoop();
    }
    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
void Nrf24Manager::workerLoop() {
    LOGD(kTag, "worker: entered");

    while (running_) {
        switch (method_) {
        case static_cast<uint8_t>(Nrf24SweepMethod::List): {
            for (uint8_t ch = startCh_; ch <= stopCh_ && running_; ++ch) {
                applySweepStep(ch);
                delayMicroseconds(kHopDelayUs);
            }
            break;
        }
        case static_cast<uint8_t>(Nrf24SweepMethod::Random): {
            const uint8_t ch = startCh_ + static_cast<uint8_t>(esp_random() % (stopCh_ - startCh_ + 1));
            applySweepStep(ch);
            delayMicroseconds(kHopDelayUs);
            break;
        }
        case static_cast<uint8_t>(Nrf24SweepMethod::Brute): {
            for (uint8_t ch = startCh_; ch <= stopCh_ && running_; ++ch) {
                applySweepStep(ch);
                delayMicroseconds(kHopDelayUs);
            }
            break;
        }
        case static_cast<uint8_t>(Nrf24SweepMethod::NoAck): {
            // NOACK flood: cycle through the range, on each channel
            // push 2-byte NOACK packets in a tight loop until either
            // the sweep step time elapses or `running_` flips false.
            for (uint8_t ch = startCh_; ch <= stopCh_ && running_; ++ch) {
                applySweepStep(ch);
                for (uint8_t t = 0; t < 8 && running_; ++t) {
                    for (uint8_t i = 0; i < PHM_MAX_NRF24_RADIOS; ++i) {
                        Nrf24* r = radios_[i];
                        if (r == nullptr)
                            continue;
                        const uint8_t jam_buf[kJamPayload] = {0xFF, 0xFF};
                        r->transmit(jam_buf, kJamPayload);
                    }
                }
                delayMicroseconds(kHopDelayUs);
            }
            break;
        }
        default: {
            // Unknown method — idle so we don't spin
            vTaskDelay(pdMS_TO_TICKS(10));
            break;
        }
        }
        vTaskDelay(pdMS_TO_TICKS(0));  ///< yield, allow stopAll() to be observed
    }

    // Tear down all carriers so the radios are quiet on exit
    for (uint8_t i = 0; i < PHM_MAX_NRF24_RADIOS; ++i) {
        if (radios_[i] != nullptr)
            radios_[i]->stop();
    }
    LOGD(kTag, "worker: exited");
}

}  // namespace phm::radio
