/**
 * @file Console.cpp
 * @brief USB-CDC / web terminal — implementation
 *
 * The class is "transport-light": it doesn't talk to `Serial` directly
 * except for the default output sink. That keeps the web server able to
 * call `processLine()` from a request handler without Serial side-effects.
 *
 * `loop()` is the only place that touches `Serial.available()`. The
 * polling is intentionally trivial — Arduino's USB-CDC driver does the
 * actual buffering.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "ui/cli/Console.h"
#include "hal/Storage.h"
#include "core/State.h"
#include "modules/Nrf24Jammer/Nrf24Jammer.h"
#include "modules/Cc1101Jammer/Cc1101Jammer.h"
#include "modules/WifiAttack/WifiAttack.h"
#include "modules/BleAttack/BleAttack.h"
#include "modules/Spectrum/Spectrum.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

#include <esp_system.h>

#include "core/Config.h"
#include "core/EventBus.h"
#include "core/PhantomRF.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Buttons.h"
#include "hal/Led.h"
#include "hal/Power.h"
#include "hal/Storage.h"
#include "utils/ChannelMath.h"
#include "utils/Logger.h"

namespace phm::ui {

using phm::hal::g_storage;
using phm::hal::g_buttons;
using phm::hal::g_led;
using phm::hal::g_board;
using phm::hal::g_power;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Console g_console;

static constexpr const char* kTag = "cli";
static constexpr const char* kPrompt = "phantom> ";

// ---- Command table for help / tab completion -----------------------------
struct CmdEntry {
    const char* name;
    const char* help;
};
static const CmdEntry kCommands[] = {
    { "help",     "List commands" },
    { "?",        "Alias for help" },
    { "info",     "Device info (version, heap, uptime)" },
    { "status",   "Current state (which module, which attack)" },
    { "attack",   "Start an attack (subcommand: bt, ble, wifi, 2_4, subghz, drone, spec, stop)" },
    { "stop",     "Stop the current attack" },
    { "scan",     "Scan networks / devices (wifi, ble)" },
    { "record",   "Record sub-GHz signal (record <mhz> [ms])" },
    { "replay",   "Replay last recording (replay <mhz>)" },
    { "set",      "Set NVS key (set <key> <value>)" },
    { "get",      "Get NVS key (get <key>)" },
    { "save",     "Save current settings to flash" },
    { "history",  "Show command history" },
    { "reboot",   "Reboot device" },
    { "reset",    "Factory reset (clears NVS)" },
};
static constexpr size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

// ===========================================================================
// Lifecycle
// ===========================================================================
void Console::setup() {
    // Default sink: USB CDC Serial. The web server replaces this with a
    // buffer-sink before calling processLine(), then restores it.
    output_ = [](const String& s) {
        Serial.print(s);
    };

    // Detect USB host (for the welcome banner). On boards with native USB
    // (ESP32-S2/S3), Serial.dtr() goes true when the host opens the port.
    g_state.usbConnected = (Serial || true);  // dtr() not on classic ESP32, fallback for S3

    Serial.println();
    Serial.print(PHM_NAME);
    Serial.print(" v");
    Serial.print(PHM_VERSION_STRING);
    Serial.print(" on ");
    Serial.print(phm::hal::g_board.boardName());
    Serial.println(" ready.");
    Serial.println("Type 'help' for commands. Ctrl+C stops the current attack.");
    Serial.print(kPrompt);
    lastCharMs_ = millis();
    LOGI(kTag, "Console ready (USB CDC at 115200 8N1)");
}

void Console::loop() {
    // Poll the USB CDC serial port for characters. We assemble lines in
    // `lineBuffer_` until a CR or LF arrives, then dispatch.
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        lastCharMs_ = millis();

        if (c == '\r' || c == '\n') {
            if (lineBuffer_.isEmpty()) {
                writeRaw("\n");
                printPrompt();
                continue;
            }
            writeRaw("\n");
            const String line = lineBuffer_;
            lineBuffer_.clear();
            pushHistory(line);
            const String reply = processLine(line);
            if (!reply.isEmpty()) {
                writeRaw(reply);
                if (!reply.endsWith("\n")) writeRaw("\n");
            }
            printPrompt();
        } else if (c == 0x08 || c == 0x7F) {
            // Backspace / DEL
            if (!lineBuffer_.isEmpty()) {
                lineBuffer_.remove(lineBuffer_.length() - 1);
                if (echoEnabled_) writeRaw("\b \b");
            }
        } else if (c == 0x03) {
            // Ctrl+C — stop the current attack
            writeRaw("^C\n");
            lineBuffer_.clear();
            processLine("attack stop");
            printPrompt();
        } else if (c == '\t') {
            // Tab completion on the first token
            const String completed = completeCommand(lineBuffer_);
            if (!completed.isEmpty()) {
                lineBuffer_ = completed;
                if (echoEnabled_) {
                    writeRaw("\r");
                    writeRaw(kPrompt);
                    writeRaw(lineBuffer_);
                }
            }
        } else if (lineBuffer_.length() < kMaxLine - 1) {
            lineBuffer_ += c;
            if (echoEnabled_) {
                Serial.write(c);
            }
        }
    }
}

void Console::printPrompt() {
    writeRaw(kPrompt);
}

// ===========================================================================
// Public: processLine
// ===========================================================================
String Console::processLine(const String& line) {
    // Tokenize and dispatch
    const std::vector<String> args = tokenize(line);
    if (args.empty()) {
        return String();
    }
    const String& cmd = args[0];

    // ---- dispatch table ------------------------------------------------
    if (cmd == "help" || cmd == "?") {
        printHelp();
        return String();
    }
    if (cmd == "info")  { printInfo();  return String(); }
    if (cmd == "status"){ printStatus();return String(); }
    if (cmd == "attack"){ cmdAttack(args); return String(); }
    if (cmd == "stop")  { cmdStop(args);   return String(); }
    if (cmd == "scan")  { cmdScan(args);  return String(); }
    if (cmd == "record"){ cmdRecord(args);return String(); }
    if (cmd == "replay"){ cmdReplay(args);return String(); }
    if (cmd == "set")   { cmdSet(args);   return String(); }
    if (cmd == "get")   { cmdGet(args);   return String(); }
    if (cmd == "save")  { cmdSave(args);  return String(); }
    if (cmd == "history"){ cmdHistory(args); return String(); }
    if (cmd == "reboot"){ cmdReboot(args); return String(); }
    if (cmd == "reset") { cmdReset(args); return String(); }

    String out = "Unknown command: ";
    out += cmd;
    out += " (try 'help')";
    return out;
}

// ===========================================================================
// Commands
// ===========================================================================
void Console::printHelp() {
    String out;
    out += "Available commands:\n";
    for (const auto& c : kCommands) {
        out += "  ";
        out += c.name;
        // Column-align
        for (size_t i = std::strlen(c.name); i < 10; ++i) out += ' ';
        out += "- ";
        out += c.help;
        out += '\n';
    }
    out += "\nExamples:\n";
    out += "  attack bt           # start Bluetooth jam\n";
    out += "  attack stop         # stop current attack\n";
    out += "  scan wifi           # list nearby APs\n";
    out += "  set nrf24.pa 3      # set NVS key nrf24.pa to 3\n";
    out += "  get ap.ssid         # print current AP SSID\n";
    out += "  reboot              # restart the device\n";
    writeRaw(out);
}

void Console::printInfo() {
    String out;
    out += "Device:   ";   out += PHM_NAME;      out += '\n';
    out += "Version:  ";   out += PHM_VERSION_STRING; out += '\n';
    out += "Board:    ";   out += phm::hal::g_board.boardName(); out += '\n';
    out += "Chip:     ";   out += ESP.getChipModel(); out += '\n';
    out += "CPU:      ";   out += ESP.getCpuFreqMHz(); out += " MHz\n";
    out += "Heap:     ";   out += ESP.getFreeHeap(); out += " bytes free\n";
    out += "Min heap: ";   out += ESP.getMinFreeHeap(); out += '\n';
    out += "Uptime:   ";   out += (millis() - g_state.bootTime) / 1000UL; out += " s\n";
    out += "Temp:     ";   out += g_state.internalTempC; out += " C\n";
    out += "Vbat:     ";   out += g_state.vbat_mV; out += " mV\n";
    out += "AP:       ";   out += (g_state.apActive ? "active" : "off"); out += '\n';
    out += "WS clis:  ";   out += g_state.webConnected ? 1 : 0; out += '\n';
    writeRaw(out);
}

void Console::printStatus() {
    String out;
    out += "State:     ";
    switch (g_state.state) {
        case phm::State::Booting:   out += "Booting";   break;
        case phm::State::Idle:      out += "Idle";      break;
        case phm::State::Running:   out += "Running";   break;
        case phm::State::Stopping:  out += "Stopping";  break;
        case phm::State::Error:     out += "Error";     break;
        case phm::State::DeepSleep: out += "DeepSleep"; break;
    }
    out += '\n';
    out += "Module:    ";
    if (g_state.currentModuleId == 0) out += "(none)";
    else                              out += g_state.currentModuleId;
    out += '\n';
    if (g_state.lastError[0] != '\0') {
        out += "Last err:  ";
        out += g_state.lastError;
        out += '\n';
    }
    writeRaw(out);
}

void Console::cmdAttack(const std::vector<String>& args) {
    // `attack` with no subcommand is a usage error
    if (args.size() < 2) {
        writeLine("Usage: attack <bt|ble|wifi|2_4|subghz|drone|spec> [method]");
        return;
    }
    const String& target = args[1];

    if (target == "stop" || target == "off") {
        cmdStop({});
        return;
    }

    String out;
    out += "Starting ";
    out += target;
    if (args.size() >= 3) {
        out += " (method=";
        out += args[2];
        out += ")";
    }
    out += "...\n";

    // The actual attack modules are registered separately; this CLI
    // only translates the user intent into an EventBus post so the
    // corresponding IModule can pick it up.
    {
        phm::Event ev;
        ev.type      = phm::EventType::WebCommand;   // treated as "command from any source"
        ev.timestamp = millis();
        ev.sourceId  = MODULE_ID;
        const String payload = String("attack ") + target +
            (args.size() >= 3 ? (String(" ") + args[2]) : String());
        ev.dataLen  = payload.length();
        ev.data     = new uint8_t[ev.dataLen + 1];
        std::memcpy(ev.data, payload.c_str(), ev.dataLen + 1);
        if (!g_events.post(ev)) {
            writeLine("Warning: event queue full, attack may not start");
        }
    }  // ev destructor frees the buffer

    g_state.state            = phm::State::Running;
    g_state.currentModuleId  = 0;  // real module will overwrite this
    out += "Running. Send 'attack stop' to abort.\n";
    writeRaw(out);
}

void Console::cmdStop(const std::vector<String>& /*args*/) {
    g_state.state = phm::State::Stopping;
    {
        phm::Event ev;
        ev.type      = phm::EventType::WebCommand;
        ev.timestamp = millis();
        ev.sourceId  = MODULE_ID;
        const char* payload = "attack stop";
        ev.dataLen   = std::strlen(payload);
        ev.data      = new uint8_t[ev.dataLen + 1];
        std::memcpy(ev.data, payload, ev.dataLen + 1);
        if (!g_events.post(ev)) {
            writeLine("Warning: event queue full");
        }
    }  // ev destructor frees the buffer

    g_state.state            = phm::State::Idle;
    g_state.currentModuleId  = 0;
    writeLine("Stopped.");
}

