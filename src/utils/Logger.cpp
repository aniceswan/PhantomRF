/**
 * @file Logger.cpp
 * @brief Tagged, leveled logger — implementation
 *
 * Formats messages via vsnprintf into a fixed-size scratch buffer,
 * writes them to `Serial` and into a small ring buffer in RAM.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "utils/Logger.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace phm::util {

// ---------------------------------------------------------------------------
// Singleton instance
// ---------------------------------------------------------------------------
Logger g_logger;

// ---------------------------------------------------------------------------
void Logger::setup() {
    // Don't call Serial.begin() here — main.cpp owns the Serial lifecycle
    // and configures the baud rate. We just clear the ring buffer.
    minLevel_ = LogLevel::Info;
    clear();
}

// ---------------------------------------------------------------------------
void Logger::log(LogLevel level, const char* tag, const char* message) {
    if (level < minLevel_) {
        return;  // filtered
    }

    // Build the ring buffer entry
    LogEntry e{};
    e.timestamp = millis();
    e.level     = level;

    // Copy the tag, truncating safely
    if (tag != nullptr) {
        strncpy(e.tag, tag, sizeof(e.tag) - 1);
        e.tag[sizeof(e.tag) - 1] = '\0';
    } else {
        strncpy(e.tag, "?", sizeof(e.tag) - 1);
    }

    // Copy the message, truncating safely
    if (message != nullptr) {
        strncpy(e.message, message, sizeof(e.message) - 1);
        e.message[sizeof(e.message) - 1] = '\0';
    } else {
        e.message[0] = '\0';
    }

    writeEntry(e);

    // Print to Serial: "[12345][I][tag] message\n"
    Serial.print('[');
    Serial.print(e.timestamp);
    Serial.print("][");
    Serial.print(levelName(e.level));
    Serial.print("][");
    Serial.print(e.tag);
    Serial.print("] ");
    Serial.println(e.message);
}

// ---------------------------------------------------------------------------
void Logger::logf(LogLevel level, const char* tag, const char* fmt, ...) {
    if (level < minLevel_) {
        return;
    }
    char buf[PHM_LOG_MSG_MAX_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    log(level, tag, buf);
}

// ---------------------------------------------------------------------------
void Logger::writeEntry(const LogEntry& e) {
    ring_[writeIndex_] = e;
    writeIndex_ = (writeIndex_ + 1) % PHM_LOG_RING_SIZE;
    if (count_ < PHM_LOG_RING_SIZE) {
        ++count_;
    }
}

// ---------------------------------------------------------------------------
const LogEntry* Logger::entryAt(size_t index) const {
    if (index >= count_) {
        return nullptr;
    }
    // index 0 = oldest entry currently held
    const size_t start = (count_ < PHM_LOG_RING_SIZE) ? 0 : writeIndex_;
    const size_t pos   = (start + index) % PHM_LOG_RING_SIZE;
    return &ring_[pos];
}

// ---------------------------------------------------------------------------
void Logger::clear() {
    for (LogEntry& e : ring_) {
        e = LogEntry{};
    }
    writeIndex_ = 0;
    count_      = 0;
}

// ---------------------------------------------------------------------------
const char* Logger::levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose: return "VRB";
        case LogLevel::Debug:   return "DBG";
        case LogLevel::Info:    return "INF";
        case LogLevel::Warn:    return "WRN";
        case LogLevel::Error:   return "ERR";
    }
    return "??";
}

// ---------------------------------------------------------------------------
const char* Logger::levelName(uint8_t level) {
    if (level > static_cast<uint8_t>(LogLevel::Error)) {
        return "?";
    }
    return levelName(static_cast<LogLevel>(level));
}

}  // namespace phm::util
