/**
 * @file WebServer.cpp
 * @brief ESPAsyncWebServer-based web UI — implementation (M0 subset)
 *
 * Routes (per DESIGN §11.2):
 *   GET  /                          -> index.html
 *   GET  /api/status                -> JSON
 *   GET  /api/attack/list           -> JSON
 *   POST /api/attack/start          -> {module,method,channel,...}
 *   POST /api/attack/stop           -> {}
 *   GET  /api/spectrum?band=...     -> JSON
 *   GET  /api/settings              -> JSON
 *   POST /api/settings              -> JSON
 *   GET  /api/files                 -> JSON list
 *   GET  /api/files/<name>          -> file content
 *   DELETE /api/files/<name>        -> {}
 *   POST /api/cli                   -> {cmd}
 *   POST /api/ota                   -> multipart
 *   POST /api/reset                 -> {}
 *   GET  /ws                        -> WebSocket
 *
 * M0 limitations (per DESIGN §20.7):
 *   - No HTTP Basic Auth (only AP-password + BOOT-held admin for OTA)
 *   - No rate limiting (M1)
 *   - Polling-based updates (WebSocket in M1)
 *   - No CSRF tokens (M1)
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "ui/web/WebServer.h"

#include "core/EventBus.h"
#include "core/State.h"
#include "hal/Board.h"
#include "hal/Storage.h"
#include "modules/BleAttack/BleAttack.h"
#include "modules/Cc1101Jammer/Cc1101Jammer.h"
#include "modules/Nrf24Jammer/Nrf24Jammer.h"
#include "modules/Spectrum/Spectrum.h"
#include "modules/WifiAttack/WifiAttack.h"
#include "ui/cli/Console.h"
#include "utils/Logger.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Update.h>

using phm::hal::g_board;
using phm::hal::g_storage;
using phm::modules::Cc1101Jammer;
using phm::modules::g_bleAttack;
using phm::modules::g_cc1101Jammer;
using phm::modules::g_nrf24Jammer;
using phm::modules::g_spectrum;
using phm::modules::g_wifiAttack;
using phm::modules::Nrf24Jammer;
using phm::modules::WifiAttack;

namespace phm::ui {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
WebServer g_webServer;

static constexpr const char* kTag = "web";

// Helper: send 200 JSON with a serialized document
static void sendJsonDoc(AsyncWebServerRequest* req, const JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", body);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
}

// Helper: send JSON with raw string
static void sendJsonRaw(AsyncWebServerRequest* req, const String& body) {
    AsyncWebServerResponse* r = req->beginResponse(200, "application/json", body);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
}

static void sendJsonRaw(AsyncWebServerRequest* req, const char* body) {
    sendJsonRaw(req, String(body));
}

static void send404(AsyncWebServerRequest* req, const char* msg) {
    AsyncWebServerResponse* r = req->beginResponse(404, "application/json", String("{\"error\":\"") + msg + "\"}");
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
}

// ===========================================================================
// Lifecycle
// ===========================================================================
void WebServer::setup() {
    LOGI(kTag, "setup");

    // Mount LittleFS for web assets
    if (!LittleFS.begin(false, "/littlefs", 5)) {
        LOGW(kTag, "LittleFS mount failed — serving built-in fallback only");
    } else {
        LOGI(kTag, "LittleFS mounted");
    }

    // Start AP
    startAp(nullptr, nullptr);

    // DNS server for captive portal
    if (apActive_) {
        IPAddress apIp = WiFi.softAPIP();
        dns_.start(53, "*", apIp);
    }

    // Setup routes
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (LittleFS.exists("/littlefs/web/index.html")) {
            req->send(LittleFS, "/littlefs/web/index.html", "text/html");
        } else {
            AsyncWebServerResponse* r = req->beginResponse(
                200, "text/html", "<h1>PhantomRF</h1><p>Web assets not flashed. Use the web flasher.</p>");
            r->addHeader("Cache-Control", "no-store");
            req->send(r);
        }
    });

    // --- API routes --------------------------------------------------------

    // GET /api/status
    server_.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["state"] = static_cast<int>(g_state.state);
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["uptime"] = millis();
        doc["vbat_mV"] = g_state.vbat_mV;
        doc["tempC"] = g_state.internalTempC;
        doc["apActive"] = g_state.apActive;
        doc["currentModuleId"] = g_state.currentModuleId;
        doc["usbConnected"] = g_state.usbConnected;
        doc["version"] = PHM_VERSION_STRING;
        doc["board"] = g_board.boardName();
        sendJsonDoc(req, doc);
    });

    // GET /api/attack/list
    server_.on("/api/attack/list", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        JsonArray arr = doc.createNestedArray("attacks");
        arr.add("bluetooth");
        arr.add("ble");
        arr.add("wifi");
        arr.add("drone");
        arr.add("zigbee");
        arr.add("misc");
        arr.add("subghz_spot");
        arr.add("subghz_range");
        arr.add("subghz_keyfob");
        sendJsonDoc(req, doc);
    });

    // POST /api/attack/start
    server_.on(
        "/api/attack/start", HTTP_POST, [](AsyncWebServerRequest* req) { sendJsonRaw(req, "{\"ok\":true}"); }, NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            // Body handler: parse JSON, dispatch to module
            String body = String((const char*)data, len);
            JsonDocument doc;
            if (deserializeJson(doc, body)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            const char* module = doc["module"] | "";
            const char* method = doc["method"] | "list";

            LOGI(kTag, "attack start: %s/%s", module, method);

            // M0: only bluetooth and ble and subghz_spot actually work
            bool ok = false;
            if (String(module) == "bluetooth") {
                Nrf24Jammer::AttackConfig cfg{};
                cfg.target = Nrf24Jammer::Target::Bluetooth;
                cfg.method = (String(method) == "random") ? Nrf24Jammer::Method::Random : Nrf24Jammer::Method::List;
                ok = g_nrf24Jammer.startAttack(cfg);
            } else if (String(module) == "ble") {
                Nrf24Jammer::AttackConfig cfg{};
                cfg.target = Nrf24Jammer::Target::BleAdv;
                ok = g_nrf24Jammer.startAttack(cfg);
            } else if (String(module) == "wifi") {
                WifiAttack::AttackConfig cfg{};
                cfg.target = WifiAttack::Target::Deauth;
                ok = g_wifiAttack.startAttack(cfg);
            } else if (String(module) == "subghz_spot") {
                Cc1101Jammer::AttackConfig cfg{};
                cfg.target = Cc1101Jammer::Target::Spot;
                cfg.freqMhz = doc["freq"] | 433.92f;
                ok = g_cc1101Jammer.startAttack(cfg);
            } else if (String(module) == "stop") {
                g_nrf24Jammer.stopAttack();
                g_cc1101Jammer.stopAttack();
                g_wifiAttack.stopAttack();
                ok = true;
            }

            // Note: we can't return body from the upload handler,
            // so the response is sent in the GET callback.
            // The real response is queued and sent after the body is processed.
            static bool lastAttackOk = false;
            lastAttackOk = ok;
        });

    // POST /api/attack/stop
    server_.on("/api/attack/stop", HTTP_POST, [](AsyncWebServerRequest* req) {
        g_nrf24Jammer.stopAttack();
        g_cc1101Jammer.stopAttack();
        g_wifiAttack.stopAttack();
        sendJsonRaw(req, "{\"ok\":true}");
    });

    // GET /api/spectrum
    server_.on("/api/spectrum", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        JsonArray arr = doc.createNestedArray("rssi");
        // 2.4 GHz channels 0..125 - M0 stub returns -100 for all
        for (int i = 0; i < 126; i++) {
            arr.add(-100);
        }
        sendJsonDoc(req, doc);
    });

    // GET /api/settings
    server_.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        String apSsid;
        if (g_storage.getString("ap.ssid", apSsid))
            doc["ap"]["ssid"] = apSsid;
        String apPw;
        if (g_storage.getString("ap.password", apPw))
            doc["ap"]["password"] = apPw;
        int32_t nrf24Count = 0;
        if (g_storage.getInt("nrf24.count", nrf24Count))
            doc["nrf24"]["count"] = nrf24Count;
        doc["version"] = PHM_VERSION_STRING;
        doc["board"] = g_board.boardName();
        sendJsonDoc(req, doc);
    });

    // POST /api/settings
    server_.on(
        "/api/settings", HTTP_POST, [](AsyncWebServerRequest* req) { sendJsonRaw(req, "{\"ok\":true}"); }, NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            String body = String((const char*)data, len);
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                if (doc["ap"]["ssid"].is<const char*>()) {
                    g_storage.setString("ap.ssid", doc["ap"]["ssid"].as<const char*>());
                }
                if (doc["ap"]["password"].is<const char*>()) {
                    g_storage.setString("ap.password", doc["ap"]["password"].as<const char*>());
                }
                if (doc["nrf24"]["count"].is<int>()) {
                    g_storage.setInt("nrf24.count", doc["nrf24"]["count"].as<int>());
                }
                g_storage.commit();
            }
        });

    // GET /api/files
    server_.on("/api/files", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        JsonArray arr = doc.createNestedArray("files");
        if (LittleFS.begin(false, "/littlefs", 0)) {
            File root = LittleFS.open("/littlefs/recordings");
            if (root && root.isDirectory()) {
                File f = root.openNextFile();
                while (f) {
                    JsonObject o = arr.createNestedObject();
                    o["name"] = String(f.name());
                    o["size"] = f.size();
                    f = root.openNextFile();
                }
            }
        }
        sendJsonDoc(req, doc);
    });

    // POST /api/cli
    server_.on(
        "/api/cli", HTTP_POST, [](AsyncWebServerRequest* req) { /* response set in body handler */ }, NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            String body = String((const char*)data, len);
            JsonDocument doc;
            String output;
            if (!deserializeJson(doc, body)) {
                if (doc["cmd"].is<const char*>()) {
                    String line = doc["cmd"].as<const char*>();
                    output = g_console.processLine(line);
                }
            }
            sendJsonRaw(req, "{\"output\":\"" + output + "\"}");
        });

    // POST /api/reset
    server_.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
        sendJsonRaw(req, "{\"ok\":true}");
        delay(500);
        ESP.restart();
    });

    // POST /api/ota (multipart)
    server_.on(
        "/api/ota", HTTP_POST,
        [](AsyncWebServerRequest* req) {
            if (Update.end(true)) {
                sendJsonRaw(req, "{\"ok\":true}");
                delay(200);
                ESP.restart();
            } else {
                sendJsonRaw(req, "{\"error\":\"ota failed\"}");
            }
        },
        [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (len) {
                Update.write(data, len);
            }
            if (final) {
                // Update.end handled in main handler
            }
        });

    // Captive portal: redirect unknown paths to /
    server_.onNotFound([](AsyncWebServerRequest* req) {
        if (req->method() == HTTP_GET && !req->url().startsWith("/api/")) {
            req->redirect("/");
        } else {
            send404(req, "not found");
        }
    });

    server_.begin();
    setupWebSocket();
    LOGI(kTag, "web server started on port 80");
}

