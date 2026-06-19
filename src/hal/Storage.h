/**
 * @file Storage.h
 * @brief NVS (settings) + LittleFS (files) abstraction
 *
 * Persistent state lives in two places:
 *   - **NVS** (via the `Preferences` API) for small key/value settings
 *     that are read often (AP SSID, nRF24 channel list, current PA level).
 *   - **LittleFS** (where supported) for larger, file-shaped data:
 *     web UI assets, recorded sub-GHz captures, settings backups, logs.
 *
 * All NVS operations are scoped to a single namespace ("phm"); callers
 * supply just the key. This avoids accidental cross-namespace clashes
 * with user-installed libraries.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <vector>

#include <Arduino.h>
#include <Preferences.h>
#include <stddef.h>
#include <stdint.h>

namespace phm::hal {

/**
 * @brief Combined NVS + LittleFS facade
 *
 * One instance lives at `phm::hal::g_storage`. Call `setup()` once at
 * boot; the rest of the API is non-blocking and safe to call from any
 * task.
 */
class Storage {
public:
    /// Initialise NVS and mount LittleFS
    void setup();

    // ---- NVS (settings, scoped to "phm" namespace) ---------------------
    bool getString(const char* key, String& out);
    bool setString(const char* key, const char* value);
    bool getInt(const char* key, int32_t& out);
    bool setInt(const char* key, int32_t value);
    bool getBool(const char* key, bool& out);
    bool setBool(const char* key, bool value);
    bool getBytes(const char* key, uint8_t* out, size_t len);
    bool setBytes(const char* key, const uint8_t* value, size_t len);
    bool remove(const char* key);
    bool commit();
    void clearNamespace();

    // ---- LittleFS (files) -----------------------------------------------
    /// Mount the partition. Returns true on success.
    bool mountFs();

    /// True if the FS is mounted
    bool fsMounted() const { return fsMounted_; }

    bool fileExists(const char* path);
    size_t fileSize(const char* path);
    String readFile(const char* path);
    bool writeFile(const char* path, const String& content);
    bool deleteFile(const char* path);

    /// List immediate children of a directory. Returns just the names.
    std::vector<String> listDir(const char* path);

    /// Free space on the LittleFS partition, in bytes (0 if not mounted)
    size_t fsFreeBytes();

    /// Total space on the LittleFS partition, in bytes (0 if not mounted)
    size_t fsTotalBytes();

    // ---- PSRAM (volatile large buffers, M0+) ----------------------------
    /// Allocate `size` bytes from PSRAM (or heap if PSRAM not available).
    /// Returns nullptr on failure. Caller must `psramFree()`.
    void* psramAlloc(size_t size);

    /// Free a pointer previously returned by `psramAlloc()`.
    void psramFree(void* ptr);

    /// True if the board has usable PSRAM
    bool hasPsram() const;

private:
    Preferences prefs_;
    bool fsMounted_ = false;
};

/// Singleton instance, defined in Storage.cpp
extern Storage g_storage;

/**
 * @brief Lock-free single-producer/single-consumer ring buffer in PSRAM
 *
 * Used for high-frequency sample capture (CC1101 record, log burst, raw
 * RF samples) where flash wear is unacceptable. The buffer is allocated
 * from PSRAM when available; otherwise from heap. SPSC semantics — one
 * task writes, one task reads. No mutex required.
 *
 * Capacity is rounded up to a power of two so the index wrap can be a
 * single bitmask instead of a modulo.
 */
template<typename T> class RingBuffer {
public:
    /// Allocate a ring buffer with `capacity` elements. Returns false on
    /// allocation failure (out of PSRAM/heap).
    bool begin(size_t capacity) {
        capacity_ = 1;
        while (capacity_ < capacity)
            capacity_ <<= 1;
        mask_ = capacity_ - 1;
        buf_ = static_cast<T*>(g_storage.psramAlloc(capacity_ * sizeof(T)));
        if (!buf_)
            return false;
        head_ = 0;
        tail_ = 0;
        return true;
    }

    void end() {
        if (buf_) {
            g_storage.psramFree(buf_);
            buf_ = nullptr;
        }
        capacity_ = 0;
        head_ = 0;
        tail_ = 0;
    }

    /// Number of elements currently stored
    size_t size() const { return head_ - tail_; }

    /// Maximum capacity
    size_t capacity() const { return capacity_; }

    /// True if the buffer is full
    bool full() const { return size() >= capacity_; }

    /// True if no elements
    bool empty() const { return head_ == tail_; }

    /// Free space
    size_t free() const { return capacity_ - size(); }

    /// Push one element. Returns false if full (caller can choose to
    /// overwrite — see `pushOver()` for that variant).
    bool push(const T& v) {
        if (full())
            return false;
        buf_[head_ & mask_] = v;
        ++head_;
        return true;
    }

    /// Push one element, overwriting the oldest if full.
    void pushOver(const T& v) {
        if (full())
            ++tail_;
        buf_[head_ & mask_] = v;
        ++head_;
    }

    /// Pop one element. Returns false if empty.
    bool pop(T& out) {
        if (empty())
            return false;
        out = buf_[tail_ & mask_];
        ++tail_;
        return true;
    }

    /// Peek at the head (most recently pushed) without removing
    bool peek(T& out) const {
        if (empty())
            return false;
        out = buf_[(head_ - 1) & mask_];
        return true;
    }

    /// Reset to empty
    void clear() { head_ = tail_ = 0; }

private:
    T* buf_ = nullptr;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    size_t head_ = 0;  /// monotonically increasing write index
    size_t tail_ = 0;  /// monotonically increasing read index
};

}  // namespace phm::hal
