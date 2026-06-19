/**
 * @file Console.h
 * @brief USB-CDC and web-terminal command-line interface
 *
 * A single, shared command-line parser. The same `processLine()` is fed
 * from:
 *   - USB CDC Serial (default transport; `loop()` polls `Serial`)
 *   - The web UI's `/api/cli` endpoint (DESIGN §11.2)
 *   - The web UI's WebSocket `command` frame (DESIGN §20.5)
 *   - The OLED menu's per-screen actions (OledMenu.cpp)
 *
 * That way the "what does `attack bt` do" knowledge lives in **one**
 * place, and the web UI cannot drift from the serial CLI.
 *
 * The transport is abstracted via an `OutputFn` callback. By default it
 * prints to `Serial`; the web server swaps it for a buffer-sink that
 * gets returned in the JSON response.
 *
 * The parser is intentionally small: a hand-rolled tokenizer that handles
 * quoted strings (`"hello world"` = one token) and backslash escapes. The
 * command set is closed (see `Command` enum) but each command may take
 * free-form arguments.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Module.h"
#include "hal/Board.h"

#include <functional>
#include <string>
#include <vector>

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace phm::ui {

/**
 * @brief USB-CDC / web terminal — shared command-line interface
 *
 * Singleton registered with the `PhantomRF` core. Output is redirected
 * through a callback; the default callback prints to `Serial`.
 */
class Console : public ::phm::IModule {
public:
    /// Stable module id
    static constexpr uint8_t MODULE_ID = 0xE2;

    /// Maximum characters we keep in the input line buffer
    static constexpr size_t kMaxLine = 256;
    /// Number of lines kept in the recall history
    static constexpr size_t kMaxHistory = 32;

    /// Callback signature: a single line of output (already newline-terminated
    /// by the caller if desired).
    using OutputFn = std::function<void(const String&)>;

    // ---- IModule ---------------------------------------------------------
    const char* name() const override { return "Console"; }
    const char* description() const override { return "USB CDC + web terminal"; }
    uint8_t id() const override { return MODULE_ID; }
    void setup() override;
    void loop() override;

    // ---- Public API ------------------------------------------------------

    /// Process one line of input, return the textual reply.
    /// The reply is the same string that was sent through `output()`,
    /// joined with newlines. The web server uses the returned value to
    /// populate the JSON response.
    String processLine(const String& line);

    /// Redirect output to a different sink. Pass an empty function to
    /// revert to the default (Serial) sink.
    void setOutput(OutputFn fn) { output_ = std::move(fn); }

    /// Current output sink. Mostly useful for the web server so it can
    /// temporarily swap to a capture buffer.
    const OutputFn& output() const { return output_; }

    /// Echo a prompt to the current output. Used by the loop() that
    /// polls the serial port.
    void printPrompt();

private:
    // ---- Command dispatch ------------------------------------------------
    void printHelp();
    void printInfo();
    void printStatus();
    void cmdAttack(const std::vector<String>& args);
    void cmdStop(const std::vector<String>& args);
    void cmdScan(const std::vector<String>& args);
    void cmdRecord(const std::vector<String>& args);
    void cmdReplay(const std::vector<String>& args);
    void cmdSet(const std::vector<String>& args);
    void cmdGet(const std::vector<String>& args);
    void cmdReboot(const std::vector<String>& args);
    void cmdReset(const std::vector<String>& args);
    void cmdSave(const std::vector<String>& args);
    void cmdHistory(const std::vector<String>& args);

    // ---- Helpers ---------------------------------------------------------
    void writeLine(const String& s);
    void writeRaw(const String& s);
    std::vector<String> tokenize(const String& line) const;
    void pushHistory(const String& line);
    String completeCommand(const String& prefix) const;

    // ---- State -----------------------------------------------------------
    OutputFn output_;              ///< Current sink
    String lineBuffer_;            ///< In-progress line (USB CDC)
    std::vector<String> history_;  ///< Last N lines
    uint32_t lastCharMs_ = 0;      ///< millis() of last rx char
    bool echoEnabled_ = true;      ///< Local echo (USB CDC)
};

/// Singleton instance, defined in Console.cpp
extern Console g_console;

}  // namespace phm::ui