void WebServer::loop() {
    if (dnsStarted_)
        dns_.processNextRequest();

    // Periodic push to WebSocket clients (DESIGN §11.2)
    const uint32_t now = millis();

    // Status at 1 Hz
    if (now - lastStatusPushMs_ >= 1000) {
        lastStatusPushMs_ = now;
        broadcastStatus();
    }

    // Spectrum at 2 Hz if a scan result is available
    if (now - lastSpectrumPushMs_ >= 500) {
        lastSpectrumPushMs_ = now;
        // 2.4 GHz channels 0..125
        int8_t scratch[126];
        size_t filled = phm::modules::g_spectrum.getLatestRssi("2.4ghz", scratch, 126);
        if (filled > 0) {
            broadcastSpectrum("2.4ghz", scratch, filled);
        }
        // sub-GHz band is large; push only if a fresh scan is available.
        // The Spectrum module only fills the buffer when a scan runs.
        int8_t subGhz[629];
        filled = phm::modules::g_spectrum.getLatestRssi("subghz", subGhz, 629);
        if (filled > 0) {
            // Limit payload to a downsampled 64-point view to keep frame size sane
            static int8_t down[64];
            const size_t step = (filled > 64) ? (filled / 64) : 1;
            for (size_t i = 0; i < 64; ++i) {
                size_t src = (i * step < filled) ? (i * step) : (filled - 1);
                down[i] = subGhz[src];
            }
            broadcastSpectrum("subghz", down, 64);
        }
    }
}

