/**
 * @file Nrf24.h
 * @brief nRF24L01+ driver wrapper for PhantomRF
 *
 * Thin C++ wrapper around the third-party `RF24` library
 * (nrf24/RF24@^1.6.1). The wrapper exists for three reasons:
 *
 *   1. **Lifetime control.** The `RF24` class is heavy (>= 100 B per
 *      instance plus SPI bookkeeping). Instantiating one per module and
 *      keeping them on the heap lets us know the exact set of radios
 *      the firmware has, even if NVS claims 5 modules and only 2 are
 *      wired on the PCB.
 *   2. **Constant-carrier / NOACK primitives.** DESIGN §10.1
 *      describes a manual `CONT_WAVE | PLL_LOCK` trick to emit an
 *      unmodulated carrier. The library exposes this as
 *      `RF24::startConstCarrier()` — we wrap it so callers don't have
 *      to know about the bit fields. Channel hopping is done by
 *      `setChannel()` after the carrier is up.
 *   3. **Sweep workers.** `Nrf24Manager` owns a single FreeRTOS task
 *      that drives all five radios through a channel sweep. The
 *      `Nrf24Jammer` module configures the sweep and waits for it to
 *      finish; the manager handles the actual register pokes.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

// Forward declaration of the third-party RF24 class so this header
// doesn't drag in <SPI.h>, <RF24_config.h>, etc. The real definition
// lives in `RF24.h` and is included only by `Nrf24.cpp`.
class RF24;

// Forward declaration of the Arduino SPI class. Defined in <SPI.h> which
// is included by `Nrf24.cpp`; the forward decl is enough for the
// `SPIClass*` members below.
class SPIClass;

// TaskHandle_t is a FreeRTOS type. We store it as a void* in the
// class to avoid a conflicting typedef in the header (FreeRTOS
// already defines `TaskHandle_t` as `void*` when <freertos/task.h>
// is included). The .cpp casts the void* to the real type.
#include <stddef.h>

namespace phm::radio {

/**
 * @brief Pin assignment for a single nRF24 module
 *
 * `ce` and `csn` are GPIO numbers (resolved from `phm::hal::Board`),
 * or -1 if the module is not wired on the current board. A module
 * with both pins == -1 is silently skipped by `Nrf24::setup()`.
 */
struct Nrf24Pins {
    int8_t ce;   ///< Chip Enable pin, or -1 if not present
    int8_t csn;  ///< Chip Select (active low), or -1 if not present
};

/**
 * @brief One nRF24L01(+) module on the shared HSPI bus
 *
 * A separate `RF24` instance backs every `Nrf24`. They all share the
 * same SPI bus but have distinct `ce` / `csn` pins, so the library
 * handles the bus arbitration for us.
 *
 * The class is **non-copyable** because the underlying `RF24` holds
 * raw SPI state.
 */
class Nrf24 {
public:
    /**
     * @brief Construct a driver for one module
     *
     * @param id       Stable identifier (0..PHM_MAX_NRF24_RADIOS-1) used
     *                 in log messages and event sources.
     * @param pins     CE / CSN pin numbers from the board table.
     * @param paLevel  Initial PA level (0=MIN, 1=LOW, 2=HIGH, 3=MAX).
     */
    Nrf24(uint8_t id, const Nrf24Pins& pins, uint8_t paLevel = 3);

    /// Destructor — frees the heap-allocated RF24 and powers down
    ~Nrf24();

    // Non-copyable: SPI state is per-instance
    Nrf24(const Nrf24&) = delete;
    Nrf24& operator=(const Nrf24&) = delete;

    // ---- Lifecycle -------------------------------------------------------

    /**
     * @brief Bring up the SPI device and probe it
     *
     * Performs `RF24::begin()` (with the shared `SPIClass*` passed in
     * by the manager) and a `setPALevel` / `setDataRate` /
     * `setAutoAck(false)` / `setCRCLength(DISABLED)` initialisation
     * pass so every radio is in a known, jamming-friendly state.
     *
     * @param spi  Pointer to a `SPIClass` owned by the manager. Must
     *             live for as long as the radio.
     * @return true on successful `begin()` (chip responded on SPI)
     */
    bool setup(SPIClass* spi);

