/**
 * @file Module.h
 * @brief The IModule interface — every attack/UI/subsystem is an IModule
 *
 * PhantomRF is built around a tiny plugin system: every feature is a class
 * that derives from `IModule` and is registered with the `PhantomRF` core.
 * The core calls each module's lifecycle methods in order, so modules do
 * not need to know about each other.
 *
 * The interface is intentionally narrow — extended functionality
 * (registering web routes, CLI commands, OLED menu items) is opt-in via
 * sub-interfaces defined in their respective subsystems.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

namespace phm {

/**
 * @brief Abstract base class for every PhantomRF subsystem / attack module
 *
 * Lifecycle:
 *   1. `setup()` is called once after registration, before the main loop
 *      starts. Use it to bring up SPI buses, allocate buffers, etc.
 *   2. `loop()` is called from the main loop on every tick. It must be
 *      non-blocking (return promptly) — use FreeRTOS tasks for heavy work.
 *   3. `teardown()` is called once before the device reboots or enters
 *      deep sleep. Use it to power down radios, flush state, etc.
 *
 * The three callbacks have empty default implementations, so subclasses
 * only override what they need.
 */
class IModule {
public:
    /// Human-readable module name, used in logs, web UI, and OLED
    virtual const char* name() const = 0;

    /// One-line description, used in help screens
    virtual const char* description() const = 0;

    /// Stable unique identifier (0..255). Used in the web API and events.
    virtual uint8_t id() const = 0;

    /// Called once after the module is registered and the core is ready
    virtual void setup() {}

    /// Called from the main loop on every tick; must be non-blocking
    virtual void loop() {}

    /// Called once before reboot / deep sleep
    virtual void teardown() {}

    /// Virtual destructor for safe polymorphic deletion
    virtual ~IModule() = default;
};

}  // namespace phm