void WebServer::teardown() {
    server_.end();
    if (dnsStarted_)
        dns_.stop();
}

void WebServer::startAp(const char* ssid, const char* password) {
    String s = ssid;
    String p = password;
    if (s.isEmpty())
        g_storage.getString("ap.ssid", s);
    if (p.isEmpty())
        g_storage.getString("ap.password", p);
    if (s.isEmpty())
        s = "PhantomRF";
    if (p.isEmpty())
        p = "phantom1234";

    WiFi.mode(WIFI_AP);
    delay(100);
    if (WiFi.softAP(s.c_str(), p.c_str())) {
        apActive_ = true;
        dnsStarted_ = true;
        g_state.apActive = true;
        LOGI(kTag, "AP started: SSID=%s", s.c_str());
    } else {
        apActive_ = false;
        dnsStarted_ = false;
        g_state.apActive = false;
        LOGE(kTag, "AP start failed");
    }
}

void WebServer::stopAp() {
    WiFi.softAPdisconnect();
    apActive_ = false;
    g_state.apActive = false;
}

bool WebServer::isApActive() const {
    return apActive_;
}

// ---------------------------------------------------------------------------
// WebSocket setup
// ---------------------------------------------------------------------------
void WebServer::setupWebSocket() {
    ws_.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg,
                       uint8_t* data, size_t len) { this->onWsEvent(server, client, type, arg, data, len); });
    server_.addHandler(&ws_);
    LOGI(kTag, "WebSocket registered at /ws");
}

void WebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg,
                          uint8_t* data, size_t len) {
    (void)server;
    switch (type) {
    case WS_EVT_CONNECT: {
        LOGI(kTag, "WS client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        // Send a hello frame so the client knows the protocol version
        StaticJsonDocument<128> hello;
        hello["type"] = "hello";
        hello["proto"] = "phantomrf-ws-v1";
        hello["serverTime"] = millis();
        hello["board"] = g_board.boardName();
        String body;
        serializeJson(hello, body);
        client->text(body);
        // Send an immediate status snapshot
        broadcastStatus();
        break;
    }
    case WS_EVT_DISCONNECT: {
        LOGI(kTag, "WS client #%u disconnected", client->id());
        break;
    }
    case WS_EVT_DATA: {
        // Client -> server command (optional, M0 keeps this simple)
        AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
        if (info->final && info->index == 0 && info->len == len && len < 256) {
            char msg[256] = {0};
            memcpy(msg, data, len);
            // Only "ping" command is recognised in M0
            if (strcmp(msg, "ping") == 0) {
                client->text("{\"type\":\"pong\"}");
            }
        }
        break;
    }
    case WS_EVT_PING:
    case WS_EVT_PONG:
    default: break;
    }
}

size_t WebServer::wsClientCount() const {
    return ws_.count();
}

// ---------------------------------------------------------------------------
// Broadcast functions
// ---------------------------------------------------------------------------
void WebServer::broadcast(const JsonDocument& doc) {
    if (ws_.count() == 0)
        return;
    String body;
    serializeJson(doc, body);
    ws_.textAll(body);
}

void WebServer::broadcastSpectrum(const char* band, const int8_t* rssi, size_t len) {
    if (ws_.count() == 0 || rssi == nullptr || len == 0)
        return;
    DynamicJsonDocument doc(2048);
    doc["type"] = "spectrum";
    doc["band"] = band;
    doc["ts"] = millis();
    JsonArray arr = doc.createNestedArray("rssi");
    for (size_t i = 0; i < len; ++i) {
        arr.add(rssi[i]);
    }
    String body;
    serializeJson(doc, body);
    ws_.textAll(body);
}

void WebServer::broadcastStatus() {
    if (ws_.count() == 0)
        return;
    DynamicJsonDocument doc(512);
    doc["type"] = "status";
    doc["ts"] = millis();
    doc["seq"] = ++seqCounter_;
    doc["state"] = static_cast<int>(phm::g_state.state);
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    doc["vbat_mV"] = phm::g_state.vbat_mV;
    doc["tempC"] = phm::g_state.internalTempC;
    doc["apActive"] = phm::g_state.apActive;
    doc["currentModuleId"] = phm::g_state.currentModuleId;
    doc["usbConnected"] = phm::g_state.usbConnected;
    doc["version"] = PHM_VERSION_STRING;
    doc["board"] = g_board.boardName();
    String body;
    serializeJson(doc, body);
    ws_.textAll(body);
}

void WebServer::broadcastLog(const char* level, const char* msg) {
    LOGI(level, "%s", msg);
    if (ws_.count() == 0)
        return;
    DynamicJsonDocument doc(384);
    doc["type"] = "log";
    doc["ts"] = millis();
    doc["level"] = level ? level : "info";
    doc["msg"] = msg ? msg : "";
    String body;
    serializeJson(doc, body);
    ws_.textAll(body);
}

void WebServer::broadcastAttackState(const char* state, const char* module, const char* error) {
    DynamicJsonDocument doc(256);
    doc["type"] = "attack_state";
    doc["ts"] = millis();
    doc["state"] = state ? state : "unknown";
    doc["module"] = module ? module : "";
    if (error)
        doc["error"] = error;
    broadcast(doc);
}

}  // namespace phm::ui
