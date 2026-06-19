/**
 * @file Cc1101.h
 * @brief CC1101 sub-GHz driver wrapper for PhantomRF
 *
 * Wraps the third-party `ELECHOUSE_CC1101` class (the class that
 * `LSatan/SmartRC-CC1101-Driver-Lib@^3.0.2` actually exports; the
 * README calls it `SmartRC_CC1101`).
 *
 * The wrapper exists for the same reasons as `Nrf24`:
 *
 *   1. **Lifetime control.** The library exposes a process-wide
 *      `ELECHOUSE_cc1101` global; we hide it behind a
 *      heap-allocated instance so the firmware can be reasoned about
 *      (one chip == one `Cc1101` object).
 *   2. **Async-serial mode.** DESIGN §10.2 needs raw OOK/FSK capture
 *      and replay. The library can switch to async-serial
 *      (`setCCMode(0)` + `setPktFormat(3)`) and exposes `SetRx()` /
 *      `SetTx()` for the bit-bang on GDO0 — we wrap those plus the
 *      record/replay buffer management.
 *   3. **Bug fixes from upstream.** The reference W0rthlessS0ul
 *      CC1101_jammer project calls `setCCMode(2)` after
 *      record/replay "to restore normal mode" — that is silently
 *      interpreted as `setCCMode(false)` = async-serial mode. We
 *      explicitly use `setCCMode(1)` to mean "normal packet mode".
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

// Forward declaration of the third-party CC1101 driver class. The real
// definition is in <SmartRC_CC1101.h> (the V3.0.2 library renamed the
// class from `ELECHOUSE_CC1101` to `SmartRC_CC1101`; the old name is
// kept as a typedef for backward compatibility). `ELECHOUSE_CC1101_SRC_DRV.h`
// is a backward-compat shim that re-includes the new header. Pulling
// the real header in here would also pull in <SPI.h>, so we keep the
// forward declaration and include the full header only in `Cc1101.cpp`.
class SmartRC_CC1101;

// TaskHandle_t is a FreeRTOS type. We store it as a void* in the
// class to avoid a conflicting typedef in the header (FreeRTOS
// already defines `TaskHandle_t` as `void*` when <freertos/task.h>
// is included). The .cpp casts the void* to the real type.

namespace phm::radio {

/**
 * @brief CC1101 operating mode
 *
 * The library's `setCCMode(bool)` only branches on `true` vs `false`,
 * so we wrap the two meaningful values:
 *
 *   - `AsyncSerial` (0 / false): bypass the packet handler so the
 *     raw demodulated bitstream appears on GDO0 (RX) or you can
 *     bit-bang GDO0 to drive TX. Used for record / replay.
 *   - `NormalPacket` (1 / true): standard packet mode with preamble,
 *     sync word, and length byte. Used for normal packet TX and
 *     for jamming (which pushes junk data via `SendData`).
 *
 * The W0rthlessS0ul `rec_play.cpp` mistakenly called `setCCMode(2)`
 * after playback to "restore" the chip. The library treats anything
 * other than `1` as async-serial mode, so that line was a no-op that
 * left the chip in async mode. We use `NormalPacket` (== 1) explicitly.
 */
enum class CcMode : uint8_t {
    AsyncSerial = 0,   ///< Raw GDO0 bitstream (record / replay)
    NormalPacket = 1,  ///< Standard packet handler (jamming / packet TX)
};

/**
 * @brief Modulation format
 *
 * Matches the `setModulation(byte)` codes in the library:
 *   0 = 2-FSK
 *   1 = GFSK
 *   2 = ASK / OOK  (the default for keyfob jamming)
 *   3 = 4-FSK
 *   4 = MSK
 */
enum class CcModulation : uint8_t {
    FSK2 = 0,
    GFSK = 1,
    ASK_OOK = 2,
    FSK4 = 3,
    MSK = 4,
};

/**
 * @brief One CC1101 module on the sub-GHz SPI bus
 *
 * M0 hardware has one CC1101; M1 has a second one (DESIGN §4.3).
 * Both share the same SPI bus but have distinct CSN pins and a
 * distinct GDO0 / GDO2 pair. The wrapper handles a single chip
 * — the manager drives the per-chip instances.
 */
class Cc1101 {
public:
    /// Default constructor — call `setup()` before any other method
    Cc1101();

    /// Destructor — powers the chip down and frees the buffer
    ~Cc1101();

    // Non-copyable
    Cc1101(const Cc1101&) = delete;
    Cc1101& operator=(const Cc1101&) = delete;

    // ---- Lifecycle -------------------------------------------------------

