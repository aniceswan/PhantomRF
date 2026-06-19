# Ethics

> What we will and won't add to PhantomRF, and the
> reasoning behind those decisions. The maintainers' stance
> on RF research, dual-use technology, and responsible
> disclosure.

PhantomRF is dual-use technology. The same hardware and
firmware that lets a security researcher evaluate the
resilience of their own WiFi can also be used to disrupt
someone else's. This page explains the maintainers' ethics
position: which features we'll add, which we won't, and why.

---

## Table of contents

- [Our principles](#our-principles)
- [What we will add](#what-we-will-add)
- [What we won't add](#what-we-wont-add)
- [Anti-features (defensive measures)](#anti-features-defensive-measures)
- [Disclosure policy](#disclosure-policy)
- [Contributions](#contributions)
- [Next steps](#next-steps)

---

## Our principles

The maintainers of PhantomRF share a few core principles:

1. **Knowledge should be free.** The algorithms PhantomRF
   implements are documented in academic papers, taught in
   university courses, and used in commercial products. We
   believe that making the knowledge accessible to more
   people leads to better-secured networks and a more
   informed public.

2. **Defense requires offense.** You cannot defend a network
   you don't understand. The only way to build good
   defensive tools is to have people who know how the
   offensive tools work. PhantomRF supports both.

3. **Personal responsibility.** PhantomRF is a tool. Like
   any tool, it can be used for good or ill. The maintainers
   trust the user to make ethical decisions about how to use
   it. We provide documentation about the legal and ethical
   picture, and we expect the user to read it.

4. **Transparency.** All of PhantomRF's design, code, and
   documentation is open source. There are no closed
   binaries, no proprietary "magic", no hidden features.
   If you can't read it on GitHub, it's not in PhantomRF.

5. **No targeting of safety-critical systems.** The
   maintainers will not add features that are designed to
   disrupt safety-critical systems: avionics, medical
   devices, emergency services, industrial control
   systems, etc. This is not a "line we'll cross later" —
   it's a hard boundary.

## What we will add

The following features are aligned with our principles and
will be merged into PhantomRF:

### Defensive features

- **Detection of WiFi deauth attacks** (DESIGN §20.1) —
  a network-side detector that notices anomalous deauth
  frame rates and alerts the admin.
- **Detection of BLE spam** — a similar detector for
  anomalous BLE advertisements.
- **Detection of sub-GHz interference** — a passive
  monitor that flags when a known-good device starts
  failing.
- **Audit log** (DESIGN §20.2) — every action of
  significance is logged, with a way to download the
  log for forensic analysis.
- **Network isolation** — the PhantomRF AP refuses to
  route traffic between connected clients by default,
  so a single device on the AP can't pivot to others.

### Educational features

- **Detailed spectrum visualisation** — waterfall
  displays, channel-by-channel breakdowns, and protocol
  decoders (DESIGN §20.5).
- **Live RSSI mapping** — visualise the 2.4 GHz band
  as a topographic map, so users can see where their
  devices are in the radio environment.
- **Step-by-step protocol explanations** — every attack
  in the docs has a "what is this doing and why" page
  (the [Algorithms](../algorithms/) section).
- **Quiz mode** — a series of multiple-choice questions
  to test the user's understanding of the RF concepts
  PhantomRF uses.

### Compatibility features

- **More ESP32 variants** — ESP32-S3 (current), ESP32
  classic, ESP32-C3, ESP32-S2. Already supported in M0.
- **More sub-GHz radios** — CC1101 (current), SX1278,
  RFM69. The M1 firmware will add SX1278 support.
- **More display types** — SSD1306 (current), SH1106,
  ILI9341 (TFT). M1 adds the ILI9341 with a richer UI.

### Quality-of-life features

- **OTA firmware updates** — already in M0.
- **Settings backup/restore** — JSON dump of all NVS
  settings, with a way to import the dump on a new
  device.
- **Multi-language support** — the web UI already has
  language detection; we'll add more translations
  (community contributions welcome).

## What we won't add

The following features will not be merged, even if
contributed:

### Targeting features

- **Pre-configured "jamming-as-a-service" payloads**
  targeting specific commercial devices (e.g. "one button
  to disable a Ring doorbell"). We will not provide
  ready-to-use profiles that target real-world products.
  The user can configure their own attack by following
  the documentation.

- **Anti-law-enforcement features.** We will not add
  anything designed to evade detection by regulators
  or law enforcement. This includes MAC randomization
  for "plausible deniability" (we already randomize
  MACs, but the OS-level MAC is still visible), signal
  obfuscation, etc.

- **Targeted disruption of safety-critical systems.** No
  features to interfere with avionics, medical devices,
  emergency services, industrial control systems, or
  any other system where disruption could cause physical
  harm. This is non-negotiable.

- **Pre-configured SSID lists for beacon spam.** The
  beacon flood attack requires the user to type out the
  SSIDs they want to spam. We will not ship a list of
  common SSIDs as a default.

### Cryptanalysis features

- **WPA2 password cracking.** PhantomRF is not a
  password cracker. Tools like `hashcat` and `aircrack-ng`
  exist for that. PhantomRF captures PMKID (which is
  useful for offline cracking), but it does not perform
  the cracking itself.

- **WPA3 downgrade attacks.** If a network supports both
  WPA2 and WPA3, the client may connect to the weaker
  one. PhantomRF does not implement this attack because
  it would let a user force their own (or someone else's)
  client to use a weaker security mode.

- **Mass surveillance of nearby devices.** PhantomRF does
  not implement a "scan everything and store it"
  feature. Each scan requires a deliberate user action
  and the results are not automatically uploaded
  anywhere.

### Privacy-invasive features

- **Tracking specific devices across networks.** PhantomRF
  does not implement a "follow this MAC address" feature.
  MAC randomization on modern devices means the value is
  not stable anyway.

- **Logging device identifiers to a remote server.**
  PhantomRF does not phone home. The firmware makes no
  outbound connections. The web UI is served only to
  the local AP.

## Anti-features (defensive measures)

PhantomRF ships with several **anti-features** that limit
its offensive capability. These are not bugs; they are
deliberate choices:

### 1. Authentication required

Every `/api/*` endpoint is behind HTTP Basic Auth
(DESIGN §20.2). A casual passerby who connects to the
PhantomRF AP cannot start an attack.

### 2. AP password on first boot

The AP password is a randomly-generated 16-character
string, not the default "phantom1234" or "admin". The
password is printed to the serial console on first boot.

### 3. Double-confirmation for 2.4 GHz attacks

The web UI requires the user to click "I understand this
will disconnect me" *and then* "Start attack anyway"
before starting a 2.4 GHz attack. This pattern is
familiar from the W0rthlessS0ul project (which doesn't
have it, to its detriment).

### 4. Rate limiting on auth

After 5 failed login attempts, the client is locked
out for 60 seconds. This prevents brute-force attacks on
the web UI password.

### 5. Status LED during attacks

The RGB LED turns red while a transmitting attack is
running. This is a visual indicator to anyone nearby that
the device is doing something. (It can be turned off via
NVS for the stealth-conscious, but the default is on.)

### 6. Thermal throttling

The firmware throttles (or shuts down) the radio if the
chip temperature exceeds 75 °C (or 85 °C). This is a
safety feature for the hardware, but it also limits how
long a continuous jam can run.

## Disclosure policy

If a vulnerability is discovered in a product that
PhantomRF could exploit, the maintainers will:

1. **Notify the vendor** within 90 days of discovery.
2. **Coordinate disclosure** with the vendor on a
   reasonable timeline.
3. **Document the finding** in the PhantomRF docs after
   the vendor has had a chance to fix it.
4. **Not include the exploit code** in PhantomRF until
   the vendor has shipped a fix.

This is the standard "responsible disclosure" policy. The
goal is to give vendors time to fix the bug, not to give
attackers a head start.

## Contributions

We welcome contributions. The maintainers review every
pull request and may reject contributions that don't align
with this ethics policy. A pull request that adds a
"jamming-as-a-service" profile for a specific commercial
device will be closed with a reference to this page.

If you're unsure whether your idea fits within the
project's ethics, open an issue and ask before writing
the code.

---

## Next steps

- The full legal disclaimer is in
  [Disclaimer](disclaimer.md).
- The per-country rules are in
  [Jurisdictions](jurisdictions.md).
- The first-build instructions are in
  [Installation](../getting-started/installation.md).