void Console::cmdScan(const std::vector<String>& args) {
    if (args.size() < 2) {
        writeLine("Usage: scan <wifi|ble>");
        return;
    }
    if (args[1] == "wifi") {
        writeLine("Scanning WiFi... (not implemented in M0; the radio modules will hook this)");
        return;
    }
    if (args[1] == "ble") {
        writeLine("Scanning BLE... (not implemented in M0; the NimBLE module will hook this)");
        return;
    }
    writeLine("Unknown scan target. Use 'wifi' or 'ble'.");
}

void Console::cmdRecord(const std::vector<String>& args) {
    if (args.size() < 2) {
        writeLine("Usage: record <freq_mhz> [duration_ms]");
        return;
    }
    const long mhz = std::atol(args[1].c_str());
    if (mhz < 100 || mhz > 1000) {
        writeLine("Frequency must be between 100 and 1000 MHz");
        return;
    }
    const uint32_t dur = (args.size() >= 3) ? static_cast<uint32_t>(std::atol(args[2].c_str())) : 1000UL;
    String out;
    out += "Recording at "; out += mhz; out += " MHz for "; out += dur; out += " ms\n";
    out += "(CC1101 driver will perform the actual capture in M1)\n";
    writeRaw(out);
}

void Console::cmdReplay(const std::vector<String>& args) {
    if (args.size() < 2) {
        writeLine("Usage: replay <freq_mhz>");
        return;
    }
    const long mhz = std::atol(args[1].c_str());
    if (mhz < 100 || mhz > 1000) {
        writeLine("Frequency must be between 100 and 1000 MHz");
        return;
    }
    String out;
    out += "Replaying last recording at "; out += mhz; out += " MHz\n";
    out += "(CC1101 driver will perform the actual transmission in M1)\n";
    writeRaw(out);
}

