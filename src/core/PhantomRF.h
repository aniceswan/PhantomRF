/**
 * @file PhantomRF.h
 * @brief The top-level application class — owns the module registry
 *
 * `PhantomRF` is a thin orchestrator. It doesn't know how to jam, how
 * to scan, or how to render a web page — it just:
 *   1. Brings up the hardware (board pins, storage, event bus).
 *   2. Iterates the registered modules, calling each one's `setup()`.
 *   3. Iterates the registered modules every loop tick.
 *
 * New features are added by subclassing `IModule` and calling
 * `registerModule()` before `setup()`.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Module.h"

#include <vector>

#include <stdint.h>

namespace phm {

/**
 * @brief The PhantomRF application — owns the module registry
 *
 * One instance is created in `main.cpp` as `phm::g_app`. The Arduino
 * `setup()` constructs it; `loop()` calls `loop()`.
 */
class PhantomRF {
public:
    /// Default constructor — does nothing; call `setup()` next
    PhantomRF() = default;

    /// Destructor — does not free registered modules (they are owned
    /// by whoever created them; usually static instances or `new`)
    ~PhantomRF() = default;

    // Non-copyable / non-movable — it's a singleton by convention
    PhantomRF(const PhantomRF&) = delete;
    PhantomRF& operator=(const PhantomRF&) = delete;
    PhantomRF(PhantomRF&&) = delete;
    PhantomRF& operator=(PhantomRF&&) = delete;

    /**
     * @brief Bring up the platform
     *
     * Initialises the board pins, storage, event bus, logger, and LED,
     * then calls `setup()` on every registered module in the order
     * they were registered. Idempotent: a second call is a no-op.
     */
    void setup();

    /**
     * @brief Main-loop tick
     *
     * Polls buttons, updates the event bus, then calls `loop()` on
     * every registered module. Designed to be called from the
     * Arduino `loop()`.
     */
    void loop();

    /**
     * @brief Register a module
     *
     * The module pointer is stored in an internal vector; PhantomRF
     * does NOT take ownership. The caller is responsible for keeping
     * the module alive for as long as it is registered.
     *
     * @param module Pointer to an IModule instance; must not be null
     */
    void registerModule(IModule* module);

    /**
     * @brief Look up a module by id
     *
     * @param id IModule::id() of the desired module
     * @return Pointer to the module, or nullptr if not found
     */
    IModule* findModule(uint8_t id) const;

    /// Number of currently registered modules
    size_t moduleCount() const { return modules_.size(); }

    /// True once `setup()` has been called
    bool isReady() const { return ready_; }

private:
    std::vector<IModule*> modules_;  ///< Registry; does not own
    bool ready_ = false;             ///< True after setup() has run
};

/// Singleton instance, set by main.cpp's setup()
extern PhantomRF* g_app;

}  // namespace phm
