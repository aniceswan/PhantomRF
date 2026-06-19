# WiFi deauth

> Send forged 802.11 management frames to disconnect clients from
> an access point. The most popular PhantomRF attack and the
> easiest to demonstrate.

The 802.11 standard has a fundamental design flaw: management
frames (including deauthentication) are **unauthenticated**. Any
device on the same channel can send a forged deauth frame with
the AP's MAC as source, and the client will accept it and tear
down the connection. PhantomRF's WiFi deauth attack exploits
this.

---

## Table of contents

- [What it does](#what-it-does)
- [Why it works](#why-it-works)
- [How to use it](#how-to-use-it)
- [The deauth frame structure](#the-deauth-frame-structure)
- [What you'll see](#what-youll-see)
- [Beacon flood (companion attack)](#beacon-flood-companion-attack)
- [Limitations](#limitations)
- [CLI / web equivalent](#cli--web-equivalent)
- [Next steps](#next-steps)

---

## What it does

You point PhantomRF at a target WiFi network (identified by its
SSID, BSSID, or channel), and it sends forged 802.11
deauthentication frames. The result:

- All clients connected to that AP see a "disconnected"
  notification.
- The clients cannot reconnect for as long as the jam
  continues.
- The AP's own logs show spurious "client left" events
  followed by reconnection attempts.

The attack works against **any** 802.11 network (a, b, g, n, ac,
ax) and against **any** client device. The only network-level
defense is 802.11w (Protected Management Frames), which signs
the management frames — but PMF is rarely enabled in practice.

## Why it works

802.11 management frames are unauthenticated. A deauth frame
looks like this:

```
Bytes 0-1:   Frame Control (0xC0, 0x00) - Mgmt, Deauth
Bytes 2-3:   Duration (0x0000)
Bytes 4-9:   Destination MAC (the client to kick)
Bytes 10-15: Source MAC (the AP)
Bytes 16-21: BSSID (the AP, again)
Bytes 22-23: Sequence Control
Bytes 24-25: Reason Code (0x0001 = unspecified)
```

A client receives this frame, checks that the source matches
the AP it's connected to (which it does, because we set it to
the AP's MAC), and disconnects. No crypto, no signature, no
challenge. The frame is trusted because the protocol says so.

The ESP32-S3's WiFi stack can transmit raw 802.11 frames via
`esp_wifi_80211_tx()`. This bypasses the normal association
state machine and sends the frame at the chosen channel and TX
power.

## How to use it

### From the web UI

1. Connect to the PhantomRF AP and open the dashboard.
2. Click **WiFi** in the sidebar.
3. Pick a target network from the **Scan** dropdown.
4. (Optional) Set the target channel. Default is the channel the
   target AP is on.
5. (Optional) Pick the reason code. 0x0001 ("unspecified") is
   the most common.
6. Click **Deauth**.

### From the CLI

```text
phantom> attack wifi
Starting wifi (method=channel)...
Running. Send 'attack stop' to abort.
phantom>
```

To target a specific AP:

```text
phantom> attack wifi 00:11:22:33:44:55
```

To target a specific channel only:

```text
phantom> set wifi.jam_method 0
phantom> set wifi.deauth_method 0
phantom> attack wifi
```

### From the OLED

The OLED doesn't have a target picker; it just sends the deauth
on the currently selected channel. To choose a channel:

1. Use the OLED menu to select **WiFi Attack**.
2. Press Next to cycle the channel number (1–14).
3. Press OK to start.

## The deauth frame structure

The full 26-byte deauth frame is built by the firmware:

```cpp
struct DeauthFrame {
    uint16_t frameControl;   // 0x00C0
    uint16_t duration;       // 0x0000
    uint8_t  dst[6];         // client MAC
    uint8_t  src[6];         // AP MAC
    uint8_t  bssid[6];       // AP MAC (same as src)
    uint16_t sequenceCtrl;   // 0x00F0
    uint16_t reasonCode;     // 0x0001 (unspecified)
} __attribute__((packed));
```

The PhantomRF scans for nearby APs in promiscuous mode first,
to learn the BSSIDs. It then sends a deauth frame for every
associated client it sees.

## What you'll see

When the deauth is running against a real AP:

- **Laptop:** WiFi disconnects, attempts to reconnect every
  5–10 seconds, may show "limited connectivity" or "no
  internet".
- **Phone:** WiFi drops to cellular, shows "WiFi unavailable".
  iOS and Android may suppress the reconnection attempt for
  30 seconds to save battery.
- **Smart TV / IoT:** goes offline, may show "no network" or
  "router not responding".
- **The AP itself:** the AP's admin console shows a flurry of
  "client disassociated" events.

> :warning: **Don't run a deauth against a network you don't
> own.** Even a 5-second test against a neighbour's AP can be
> enough to disrupt a video call or kick a security camera
> offline. See [Disclaimer](../legal/disclaimer.md).

## Beacon flood (companion attack)

The WiFi page also exposes a **Beacon Flood** attack. Instead
of deauthing clients, it spams beacon frames with random
SSIDs. The effect is the opposite: instead of disconnecting
clients, it confuses them.

- The WiFi network list on nearby devices fills up with
  hundreds of SSIDs.
- Some devices slow down or become unresponsive as they try
  to parse the beacons.
- It's a "denial of service" against the WiFi scanning
  function, not against the data path.

The beacon flood is fully unauthenticated and works against
every device.

## Limitations

- **802.11w (PMF) clients ignore unauthenticated deauth
  frames.** Modern iPhones, Macs, and some Android devices
  support PMF and will stay connected even under a deauth
  attack.
- **The ESP32-S3's raw TX is rate-limited.** Espressif's
  firmware throttles `esp_wifi_80211_tx()` to about 10
  frames/second. The first deauth usually disconnects the
  client; subsequent frames are wasted.
- **Channel hopping reduces effectiveness.** If the target AP
  hops channels (rare for APs, common for mesh networks), the
  jam must sweep channels too.
- **No effect on a wired-only network.** The deauth targets
  the WiFi link; a desktop connected by Ethernet is
  unaffected.
- **The PhantomRF AP is itself a 2.4 GHz network.** A
  deauth against a real network will also disrupt the
  PhantomRF AP. See
  [DESIGN §20.1](https://github.com/anomalyco/PhantomRF/blob/main/DESIGN.md#201-mitigation-self-jamming--rf-co-existence).

## CLI / web equivalent

The web UI's `/api/attack/start` endpoint:

```json
{
  "module": "wifi",
  "method": "deauth"
}
```

Other valid `method` values:
- `deauth` — forged deauth frames
- `beacon` — beacon flood
- `smart` — auto-detect APs and deauth their clients

## Next steps

- For the full algorithm (frame structure, sniff-then-deauth
  trick, ESP-IDF specifics), see
  [802.11-deauth.md](../algorithms/802.11-deauth.md).
- For the Bluetooth jam (which uses the same radio but a
  different modulation), see
  [bluetooth-jam.md](bluetooth-jam.md).
- For the legal picture, see
  [Disclaimer](../legal/disclaimer.md).