void Console::cmdSet(const std::vector<String>& args) {
    if (args.size() < 3) {
        writeLine("Usage: set <key> <value>");
        return;
    }
    const char* key = args[1].c_str();
    const char* val = args[2].c_str();

    // Try int, bool, then fall back to string. We use a small heuristic so
    // that `set nrf24.pa 3` doesn't get stored as the string "3".
    char* endp = nullptr;
    const long iv = std::strtol(val, &endp, 10);
    if (endp != val && *endp == '\0') {
        g_storage.setInt(key, static_cast<int32_t>(iv));
    } else if (std::strcmp(val, "true") == 0 || std::strcmp(val, "false") == 0) {
        g_storage.setBool(key, std::strcmp(val, "true") == 0);
    } else {
        g_storage.setString(key, val);
    }
    g_storage.commit();
    String out = "Set "; out += key; out += " = "; out += val; out += "\n";
    writeRaw(out);
}

void Console::cmdGet(const std::vector<String>& args) {
    if (args.size() < 2) {
        writeLine("Usage: get <key>");
        return;
    }
    const char* key = args[1].c_str();

    // Try each type. String is the most common, so try it first.
    String s;
    if (g_storage.getString(key, s)) {
        String out = key; out += " = "; out += s; out += "\n";
        writeRaw(out);
        return;
    }
    int32_t iv = 0;
    if (g_storage.getInt(key, iv)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s = %ld\n", key, static_cast<long>(iv));
        writeRaw(String(buf));
        return;
    }
    bool bv = false;
    if (g_storage.getBool(key, bv)) {
        String out = key; out += " = "; out += (bv ? "true" : "false"); out += "\n";
        writeRaw(out);
        return;
    }
    writeLine("(key not set)");
}