    /**
     * @brief Bring up the SPI device and configure it for jamming
     *
     * Calls the library's `setSpiPin()` + `setGDO()` + `Init()`
     * sequence (REQUIRED ordering in V3.0.2) and then applies the
     * W0rthlessS0ul default configuration for sub-GHz jamming
     * (9.6 kbps, 200 kHz channel spacing, 812 kHz RX BW, +12 dBm,
     * ASK/OOK modulation, sync word 0xD391, etc.).
     *
     * @param sck  SPI clock GPIO
     * @param miso SPI MISO GPIO
     * @param mosi SPI MOSI GPIO
     * @param csn  SPI chip-select GPIO
     * @param gdo0 GDO0 GPIO (used as TX-data in async mode)
     * @param gdo2 GDO2 GPIO (used as RX-data in async mode)
     * @return true if the chip responded with a non-zero VERSION register
     */
    bool setup(int8_t sck, int8_t miso, int8_t mosi, int8_t csn, int8_t gdo0, int8_t gdo2);

    /// True if the chip responded on SPI during `setup()`
    bool isConnected() const { return running_; }

    // ---- Frequency / modulation ----------------------------------------

    /**
     * @brief Set the centre frequency in MHz
     *
     * Wraps the library's `setMHZ()` which writes the FREQ2/1/0
     * registers and triggers a PLL re-calibration (slow — ~1 ms).
     *
     * @param mhz Target frequency, e.g. 315.0, 433.92, 868.0, 915.0
     */
    void setFrequency(float mhz);

    /// Currently-configured centre frequency (MHz)
    float frequency() const { return currentFreq_; }

    /// Read the RSSI register (dBm) at the currently-tuned frequency.
    /// The CC1101 returns the current RX signal strength; if RX is
    /// not active the value is meaningless. Caller must put the chip
    /// in RX first via `setMode(CcMode::Rx)`.
    int8_t getRssiDbm() const;

    /// Set modulation format
    void setModulation(CcModulation mod);

    // ---- Mode -----------------------------------------------------------

    /// Switch between async-serial and normal-packet modes
    void setMode(CcMode mode);

    // ---- TX power / data rate / bandwidth ------------------------------

    /// Set TX power in dBm (typically 0..12; 10..12 dBm only with PA table)
    void setTxPower(int8_t dbm);

    /// Currently-configured TX power (dBm)
    int8_t txPower() const { return currentPower_; }

    /// Set data rate in kbps (typical 1.2..600)
    void setDataRate(float kbps);

    /// Currently-configured data rate (kbps)
    float dataRate() const { return currentDataRate_; }

    /// Set RX filter bandwidth in kHz
    void setRxBandwidth(float khz);

    // ---- Standard packet TX -------------------------------------------

    /**
     * @brief Transmit a single packet (blocking)
     *
     * Calls the library's `SendData()`. For jamming, use the
     * `startSpot/Range/Hopper/KeyfobJam()` methods instead.
     */
    bool transmit(const uint8_t* data, uint8_t len);

    // ---- Jamming primitives -------------------------------------------

    /**
     * @brief Jam a single frequency with a junk-data flood
     *
     * The library's `SendData` in normal-packet mode pushes packets
     * as fast as the chip can transmit. We use a heap-allocated
     * "jam data" buffer that is **never** zeroed (the W0rthlessS0ul
     * bug uses a global that is re-allocated on every attack — we
     * allocate once in `setup()` and reuse it).
     *
     * @param mhz          Centre frequency in MHz
     * @param payloadSize  Bytes per `SendData` call (default 60)
     * @return true on success
     */
    bool startSpotJam(float mhz, uint8_t payloadSize = 60);

    /**
     * @brief Jam a frequency range by sweeping start..stop in `stepMhz` steps
     *
     * Spawns the worker task pinned to Core 0 at priority 3 (DESIGN §3.3).
     */
    bool startRangeJam(float start, float stop, float stepMhz, uint8_t payloadSize = 60);

    /**
     * @brief Jam a user-supplied list of frequencies
     */
    bool startHopperJam(const float* freqs, uint8_t count, uint8_t payloadSize = 60);

    /**
     * @brief Jam one of the predefined keyfob bands
     *
     * @param index  0..KEYFOB_COUNT-1 — see `Nrf24Jammer::KEYFOB_FREQS`
     */
    bool startKeyfobJam(uint8_t index, uint8_t payloadSize = 60);

    // ---- Recording (async-serial RX) -----------------------------------

