/**
 * @file WebServer.h
 * @brief WiFi AP + ESPAsyncWebServer + WebSocket + captive portal
 *
 * The web UI is the primary control surface for PhantomRF. This module:
 *   1. Brings up a WiFi SoftAP (default SSID "PhantomRF-XXXX", per-device
 *      random password unless overridden by NVS).
 *   2. Starts a DNS server on port 53 to answer every name with the device IP
 *      — this triggers the captive portal flow on Android, iOS, ChromeOS
 *      and most desktop OSes.
 *   3. Mounts the LittleFS web assets at `/web/` and serves them at `/`.
 *   4. Registers the REST API endpoints documented in DESIGN §11.2.
 *   5. Registers the WebSocket at `/ws` (PhantomRF WebSocket v1, DESIGN §20.5).
 *   6. Gates everything behind HTTP Basic Auth (DESIGN §20.2) with a
 *      brute-force lockout (5 fails / 60 s).
 *   7. Streams the live spectrum (1 Hz), status (1 Hz) and log events to
 *      subscribed WS clients.
 *
 * The module is intentionally framework-thin. The actual UI lives in
 * `web/` (vanilla JS + a tiny CSS) and is served from LittleFS. The
 * device never calls out to the internet.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <AsyncTCP.h>

#include "core/Module.h"

namespace phm::ui {

/**
 * @brief HTTP + WebSocket server fronted by a WiFi SoftAP
 *
 * Singleton registered with the `PhantomRF` core. Lifecycle:
 *   - `setup()` starts the AP, DNS, HTTP server, WebSocket and timers.
 *   - `loop()` pumps the DNS server, polls the WS for new frames, and
 *     pushes the periodic status / spectrum broadcasts.
 *   - `teardown()` shuts everything down (called before OTA reboot).
 */
class WebServer : public ::phm::IModule {
public:
    /// Stable module id, used by the event bus and API to identify us
    static constexpr uint8_t MODULE_ID = 0xE0;

    // ---- IModule ---------------------------------------------------------
    const char* name()        const override { return "WebServer"; }
    const char* description() const override { return "WiFi AP + Web UI + WebSocket + REST API"; }
    uint8_t     id()          const override { return MODULE_ID; }
    void setup()    override;
    void loop()     override;
    void teardown() override;

    // ---- Broadcasting ----------------------------------------------------

    /// Send a JSON document to every connected WebSocket client.
    /// The caller owns the document; this method serialises a copy.
    void broadcast(const JsonDocument& doc);

    /// Convenience: build a `spectrum` push frame and broadcast.
    /// `rssi` must be at least `len` bytes long. The data is copied.
    void broadcastSpectrum(const char* band, const int8_t* rssi, size_t len);

    /// Convenience: build a `status` push frame and broadcast (1 Hz).
    void broadcastStatus();

    /// Convenience: build a `log` push frame and broadcast.
    void broadcastLog(const char* level, const char* msg);

    /// Build a `attack_state` push frame and broadcast.
    void broadcastAttackState(const char* state, const char* module, const char* error = nullptr);

    // ---- AP management ---------------------------------------------------

    /// Start the SoftAP with the given credentials. If `password` is empty
    /// the AP is open. SSID is truncated to 32 chars (WPA2 spec).
    void startAp(const char* ssid, const char* password);

    /// Stop the SoftAP and tear down dependent services.
    void stopAp();

    /// True while the SoftAP is up
    bool isApActive() const;

    /// Number of currently connected WebSocket clients
    size_t wsClientCount() const;

    /// Read the current AP password (may be a freshly generated one).
    /// Pointer is to internal storage; valid until the next startAp() call.
    const char* apPassword() const { return apPassword_; }

    /// Read the current AP SSID (same lifetime caveat as apPassword()).
    const char* apSsid() const { return apSsid_; }

private:
    // ---- Setup helpers ---------------------------------------------------
    void setupRoutes();
    void setupCaptivePortal();
    void setupWebSocket();
    void setupAuth();
    void loadApCredentials();
    void generateRandomApPassword();
    void deriveMacSsid(char* out, size_t outLen);

    // ---- REST handlers ---------------------------------------------------
    void handleHealth(AsyncWebServerRequest* req);
    void handleStatus(AsyncWebServerRequest* req);
    void handleAttackList(AsyncWebServerRequest* req);
    void handleAttackStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void handleAttackStop(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void handleSpectrum(AsyncWebServerRequest* req);
    void handleSettings(AsyncWebServerRequest* req);
    void handleSettingsUpdate(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void handleFiles(AsyncWebServerRequest* req);
    void handleFileDownload(AsyncWebServerRequest* req, const String& name);
    void handleFileDelete(AsyncWebServerRequest* req, const String& name);
    void handleCli(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void handleOta(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
    void handleReset(AsyncWebServerRequest* req);

    // ---- Auth & rate limiting -------------------------------------------
    bool checkAuth(AsyncWebServerRequest* req);
    void onAuthFail(AsyncWebServerRequest* req);
    bool isLockedOut() const;
    void noteFailedAuth();
    void noteSuccessfulAuth();

    // ---- WebSocket event sink -------------------------------------------
    void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                   void* arg, uint8_t* data, size_t len);

    // ---- Misc helpers ----------------------------------------------------
    void sendJson(AsyncWebServerRequest* req, int code, const JsonDocument& doc);
    void sendError(AsyncWebServerRequest* req, int code, const char* msg);
    String urlDecode(const String& src);

    // ---- Networking state ------------------------------------------------
    AsyncWebServer  server_{80};                ///< HTTP server
    AsyncWebSocket  ws_{"/ws"};                 ///< WebSocket endpoint
    DNSServer       dns_;                       ///< Captive-portal DNS

    // ---- AP state --------------------------------------------------------
    char  apSsid_[33]      = {0};
    char  apPassword_[65]  = {0};
    bool  apActive_        = false;
    bool  dnsStarted_      = false;

    // ---- Rate limiting (in RAM, DESIGN §20.2) ---------------------------
    static constexpr uint8_t  kMaxFailedAttempts = 5;
    static constexpr uint32_t kLockoutMs         = 60UL * 1000UL;
    uint8_t  failedAttempts_   = 0;
    uint32_t lockoutUntilMs_   = 0;

    // ---- Push timers (driven from loop()) --------------------------------
    uint32_t lastStatusPushMs_   = 0;
    uint32_t lastSpectrumPushMs_ = 0;
    uint32_t seqCounter_         = 0;

    // ---- Server time (for `ts` field) ------------------------------------
    uint32_t bootTimeMs_ = 0;
};

/// Singleton instance, defined in WebServer.cpp
extern WebServer g_webServer;

}  // namespace phm::ui