void Console::cmdSave(const std::vector<String>& /*args*/) {
    g_storage.commit();
    writeLine("Settings saved to flash.");
}

void Console::cmdHistory(const std::vector<String>& /*args*/) {
    String out;
    int n = 1;
    for (const auto& h : history_) {
        out += n++;
        out += "  ";
        out += h;
        out += '\n';
    }
    writeRaw(out);
}

void Console::cmdReboot(const std::vector<String>& /*args*/) {
    writeLine("Rebooting...");
    Serial.flush();
    delay(100);
    ESP.restart();
}

void Console::cmdReset(const std::vector<String>& /*args*/) {
    writeLine("Factory reset: clearing NVS and rebooting...");
    g_storage.clearNamespace();
    Serial.flush();
    delay(200);
    ESP.restart();
}

// ===========================================================================
// Helpers
// ===========================================================================
void Console::writeLine(const String& s) {
    String tmp = s;
    tmp += '\n';
    writeRaw(tmp);
}

void Console::writeRaw(const String& s) {
    if (output_) {
        output_(s);
    } else {
        Serial.print(s);
    }
}

std::vector<String> Console::tokenize(const String& line) const {
    std::vector<String> out;
    String cur;
    bool inQuotes = false;
    char quoteCh  = 0;
    for (size_t i = 0; i < line.length(); ++i) {
        const char c = line.charAt(i);
        if (inQuotes) {
            if (c == '\\' && i + 1 < line.length()) {
                // Simple escape: \" and \\ inside a quoted string
                cur += line.charAt(++i);
            } else if (c == quoteCh) {
                inQuotes = false;
            } else {
                cur += c;
            }
        } else {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.isEmpty()) {
                    out.push_back(cur);
                    cur.clear();
                }
            } else if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteCh  = c;
            } else {
                cur += c;
            }
        }
    }
    if (!cur.isEmpty()) {
        out.push_back(cur);
    }
    return out;
}

void Console::pushHistory(const String& line) {
    if (line.isEmpty()) return;
    if (!history_.empty() && history_.back() == line) return;
    history_.push_back(line);
    if (history_.size() > kMaxHistory) {
        history_.erase(history_.begin());
    }
}

String Console::completeCommand(const String& prefix) const {
    // Tab-completion only completes command names (first token).
    if (prefix.indexOf(' ') >= 0) {
        return String();
    }
    String out;
    for (const auto& c : kCommands) {
        if (std::strncmp(c.name, prefix.c_str(), prefix.length()) == 0) {
            if (out.isEmpty()) {
                out = c.name;
            } else {
                // Two matches — give up. The shell will print nothing.
                return String();
            }
        }
    }
    if (!out.isEmpty() && out.length() > prefix.length()) {
        return out;
    }
    return String();
}

}  // namespace phm::ui