    /**
     * @brief Record raw OOK/FSK into the PSRAM buffer
     *
     * Switches the chip to async-serial RX, then bit-bangs GDO0
     * with `delayMicroseconds(samplingUs_)` per bit. Captures up to
     * `durationMs` of traffic (or until `stop()` is called).
     *
     * @param durationMs  Cap on capture duration in milliseconds
     */
    bool startRecord(uint16_t durationMs = 30000);

    /// Stop an in-progress recording
    void stopRecord();

    /// True while the worker is recording
    bool isRecording() const { return recording_; }

    /// Progress 0..100
    uint16_t recordProgress() const;

    // ---- Replay (async-serial TX) -------------------------------------

    /**
     * @brief Replay a previously-captured buffer
     *
     * @param durationMs  Cap on playback duration
     */
    bool startReplay(uint16_t durationMs = 30000);

    void stopReplay();
    bool isReplaying() const { return replaying_; }

    // ---- Buffer access ------------------------------------------------

    /// Direct read access to the record buffer (raw bytes)
    const uint8_t* recordBuffer() const { return recordBuf_; }

    /// Total allocated size of the record buffer
    size_t recordBufferSize() const { return recordBufSize_; }

    /// Number of bytes actually captured in the most recent record
    size_t recordedLength() const { return recordedLen_; }

    // ---- Sampling interval -------------------------------------------

    /// Time per bit in microseconds (1 / bitrate). Default matches 9.6 kbps.
    void setSamplingInterval(uint16_t us) { samplingUs_ = us; }
    uint16_t samplingInterval() const { return samplingUs_; }

    // ---- Stop everything ---------------------------------------------

    /// Stop any in-progress jam / record / replay
    void stop();

    // ---- Identity / introspection -------------------------------------

    /// True while any worker is running
    bool isRunning() const { return task_ != nullptr; }

private:
    /// Worker entry — dispatches to spot/range/hopper/keyfob or record/replay
    void workerLoop();

    /// FreeRTOS trampoline
    static void taskEntry(void* arg);

    /// Per-target worker body
    void workerSpot();
    void workerRange();
    void workerHopper();
    void workerKeyfob();
    void workerRecord();
    void workerReplay();

    /// Helpers
    void allocateRecordBuffer(size_t size);
    void freeRecordBuffer();
    void applyJammingDefaults();

    /// Common path for the four `startXxxJam` methods — sets `running_`
    /// and creates the worker task with the configured `op_`.
    bool launchWorker(const char* taskName);

    // ---- State ---------------------------------------------------------
    bool running_ = false;
    bool recording_ = false;
    bool replaying_ = false;
    float currentFreq_ = 433.92f;
    int8_t currentPower_ = 12;
    CcMode currentMode_ = CcMode::NormalPacket;
    CcModulation currentMod_ = CcModulation::ASK_OOK;
    float currentDataRate_ = 9.6f;
    uint16_t samplingUs_ = 100;  ///< ~10 kbps default

    // Heap-allocated junk data for jamming (never zeroed, per upstream)
    uint8_t* jamData_ = nullptr;
    size_t jamDataSize_ = 0;

    // Recording buffer (PSRAM when available)
    uint8_t* recordBuf_ = nullptr;
    size_t recordBufSize_ = 0;
    size_t recordedLen_ = 0;

    // Sweep / record parameters for the worker
    enum class Op : uint8_t { None, Spot, Range, Hopper, Keyfob, Record, Replay };
    Op op_ = Op::None;
    float opStart_ = 0.0f;
    float opStop_ = 0.0f;
    float opStep_ = 0.0f;
    const float* opFreqs_ = nullptr;
    uint8_t opFreqCount_ = 0;
    uint8_t opIndex_ = 0;
    uint8_t opPayload_ = 60;
    uint32_t opDeadlineMs_ = 0;

    void* task_ = nullptr;

    // The third-party driver (single instance, owned by us)
    SmartRC_CC1101* radio_ = nullptr;
};

/**
 * @brief Owns the CC1101 driver(s) and the worker task
 *
 * PhantomRF's M0 hardware has one CC1101, M1 has two. The manager
 * always brings up the first one; the second slot is reserved for
 * future M1 builds.
 */
class Cc1101Manager {
public:
    /// Bring up the (single) CC1101 module on the board
    void setup();

    /// Tear down the module and stop the worker
    void teardown();

    /// Pointer to the (single) CC1101, or nullptr if not wired
    Cc1101* radio() { return radio_; }
    const Cc1101* radio() const { return radio_; }

    /// True if the chip is wired and responded during setup
    bool isPresent() const { return radio_ != nullptr && radio_->isConnected(); }

private:
    Cc1101* radio_ = nullptr;
};

/// Singleton, defined in Cc1101.cpp
extern Cc1101Manager g_cc1101;

}  // namespace phm::radio
