/**
 * @file Storage.cpp
 * @brief NVS + LittleFS wrapper — implementation
 *
 * All NVS calls go through a single Preferences instance scoped to the
 * "phm" namespace. LittleFS is mounted once during `setup()`; the rest
 * of the FS API gracefully no-ops if the mount failed (e.g. on boards
 * without a LittleFS partition).
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "hal/Storage.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include "utils/Logger.h"

namespace phm::hal {

// ---------------------------------------------------------------------------
// Singleton instance
// ---------------------------------------------------------------------------
Storage g_storage;

// "phm" is short, ASCII, and unlikely to collide with library namespaces.
static constexpr const char* kNamespace = "phm";
static constexpr const char* kTag       = "storage";

// ---------------------------------------------------------------------------
void Storage::setup() {
    // NVS is opened lazily on first get/set; just verify we can begin.
    if (!prefs_.begin(kNamespace, false)) {
        // Read-only mode may have failed if NVS hasn't been initialised.
        // Try read-write, fall back to read-only.
        if (!prefs_.begin(kNamespace, true)) {
            LOGE(kTag, "NVS begin failed; settings will not persist");
        }
    }
    LOGD(kTag, "NVS namespace '%s' opened", kNamespace);

    mountFs();
}

// ---- NVS -------------------------------------------------------------------
bool Storage::getString(const char* key, String& out) {
    if (key == nullptr) return false;
    out = prefs_.getString(key, "");
    return prefs_.isKey(key);
}

bool Storage::setString(const char* key, const char* value) {
    if (key == nullptr || value == nullptr) return false;
    return prefs_.putString(key, value) > 0;
}

bool Storage::getInt(const char* key, int32_t& out) {
    if (key == nullptr) return false;
    if (!prefs_.isKey(key)) {
        return false;
    }
    out = prefs_.getInt(key, 0);
    return true;
}

bool Storage::setInt(const char* key, int32_t value) {
    if (key == nullptr) return false;
    return prefs_.putInt(key, value) > 0;
}

bool Storage::getBool(const char* key, bool& out) {
    if (key == nullptr) return false;
    if (!prefs_.isKey(key)) {
        return false;
    }
    out = prefs_.getBool(key, false);
    return true;
}

bool Storage::setBool(const char* key, bool value) {
    if (key == nullptr) return false;
    return prefs_.putBool(key, value) > 0;
}

bool Storage::getBytes(const char* key, uint8_t* out, size_t len) {
    if (key == nullptr || out == nullptr || len == 0) return false;
    if (!prefs_.isKey(key)) {
        return false;
    }
    return prefs_.getBytes(key, out, len) == len;
}

bool Storage::setBytes(const char* key, const uint8_t* value, size_t len) {
    if (key == nullptr || value == nullptr || len == 0) return false;
    return prefs_.putBytes(key, value, len) > 0;
}

bool Storage::remove(const char* key) {
    if (key == nullptr) return false;
    return prefs_.remove(key);
}

bool Storage::commit() {
    // end() returns void but flushes pending writes to flash. Always
    // succeed; failures inside nvs_set_blob() are reflected by the
    // putX() return values.
    prefs_.end();
    // Re-open read-write so subsequent operations work. Don't fail
    // if re-open fails — the caller can decide what to do.
    if (!prefs_.begin(kNamespace, false)) {
        (void)prefs_.begin(kNamespace, true);
    }
    return true;
}

void Storage::clearNamespace() {
    prefs_.clear();
    prefs_.end();
    prefs_.begin(kNamespace, false);
    LOGW(kTag, "NVS namespace cleared");
}

// ---- LittleFS --------------------------------------------------------------
bool Storage::mountFs() {
    if (fsMounted_) {
        return true;
    }
    if (!LittleFS.begin(true /* formatOnFail */)) {
        LOGW(kTag, "LittleFS mount failed; FS APIs will be no-ops");
        fsMounted_ = false;
        return false;
    }
    fsMounted_ = true;
    LOGD(kTag, "LittleFS mounted: %u / %u bytes free",
         static_cast<unsigned>(fsFreeBytes()),
         static_cast<unsigned>(fsTotalBytes()));
    return true;
}

bool Storage::fileExists(const char* path) {
    if (!fsMounted_ || path == nullptr) return false;
    return LittleFS.exists(path);
}

size_t Storage::fileSize(const char* path) {
    if (!fsMounted_ || path == nullptr) return 0;
    File f = LittleFS.open(path, "r");
    if (!f) return 0;
    const size_t sz = f.size();
    f.close();
    return sz;
}

String Storage::readFile(const char* path) {
    if (!fsMounted_ || path == nullptr) return String();
    File f = LittleFS.open(path, "r");
    if (!f) return String();
    String out = f.readString();
    f.close();
    return out;
}

bool Storage::writeFile(const char* path, const String& content) {
    if (!fsMounted_ || path == nullptr) return false;
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    const size_t n = f.print(content);
    f.close();
    return n == static_cast<size_t>(content.length());
}

bool Storage::deleteFile(const char* path) {
    if (!fsMounted_ || path == nullptr) return false;
    return LittleFS.remove(path);
}

std::vector<String> Storage::listDir(const char* path) {
    std::vector<String> out;
    if (!fsMounted_ || path == nullptr) return out;
    File dir = LittleFS.open(path, "r");
    if (!dir || !dir.isDirectory()) return out;
    File entry = dir.openNextFile();
    while (entry) {
        out.emplace_back(entry.name());
        entry = dir.openNextFile();
    }
    dir.close();
    return out;
}

size_t Storage::fsFreeBytes() {
    if (!fsMounted_) return 0;
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

size_t Storage::fsTotalBytes() {
    if (!fsMounted_) return 0;
    return LittleFS.totalBytes();
}

// ---- PSRAM ----------------------------------------------------------------
void* Storage::psramAlloc(size_t size) {
    if (size == 0) return nullptr;
#if defined(BOARD_HAS_PSRAM) && (ESP_IDF_VERSION_MAJOR >= 4)
    // Prefer PSRAM when available — `ps_malloc()` returns regular heap
    // pointer if PSRAM is not present, so it's always safe to call.
    if (psramFound()) {
        return ps_malloc(size);
    }
#endif
    return malloc(size);
}

void Storage::psramFree(void* ptr) {
    if (ptr) free(ptr);
}

bool Storage::hasPsram() const {
#if defined(BOARD_HAS_PSRAM)
    return psramFound();
#else
    return false;
#endif
}

}  // namespace phm::hal
