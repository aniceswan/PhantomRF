/**
 * @file Settings.h
 * @brief Settings management module
 *
 * Reads / writes the NVS keys defined in DESIGN §13.1, exposes them
 * to the web UI as JSON, and owns the "factory reset" flow. Lives
 * at a high module ID (0xF0) so it sorts last in the registry.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "core/Module.h"

namespace phm::modules {

/**
 * @brief Persistent settings facade
 *
 * Module ID 0xF0. Stateless from the caller's perspective: every
 * method reads or writes NVS, returning success/failure. RAM
 * mirroring of the most-used keys is the caller's job (or, in
 * practice, the web subsystem's).
 */
class SettingsModule : public ::phm::IModule {
public:
    static constexpr uint8_t  MODULE_ID   = 0xF0;
    static constexpr const char* MODULE_NAME = "settings";
    static constexpr const char* MODULE_DESC =
        "Persistent settings (NVS) and factory reset";

    uint8_t id() const override { return MODULE_ID; }
    const char* name() const override { return MODULE_NAME; }
    const char* description() const override { return MODULE_DESC; }

    void setup() override;
    void loop() override;
    void teardown() override;

    // ---- JSON I/O --------------------------------------------------------

    /// Serialise every known setting to a JSON document for the web UI
    String toJson();

    /**
     * @brief Apply a partial update from JSON
     *
     * Unknown keys are ignored. Missing keys keep their current value.
     * Each recognised key is validated (range, length) before being
     * written; on validation failure the corresponding key is left
     * unchanged and the call returns false.
     *
     * @return true if every supplied key was valid and persisted
     */
    bool fromJson(const String& json);

    // ---- Destructive operations ------------------------------------------

    /**
     * @brief Wipe the "phm" NVS namespace and reboot
     *
     * After clearing, the next boot's `setup()` regenerates the AP
     * password and stores it back. A 1-second delay is inserted so
     * the HTTP response can complete before the restart.
     */
    void factoryReset();

    /**
     * @brief Generate a new 16-character AP password
     *
     * Uses `esp_random()` for cryptographic strength; characters are
     * drawn from alphanumeric + a small set of safe symbols (the
     * standard set W0rthlessS0ul uses). The new value is persisted
     * to NVS immediately.
     *
     * @return The newly-generated password
     */
    String regenerateApPassword();

private:
    /// Validate one key from the JSON document; returns true if the
    /// value is acceptable for the named key.
    static bool validateKey(const char* key, const char* value);
    static bool validateKey(const char* key, int32_t value);
};

extern SettingsModule g_settings;

}  // namespace phm::modules
