/**
 * @file Logger.h
 * @brief Tagged, leveled logger with a small ring buffer
 *
 * `phm::util::g_logger` is a process-wide singleton that formats log
 * messages, prints them to the Arduino `Serial`, and keeps the last
 * `PHM_LOG_RING_SIZE` entries in RAM so the web UI can fetch them.
 *
 * Use the convenience macros `LOGV / LOGD / LOGI / LOGW / LOGE` for
 * printf-style formatting. The macros take a `tag` (a short string
 * identifying the source) and a printf format string.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include "core/Config.h"

#include <array>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

namespace phm::util {

/**
 * @brief Severity of a log message
 *
 * Higher value = more severe. The `Verbose` level is filtered out by
 * default to keep the serial output quiet.
 */
enum class LogLevel : uint8_t {
    Verbose = 0,  ///< Chatty debug output
    Debug = 1,    ///< Diagnostic info useful during development
    Info = 2,     ///< Normal operational messages
    Warn = 3,     ///< Something unexpected, recoverable
    Error = 4,    ///< Something broke, action is needed
};

/**
 * @brief One entry in the ring buffer
 *
 * POD so the array can live in BSS and be inspected via the web UI
 * without copy overhead.
 */
struct LogEntry {
    uint32_t timestamp;                 ///< millis() when logged
    LogLevel level;                     ///< Severity
    char tag[PHM_LOG_TAG_MAX_LEN];      ///< Module tag, NUL-terminated
    char message[PHM_LOG_MSG_MAX_LEN];  ///< Formatted message, NUL-terminated
};

/**
 * @brief Singleton logger
 *
 * Use the global `phm::util::g_logger` from anywhere. Call `setup()`
 * once during boot to start logging.
 */
class Logger {
public:
    /// Initialise — opens Serial at 115200 baud and resets the ring buffer
    void setup();

    /// Log a pre-formatted message
    void log(LogLevel level, const char* tag, const char* message);

    /// Log a printf-style formatted message
    void logf(LogLevel level, const char* tag, const char* fmt, ...);

    /// Set the minimum level that will be printed to Serial
    void setMinLevel(LogLevel level) { minLevel_ = level; }

    /// Current minimum level
    LogLevel minLevel() const { return minLevel_; }

    /// Read a ring-buffer entry (0 = oldest, count()-1 = newest)
    const LogEntry* entryAt(size_t index) const;

    /// Number of entries currently in the ring (capped at the size)
    size_t count() const { return count_; }

    /// Empty the ring buffer
    void clear();

    /// String form of a level, e.g. "INFO" — for the web UI
    static const char* levelName(LogLevel level);

    /// String form of a level from a raw uint8_t (returns "?" if unknown)
    static const char* levelName(uint8_t level);

private:
    void writeEntry(const LogEntry& e);

    LogLevel minLevel_ = LogLevel::Info;
    std::array<LogEntry, PHM_LOG_RING_SIZE> ring_{};
    size_t writeIndex_ = 0;
    size_t count_ = 0;
};

/// Singleton instance, defined in Logger.cpp
extern Logger g_logger;

}  // namespace phm::util

// ---------------------------------------------------------------------------
// Convenience macros — pass a short tag and a printf-style format.
// Example: LOGI("nrf24", "channel set to %u", ch);
// ---------------------------------------------------------------------------
#define LOGV(tag, ...) phm::util::g_logger.logf(phm::util::LogLevel::Verbose, tag, __VA_ARGS__)
#define LOGD(tag, ...) phm::util::g_logger.logf(phm::util::LogLevel::Debug, tag, __VA_ARGS__)
#define LOGI(tag, ...) phm::util::g_logger.logf(phm::util::LogLevel::Info, tag, __VA_ARGS__)
#define LOGW(tag, ...) phm::util::g_logger.logf(phm::util::LogLevel::Warn, tag, __VA_ARGS__)
#define LOGE(tag, ...) phm::util::g_logger.logf(phm::util::LogLevel::Error, tag, __VA_ARGS__)