    /// True if the chip responded on the SPI bus during `setup()`
    bool isConnected() const { return connected_; }

    // ---- Channel control -------------------------------------------------

    /// Pick the 2.4 GHz channel (0..125). Clamped internally.
    void setChannel(uint8_t ch);

    /// Currently-configured channel
    uint8_t channel() const { return channel_; }

    // ---- PA level --------------------------------------------------------

    /// Set power amplifier level: 0=MIN, 1=LOW, 2=HIGH, 3=MAX
    void setPALevel(uint8_t level);

    /// Currently-configured PA level (one of 0..3)
    uint8_t paLevel() const { return paLevel_; }

    // ---- Data rate -------------------------------------------------------

    /// Set data rate: 0=1 Mbps, 1=2 Mbps, 2=250 kbps
    void setDataRate(uint8_t rate);

    /// Currently-configured data-rate code (one of 0..2)
    uint8_t dataRate() const { return dataRate_; }

    // ---- Normal packet TX -----------------------------------------------

    /**
     * @brief Transmit a single packet (blocking)
     *
     * Sends via `RF24::write()`. Useful for the legitimate / debug
     * side of the project (telemetry, RC). For jamming use
     * `startConstCarrier()` or `startNoAckFlood()` instead.
     *
     * @return true if the chip reported success
     */
    bool transmit(const uint8_t* buf, uint8_t len);

    // ---- Jamming primitives ---------------------------------------------

    /**
     * @brief Emit a constant unmodulated carrier on `channel`
     *
     * Implements DESIGN §10.1. Internally sets `CONT_WAVE` and
     * `PLL_LOCK` in `RF_SETUP`, then pulses CE high. To hop channels
     * while the carrier is up, just call `setChannel()` — the PLL
     * auto-relocks in ~120 µs.
     *
     * @param channel Channel 0..125
     * @return true on success
     */
    bool startConstCarrier(uint8_t channel);

    /**
     * @brief Start streaming 2-byte NOACK packets (BLE-adv style)
     *
     * Configures the radio for unacknowledged, no-CRC, 2-byte
     * address-width packet flood and lets the manager's worker drive
     * `transmit()` in a tight loop. The actual flooding happens in
     * `Nrf24Manager::jamSweep(..., method=NoAck, ...)`; this method
     * only primes the registers.
     *
     * @param channel Channel 0..125
     * @return true on success
     */
    bool startNoAckFlood(uint8_t channel);

    /// Stop whatever this radio is doing (carrier, flood, or packet TX)
    void stop();

    /// True if a carrier or flood is currently running
    bool isRunning() const { return running_; }

    // ---- Identity --------------------------------------------------------

    /// Stable module ID (0..4)
    uint8_t id() const { return id_; }

    /**
     * @brief Read the chip's last-seen RSSI on the current channel
     *
     * Used by the spectrum module. Reading RSSI requires the radio
     * to have been in RX at least briefly; the caller is expected
     * to have done that already.
     *
     * @return RSSI in dBm (negative number; -100 .. 0)
     */
    int readRssi() const;

    /// Switch to `ch`, enter RX briefly, return the carrier-detected dBm
    /// estimate. Safe to call from Core 0/1; takes the radio out of any
    /// in-progress sweep (the caller is responsible for re-engaging).
    /// Returns -128 if the radio is not connected.
    int8_t scanRssi(uint8_t ch, uint16_t dwellUs = 256);

private:
    /// Internal helper — read-modify-write `RF_SETUP` (CONT_WAVE | PLL_LOCK)
    void writeContWaveReg(bool on);

    /// Internal helper — power-up the chip and wait for the crystal
    void powerUp();

    /// Internal helper — power-down the chip
    void powerDown();

    uint8_t     id_;
    Nrf24Pins   pins_;
    uint8_t     paLevel_;
    uint8_t     dataRate_;
    uint8_t     channel_;
    bool        running_;
    bool        connected_;

    RF24*       radio_;     ///< Owned by this object
    SPIClass*   spi_;       ///< Not owned; lifetime managed by manager
};

