# REST API

> Every HTTP endpoint exposed by the web server. The web UI
> calls these; so can you, from any client that can speak
> HTTP + JSON.

All endpoints are JSON in / JSON out, except `/` and the
static asset paths. Errors are `{"error": "..."}` with the
appropriate HTTP status code.

All endpoints **except `/api/health`** are gated by HTTP
Basic Auth. The default username is `admin` and the default
password is the 16-character string printed to the serial
console on first boot.

---

## Table of contents

- [Conventions](#conventions)
- [`GET /api/health`](#get-apihealth)
- [`GET /api/status`](#get-apistatus)
- [`GET /api/attack/list`](#get-apiattacklist)
- [`POST /api/attack/start`](#post-apiattackstart)
- [`POST /api/attack/stop`](#post-apiattackstop)
- [`GET /api/spectrum`](#get-apispectrum)
- [`GET /api/settings`](#get-apisettings)
- [`POST /api/settings`](#post-apisettings)
- [`GET /api/files`](#get-apifiles)
- [`GET /api/files/{name}`](#get-apifilesname)
- [`DELETE /api/files/{name}`](#delete-apifilesname)
- [`POST /api/cli`](#post-apicli)
- [`POST /api/ota`](#post-apiota)
- [`POST /api/reset`](#post-apireset)
- [Error responses](#error-responses)
- [Next steps](#next-steps)

---

## Conventions

- **Content-Type:** `application/json` for all request and
  response bodies (except `POST /api/ota`, which is
  `multipart/form-data`).
- **Authentication:** every endpoint except `/api/health`
  requires `Authorization: Basic <base64(admin:password)>`.
  Failed auth returns `401` with a `WWW-Authenticate: Basic`
  challenge. After 5 failed attempts in a row, the client
  is locked out for 60 seconds (`429 Too Many Requests`).
- **CORS:** the `Origin` header is checked against a
  whitelist. Only `http://192.168.4.1` and `http://localhost`
  are allowed by default.
- **Rate limit:** the `/api/cli` endpoint is rate-limited to
  10 requests per second per client.
- **Time:** all timestamps are milliseconds since boot.
  Use `Date.now() - serverTime` to compute wall-clock time
  on the client.

## `GET /api/health`

Returns basic device info. **Not auth-gated.**

**Response (200):**

```json
{
  "name": "PhantomRF",
  "version": "0.1.0",
  "board": "ESP32-S3-N16R8",
  "chipModel": "ESP32-S3",
  "chipRev": 1,
  "cpuMhz": 240,
  "freeHeap": 198456,
  "uptimeMs": 312000,
  "tempC": 42.3,
  "vbatMv": 3872,
  "apActive": true,
  "wsClients": 1
}
```

## `GET /api/status`

Returns the current operating state.

**Response (200):**

```json
{
  "state": 1,
  "running": false,
  "currentModule": 0,
  "uptimeMs": 312000,
  "freeHeap": 198456,
  "minFreeHeap": 185320,
  "vbat": 3872,
  "tempC": 42.3,
  "apActive": true,
  "usbConnected": false,
  "wsClients": 1
}
```

The `state` field is one of:
- `0` = Booting
- `1` = Idle
- `2` = Running
- `3` = Stopping
- `4` = Error
- `5` = DeepSleep

## `GET /api/attack/list`

Returns the list of registered attack modules.

**Response (200):**

```json
[
  { "id": "bt",     "name": "Bluetooth Jam", "description": "Disrupt BT classic on all 79 channels" },
  { "id": "ble",    "name": "BLE Spam",      "description": "Trigger Apple / Samsung / Fast Pair popups" },
  { "id": "wifi",   "name": "WiFi Deauth",   "description": "Send forged 802.11 deauth frames" },
  { "id": "beacon", "name": "Beacon Flood",  "description": "Spam SSIDs in beacon frames" },
  { "id": "2_4",    "name": "2.4 GHz Jam",   "description": "nRF24 constant carrier sweep" },
  { "id": "drone",  "name": "Drone Jam",     "description": "Target popular drone bands (2.4 / 5.8)" },
  { "id": "subghz", "name": "Sub-GHz Jam",   "description": "CC1101 spot / range / hopper" },
  { "id": "subrec", "name": "Sub-GHz Record","description": "Capture raw OOK / ASK signal" },
  { "id": "subrep", "name": "Sub-GHz Replay","description": "Transmit last recording" },
  { "id": "spec",   "name": "Spectrum Analyzer","description": "Live RSSI waterfall" }
]
```

## `POST /api/attack/start`

Start an attack.

**Request body:**

```json
{
  "module": "bt",
  "method": "default"
}
```

`module` is one of the `id` values from `/api/attack/list`.
`method` is optional (defaults to the module's default
method).

**Response (200):**

```json
{
  "ok": true,
  "echo": "attack bt default",
  "console": "Starting bt (method=default)...\nRunning."
}
```

**Errors:**

- `400` — missing or invalid `module`.
- `409` — an attack is already running.

## `POST /api/attack/stop`

Stop the current attack.

**Response (200):**

```json
{ "ok": true, "console": "Stopped." }
```

**Errors:**

- `409` — no attack is running.

## `GET /api/spectrum`

Returns the current RSSI snapshot.

**Query parameters:**

- `band` — `"2.4ghz"` (default) or `"subghz"`.

**Response (200):**

```json
{
  "band": "2.4ghz",
  "channels": 126,
  "rssi": [-94, -91, -88, -85, -90, -100, -100, ...]
}
```

The `rssi` array has one entry per channel. Values are in
dBm, ranging from -100 to -40.

## `GET /api/settings`

Returns the current NVS settings (curated subset).

**Response (200):**

```json
{
  "apSsid": "PhantomRF-A3F2",
  "apPasswordSet": true,
  "nrf24Count": 1,
  "nrf24Pa": 0,
  "ccSamplingUs": 150,
  "oledEnabled": true,
  "buttonsConfig": 2,
  "wifiJamMethod": 0,
  "bleMethod": 0,
  "board": "ESP32-S3-N16R8",
  "apActive": true,
  "wsClients": 1
}
```

## `POST /api/settings`

Update NVS settings.

**Request body** (all fields optional):

```json
{
  "apSsid": "PhantomRF Lab",
  "apPassword": "change-me-now",
  "nrf24Count": 1,
  "nrf24Pa": 3,
  "oledEnabled": true,
  "buttonsConfig": 2,
  "wifiJamMethod": 0,
  "bleMethod": 0
}
```

**Response (200):**

```json
{ "ok": true }
```

Only the whitelisted keys are written. Unknown keys are
silently ignored. After the update, the firmware may need to
re-init the affected subsystems (the next attack will pick
up the new settings).

## `GET /api/files`

List files in a directory.

**Query parameters:**

- `path` — directory to list (default: `/recordings`).

**Response (200):**

```json
[
  { "name": "2026-06-19_12-34-56_433.92.sub", "path": "/recordings/2026-06-19_12-34-56_433.92.sub", "size": 32768 },
  { "name": "2026-06-19_13-22-11_433.92.sub", "path": "/recordings/2026-06-19_13-22-11_433.92.sub", "size": 32768 }
]
```

## `GET /api/files/{name}`

Download a file. The response body is the file content; the
`Content-Disposition` header is set to `attachment` with the
original filename.

**Response (200):** `application/octet-stream`

**Errors:**

- `404` — file not found.

## `DELETE /api/files/{name}`

Delete a file.

**Response (200):**

```json
{ "ok": true }
```

**Errors:**

- `404` — file not found.

## `POST /api/cli`

Run a CLI command. The same parser as the USB CDC terminal
is used.

**Request body:**

```json
{ "cmd": "attack bt" }
```

**Response (200):**

```json
{
  "ok": true,
  "cmd": "attack bt",
  "out": "Starting bt (method=default)...\nRunning."
}
```

**Rate limit:** 10 requests/second per client.

**Errors:**

- `400` — missing or invalid `cmd`.

## `POST /api/ota`

Upload a new firmware image. **Multipart/form-data** with a
single `firmware` field.

```bash
curl -X POST -u admin:password \
  -F "firmware=@phantomrf-0.2.0.bin" \
  http://192.168.4.1/api/ota
```

**Response (200):**

```json
{ "ok": true, "size": 1572864 }
```

After the upload, the device reboots into the new firmware.
The response is sent before the reboot; the connection will
drop after the device restarts.

> :warning: **An OTA flash overwrites the current firmware.**
> Make sure you've saved anything you need to keep (e.g.
> custom settings, recordings) before triggering an OTA.

## `POST /api/reset`

Factory reset: clears the NVS namespace and reboots.

**Response (200):**

```json
{ "ok": true }
```

The response is sent before the reboot. The device will
restart with default settings and a freshly-generated AP
password (printed to the serial console on first boot).

> :warning: **Destructive.** All custom settings are lost.
> Recordings on LittleFS are preserved (they're not in
> NVS).

## Error responses

All error responses use a JSON body of the form:

```json
{ "error": "human-readable message" }
```

Common status codes:

| Status | Meaning |
|---|---|
| `400` | Bad Request — malformed JSON, missing field |
| `401` | Unauthorized — missing or invalid credentials |
| `404` | Not Found — endpoint or file doesn't exist |
| `409` | Conflict — operation not allowed in current state |
| `413` | Payload Too Large — body exceeds 1 KB |
| `429` | Too Many Requests — auth lockout or rate limit |
| `500` | Internal Server Error — bug in the firmware |
| `503` | Service Unavailable — subsystem not ready |

The server always includes a `Server: PhantomRF/0.1.0`
header so the client can identify the firmware version.

## Next steps

- The CLI equivalents of these endpoints are in
  [commands.md](commands.md).
- The push protocol (status, spectrum, log) is in
  [websocket-protocol.md](websocket-protocol.md).
- The NVS settings (what you can change) are in
  [settings.md](settings.md).
