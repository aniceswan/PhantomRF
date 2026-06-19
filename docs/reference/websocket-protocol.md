# WebSocket protocol

> PhantomRF WebSocket v1 — the push protocol used by the web
> UI to receive live status, spectrum, and log updates. Same
> protocol on the USB CDC and BLE Serial channels (sort of;
> see the cross-transport notes at the end).

The WebSocket endpoint is `ws://192.168.4.1/ws`. The
handshake is a standard HTTP 101 upgrade. After the upgrade,
the client and server exchange JSON text frames. Binary
frames are reserved for the high-throughput spectrum mode
(M1).

---

## Table of contents

- [Connection lifecycle](#connection-lifecycle)
- [Message format](#message-format)
- [Client → server messages](#client--server-messages)
- [Server → client messages](#server--client-messages)
- [Topic subscription](#topic-subscription)
- [Example session](#example-session)
- [Bandwidth budget](#bandwidth-budget)
- [JavaScript client library](#javascript-client-library)
- [Cross-transport notes](#cross-transport-notes)
- [Next steps](#next-steps)

---

## Connection lifecycle

1. **Client opens** the WebSocket at `ws://192.168.4.1/ws`
   (or `wss://...` if HTTPS is enabled). Standard HTTP 101
   upgrade; no custom headers required.
2. **Server sends `hello`** as the first frame after the
   upgrade succeeds. This tells the client the protocol
   version, the server's uptime, and the device's
   capabilities.
3. **Client subscribes** to topics with a `subscribe` frame.
   Until the client subscribes, no periodic push frames
   (spectrum, status) are sent. (The `hello` frame is
   always sent.)
4. **Server pushes** frames for each subscribed topic at
   the topic's rate (1 Hz for status, 1 Hz for spectrum in
   M0; see Bandwidth budget).
5. **Either side can ping** with a `{"type":"ping"}` frame.
   The other side replies with `{"type":"pong"}` within
   100 ms.
6. **Either side can close** the WebSocket at any time. The
   server cleans up the client and stops pushing frames to
   it.

Reconnection is the client's responsibility. The
recommended backoff is exponential (1 s, 2 s, 4 s, 8 s, max
30 s) with a ±20% jitter.

## Message format

All frames are JSON, UTF-8 encoded. The wire format is:

```json
{
  "type": "<message-type>",
  "seq": 1234,
  "ts": 1234567890,
  "data": { ... }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | yes | Message type (see below) |
| `seq` | integer | optional | Monotonically increasing counter for ordering |
| `ts` | integer | optional | Server timestamp in ms since boot |
| `data` | object | depends | Type-specific payload |

Unknown `type` values are ignored by both sides (forward
compatibility).

## Client → server messages

| `type` | `data` shape | Description |
|---|---|---|
| `subscribe`   | `{ "topics": ["spectrum","status","log"] }` | Subscribe to topics |
| `unsubscribe` | `{ "topics": ["spectrum"] }` | Unsubscribe |
| `ping`        | `{ "clientTs": 12345 }` | Keepalive; server replies with `pong` |
| `command`     | `{ "cmd": "attack bt", "args": [] }` | Run a CLI command |
| `ack`         | `{ "seq": 1234 }` | Acknowledge a server message (e.g. `attack_state`) |

### `subscribe`

The client should send this immediately after the `hello`
frame. The server starts pushing frames for the listed
topics; topics not in the list are not pushed.

```json
{
  "type": "subscribe",
  "data": {
    "topics": ["spectrum", "status", "log", "attack_state"]
  }
}
```

### `command`

Runs a CLI command (the same command set as the USB CDC
terminal). The server replies with an `ack` containing the
sequence number, then a `log` frame for each line of
output.

```json
{
  "type": "command",
  "data": {
    "cmd": "attack bt"
  }
}
```

## Server → client messages

| `type` | Rate | `data` shape | Size |
|---|---|---|---|
| `hello`        | once | `{ version, serverTime, deviceId, board, firmware, capabilities }` | ~150 B |
| `spectrum`     | 1 Hz (M0), 10 Hz (M1) | `{ band, rssi: int[126], ts }` | 380 B (M0 JSON) |
| `status`       | 1 Hz | `{ running, state, currentAttack, freeHeap, minFreeHeap, uptime, rssiAp, vbat, tempC, usbConnected, wsClients }` | 200 B |
| `attack_state` | on event | `{ state, module, error? }` | 100 B |
| `log`          | event-driven | `{ level, msg, source }` | 100 B avg |
| `pong`         | on ping | `{ clientTs }` | 30 B |
| `error`        | on error | `{ code, msg }` | 100 B |

### `hello`

The first frame after the upgrade. Sent exactly once per
connection.

```json
{
  "type": "hello",
  "seq": 1,
  "ts": 0,
  "data": {
    "version": 1,
    "serverTime": 1234,
    "deviceId": "PhantomRF",
    "board": "ESP32-S3-N16R8",
    "firmware": "0.1.0",
    "capabilities": "spectrum,status,log,attack_state"
  }
}
```

The `capabilities` field is a comma-separated list of
topics the server can push. A client that subscribes to a
topic not in the list will not receive frames for it
(but no error is returned).

### `spectrum`

Live RSSI snapshot. The `rssi` array has one entry per
channel in the requested band.

```json
{
  "type": "spectrum",
  "seq": 42,
  "ts": 1234,
  "data": {
    "band": "2.4ghz",
    "rssi": [-94, -91, -88, -85, -90, -100, -100, ...],
    "ts": 1234
  }
}
```

Values are in dBm, ranging from -100 to -40. The array
length is 126 for 2.4 GHz (nRF24 channels 0–125) and
variable for sub-GHz (CC1101 supports up to 256 channels
in the configured band).

In M0, the spectrum is JSON. In M1, it switches to a
binary frame for higher throughput:

```
[band: 1 byte (0=2.4ghz, 1=subghz)]
[channel_count: 2 bytes (little-endian)]
[rssi: int8_t × channel_count]
```

The binary frame is the same wire format, just without the
JSON overhead (≈ 130 bytes vs. 380 bytes for 2.4 GHz).

### `status`

1 Hz heartbeat with the current device state.

```json
{
  "type": "status",
  "seq": 43,
  "ts": 1500,
  "data": {
    "running": false,
    "state": 1,
    "currentAttack": 0,
    "freeHeap": 198456,
    "minFreeHeap": 185320,
    "uptimeMs": 312000,
    "rssiAp": 1,
    "vbat": 3872,
    "tempC": 42.3,
    "usbConnected": false,
    "wsClients": 1
  }
}
```

The `state` field is the integer value of the
`phm::State` enum. See [api.md](api.md#get-apistatus) for
the values.

### `attack_state`

Sent when an attack starts, stops, or fails.

```json
{
  "type": "attack_state",
  "seq": 44,
  "ts": 2000,
  "data": {
    "state": "running",
    "module": "bt"
  }
}
```

The `state` field is one of: `running`, `stopped`,
`failed`. The `module` field is the attack's short name
(bt, ble, wifi, etc.).

If the attack failed, an `error` field is included:

```json
{
  "type": "attack_state",
  "data": {
    "state": "failed",
    "module": "wifi",
    "error": "CC1101 SPI timeout"
  }
}
```

### `log`

A log message. The `level` field is one of `verbose`,
`debug`, `info`, `warn`, `error`. The `source` field is
the module tag (e.g. `web`, `cli`, `nrf24`).

```json
{
  "type": "log",
  "seq": 45,
  "ts": 2010,
  "data": {
    "level": "info",
    "msg": "nRF24 carrier started on channel 76",
    "source": "nrf24"
  }
}
```

The M0 firmware forwards log messages from the ring buffer
to all connected WS clients. M1 adds a per-client level
filter.

## Topic subscription

The M0 firmware sends every topic to every connected
client. M1 adds per-client topic tracking: a client only
receives frames for the topics it's subscribed to.

To subscribe:

```json
{ "type": "subscribe", "data": { "topics": ["spectrum", "status"] } }
```

To unsubscribe:

```json
{ "type": "unsubscribe", "data": { "topics": ["spectrum"] } }
```

Unknown topics are silently ignored. A client can re-subscribe
at any time to update its topic set.

## Example session

```
   Client                                Server
     |                                      |
     |  ---- HTTP GET /ws (Upgrade) ---->   |
     |  <---- 101 Switching Protocols ----   |
     |                                      |
     |  <---- { "type": "hello", ... } ---  |
     |                                      |
     |  ---- { "type": "subscribe",  --->   |
     |         "topics": ["status",         |
     |                  "spectrum"] }       |
     |                                      |
     |  <---- { "type": "status", ... } --  |
     |  <---- { "type": "spectrum", ... } -  |
     |  <---- { "type": "status", ... } --  |
     |  <---- { "type": "spectrum", ... } -  |
     |                                      |
     |  ---- { "type": "ping" } --------->  |
     |  <---- { "type": "pong" } ---------  |
     |                                      |
     |  ---- { "type": "command", -------->  |
     |         "cmd": "attack bt" }         |
     |  <---- { "type": "ack", "seq":N } --  |
     |  <---- { "type": "log", ... } -----  |
     |  <---- { "type": "attack_state",     |
     |          "state": "running",         |
     |          "module": "bt" }            |
     |  <---- { "type": "status", ... } --  |
     |                                      |
     |  <---- close ----------------------  |
     X                                      X
```

## Bandwidth budget

The M0 firmware uses JSON frames. Per frame:

| Topic | Rate | Frame size | Bandwidth |
|---|---|---|---|
| `status` | 1 Hz | 200 B | 200 B/s |
| `spectrum` (2.4 GHz) | 1 Hz | 380 B | 380 B/s |
| `spectrum` (sub-GHz) | 1 Hz | 600 B | 600 B/s |
| `log` | ≈ 1/s | 100 B | 100 B/s |
| `ping/pong` | 1/s combined | 50 B | 50 B/s |
| **Total** | | | **≈ 1.3 KB/s** |

Well within the WiFi capacity of the SoftAP (≈ 1 MB/s).

M1 increases spectrum to 10 Hz and switches to binary
frames:

| Topic | Rate | Frame size | Bandwidth |
|---|---|---|---|
| `spectrum` (binary, 2.4 GHz) | 10 Hz | 130 B | 1.3 KB/s |
| **New total** | | | **≈ 2 KB/s** |

Still trivial.

## JavaScript client library

A minimal client (works in any modern browser):

```javascript
class PhantomRFClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.handlers = new Map();
        this.seq = 0;
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);
            this.ws.onopen = () => {
                this.send({ type: 'subscribe', data: { topics: ['spectrum', 'status', 'log', 'attack_state'] } });
                resolve();
            };
            this.ws.onmessage = (e) => {
                const msg = JSON.parse(e.data);
                const h = this.handlers.get(msg.type);
                if (h) h(msg);
            };
            this.ws.onerror = reject;
        });
    }

    send(obj) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(obj));
        }
    }

    on(type, fn) {
        this.handlers.set(type, fn);
    }
}

// Usage:
const c = new PhantomRFClient('ws://192.168.4.1/ws');
await c.connect();
c.on('spectrum', m => console.log('rssi', m.data.rssi));
c.on('status',   m => console.log('temp', m.data.tempC));
c.on('log',      m => console.log(`[${m.data.level}] ${m.data.msg}`));
```

The web UI uses a slightly more sophisticated version that
handles reconnection, batching, and binary frames.

## Cross-transport notes

The same command set is available on three transports:

| Transport | Endpoint | Format |
|---|---|---|
| USB CDC | `/dev/ttyACM0` (Serial at 115200 8N1) | Line-oriented text |
| Web UI | `ws://192.168.4.1/ws` | JSON frames (this page) |
| BLE Serial | `NimBLE GATT`, characteristic `phm.cli` | Line-oriented text |

The WebSocket transport is **push-based** (server-initiated);
the USB CDC and BLE transports are **pull-based**
(client-initiated). To send a command over WebSocket, use
the `command` frame; over USB CDC or BLE, just type the
command and press Enter.

The M1 firmware adds a `command` response frame to the
WebSocket, so the client can see the command's output.

## Next steps

- The REST API (request/response only, no push) is in
  [api.md](api.md).
- The CLI commands (the things you can send) are in
  [commands.md](commands.md).
- The NVS settings (what the server's state actually is)
  are in [settings.md](settings.md).