/**
 * @brief Sweep method codes for `Nrf24Manager::jamSweep()`
 *
 * The first three describe the *order* in which channels are visited;
 * the last two describe how multiple radios cooperate.
 */
enum class Nrf24SweepMethod : uint8_t {
    List      = 0,  ///< Walk a predefined list (BT AFH, BLE adv, ...)
    Random    = 1,  ///< Pick a random channel each loop
    Brute     = 2,  ///< Step sequentially through the full range
    NoAck     = 3,  ///< Use NOACK flood instead of constant carrier
    Together  = 0x80,  ///< (flag bit) All radios on the same channel
    Separate  = 0x81,  ///< (flag bit) Round-robin `i = ch % count`
};

/// OR-able flags / methods packed into one byte for `jamSweep()`.
/// Use one Method + one SweepMode ORed together.
static constexpr uint8_t kNrf24SweepTogether = 0x00;
static constexpr uint8_t kNrf24SweepSeparate = 0x80;

/**
 * @brief Owns all nRF24 modules and the sweep worker task
 *
 * Singleton — use `phm::radio::g_nrf24`. The manager does not own
 * the SPI bus; the application provides a single `SPIClass` in
 * `setup()` and all radios share it.
 */
class Nrf24Manager {
public:
    /**
     * @brief Build the per-module driver objects
     *
     * Reads the configured module count and pin assignments from NVS
     * (via `phm::hal::g_storage`) and constructs one `Nrf24` per
     * wired module. Modules whose `ce` / `csn` resolve to -1 are
     * skipped — they will appear as `at(i) == nullptr`.
     *
     * @param count  Upper bound on the number of modules to bring up
     *               (clamped to `PHM_MAX_NRF24_RADIOS`).
     * @param pins   Array of pin pairs indexed by module id.
     */
    void setup(uint8_t count, const Nrf24Pins* pins);

    /// Tear down all radios, stop the worker, free the SPI bus
    void teardown();

    /// Look up a module by index; returns nullptr if not wired
    Nrf24* at(uint8_t i) { return radios_[i]; }
    const Nrf24* at(uint8_t i) const { return radios_[i]; }

    /// Number of *wired* modules (== number of non-null entries)
    uint8_t count() const { return count_; }

    // ---- High-level sweep API -------------------------------------------

    /**
     * @brief Start a sweep attack on all wired radios
     *
     * Spawns the worker task pinned to Core 0 at priority 3. The
     * task runs `workerLoop()` until `stopAll()` is called.
     *
     * @param startCh  First channel (inclusive)
     * @param stopCh   Last channel (inclusive)
     * @param method   One of the `Nrf24SweepMethod` values; the
     *                 `kNrf24SweepSeparate` bit selects the
     *                 multi-radio cooperation mode.
     * @return true on successful task creation
     */
    bool jamSweep(uint8_t startCh, uint8_t stopCh, uint8_t method);

    /// Signal the worker to stop and wait up to 500 ms
    void stopAll();

    /// True between `jamSweep()` and the worker exiting
    bool isRunning() const { return running_; }

private:
    /// One body of the worker — applies the current sweep step
    void workerLoop();

    /// FreeRTOS trampoline
    static void workerEntry(void* arg);

    /// Apply the current `sweepState_` to every wired radio
    void applySweepStep(uint8_t ch);

    // ---- State ----------------------------------------------------------
    Nrf24*      radios_[5] = {nullptr};
    uint8_t     count_      = 0;
    bool        running_    = false;

    // Sweep parameters (consumed by the worker)
    uint8_t     startCh_    = 0;
    uint8_t     stopCh_     = 0;
    uint8_t     method_     = 0;
    bool        separate_   = true;   ///< false = Together, true = Separate
    bool        noAck_      = false;  ///< true = NOACK flood, false = CW

    // Worker task
    void*        workerTask_ = nullptr;

    // SPI bus shared by every radio
    SPIClass*    spi_ = nullptr;     ///< Owned by this object
};

/// Singleton, defined in Nrf24.cpp
extern Nrf24Manager g_nrf24;

}  // namespace phm::radio
