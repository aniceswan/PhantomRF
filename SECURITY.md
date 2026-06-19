# Security Policy

> PhantomRF is a research and educational tool. It is **not** a weapon
> and should never be used against systems you do not own or have
> explicit written authorisation to test. **Read [docs/legal/](./docs/legal)
> before flashing the device.**

This document explains how to report a security vulnerability in
**PhantomRF itself** — that is, a bug in our firmware, build pipeline,
web UI or distribution infrastructure. It does **not** authorise you
to use the tool against any system you do not own.

---

## Supported versions

We patch the most recent minor release line and the `main` branch.
Older releases receive **no** security backports.

| Version | Supported | Notes |
|---|---|---|
| `main` (latest) | ✅ | Cutting-edge. New features may land with rough edges. |
| `v1.x.y` (latest minor) | ✅ | Backported fixes for at least 6 months. |
| `v1.x.y` (older minors) | ⚠️ Critical only | No SLA, fix at our discretion. |
| `v0.x.y` (pre-1.0) | ❌ | Unstable. Please upgrade. |

If you are running a pre-1.0 build, **upgrade to `main`** before
reporting anything other than a critical issue.

---

## Reporting a vulnerability

### Preferred: GitHub Security tab

Use [GitHub's private vulnerability reporting][gh-advisory] on this
repository. This creates a private advisory that only you, the
maintainers, and (after triage) any co-disclosed parties can see.

[gh-advisory]: https://github.com/anomalyco/PhantomRF/security/advisories/new

### Fallback: email

If you cannot or will not use the GitHub Security tab, email
**security@example.com** (PGP key: see end of this document).

For both channels, please include:

1. **A clear, English-language title** that describes the bug in
   one sentence.
2. **A description of the impact**: what can an attacker do, and
   under what conditions?
3. **Affected version(s)**: commit hash, tag, or `pio run -e <env>` `-t
   size` output.
4. **Reproduction steps**: hardware, firmware version, attack commands,
   observed vs. expected behaviour. Serial-monitor output is great.
5. **Your assessment of severity** (Critical / High / Medium / Low)
   if you have one. We will confirm or adjust.
6. **Whether the issue is already public** (blog post, CVE,
   conference talk). If public, please link the source.
7. **Your name / handle** if you would like to be credited in the
   fix announcement. (Default is to stay anonymous unless you say
   otherwise.)

### What we will **not** ask for

We will never ask you to run a "proof-of-concept" that connects to
our infrastructure, and we will never ask for your private keys. If
you receive such a request claiming to be from the PhantomRF team,
it is a scam — please report it.

---

## Response timeline

We aim to:

| Stage | Target | Hard upper bound |
|---|---|---|
| **Acknowledge** receipt of the report | 2 business days | 7 days |
| **Triage** (reproduce, assess severity, assign a CVE) | 7 days | 30 days |
| **Fix** for Critical issues | 14 days | 60 days |
| **Fix** for High issues | 30 days | 90 days |
| **Fix** for Medium / Low issues | next minor release | — |
| **Public disclosure** | after the fix is released | — |

These are targets, not guarantees. If we are late, we will tell you
why. You are welcome to ping us at any point in the process.

---

## What is in scope

Anything that affects the **confidentiality, integrity or availability**
of:

- PhantomRF firmware running on an ESP32(-S3 / -C3 / -S2) device.
- The PhantomRF web UI served by the device.
- The PhantomRF build / release pipeline (CI, release artifacts,
  GitHub Actions workflows).
- The PhantomRF flasher site (`flasher/`).
- PhantomRF documentation site (`docs/` → gh-pages).

### Example bugs that **are** in scope

- An attacker on the same WiFi network can take over the web UI
  without authentication.
- A malicious `.bin` uploaded via the OTA endpoint escapes the
  firmware partition and corrupts user data.
- A buffer overflow in the NVS settings parser allows a malformed
  LittleFS file to execute arbitrary code.
- The release pipeline can be tricked into publishing an unsigned
  binary that downstream users will flash.
- The web flasher silently swaps the binary it's about to flash.
- CSRF / XSS in the web UI that leaks SSIDs or other sensitive
  settings to an attacker.

### Example bugs that are **out of scope**

- "PhantomRF can be used to deauth a WiFi network" — by design;
  see [docs/legal/](./docs/legal).
- "PhantomRF transmits RF energy" — by design; you build it
  yourself and accept regulatory responsibility.
- Theoretical side-channel attacks (e.g. power analysis on
  nRF24 emissions) with no demonstrated impact.
- Reports about hardware modules (nRF24L01, CC1101) themselves;
  those go to Nordic / TI.

If you're not sure whether something is in scope, report it
anyway — we will triage and tell you.

---

## Disclosure policy

We follow **coordinated disclosure**:

1. You report privately to us.
2. We confirm and triage.
3. We develop a fix.
4. We agree on a public disclosure date (typically the day the
   fix ships).
5. We credit you in the release notes and the GitHub Security
   Advisory, unless you ask to stay anonymous.
6. After disclosure, the advisory becomes public on the GitHub
   Security tab.

We will **not** pursue legal action against researchers who:

- Make a good-faith effort to avoid privacy violations, data
  destruction, or service disruption.
- Only test against systems they own or have written authorisation
  to test.
- Stop testing immediately if they find a critical issue and contact
  us before publishing anything.

If your testing accidentally affects a third party (e.g. a noisy
spectrum sweep that disrupts a neighbour's WiFi), stop, document
what happened, and tell us in the same report.

---

## Severity classification

We use CVSS v3.1 as a starting point, then adjust for the
research/educational framing of the project:

| Severity | Examples in PhantomRF |
|---|---|
| **Critical** | RCE in the web UI without auth, OTA bypass, persistent brick via malformed input |
| **High** | Authenticated web-UI RCE, settings leak across devices, NVS corruption |
| **Medium** | Stored XSS, CSRF on a destructive endpoint, weak default password |
| **Low** | Information disclosure (e.g. WiFi MAC), missing rate limit on harmless endpoints |

---

## Recognition

Security researchers who follow this policy and whose reports lead
to a fix are listed in the release notes and the project README.
If you would prefer to stay anonymous, say so in your initial
report and we will respect that.

Hall of fame (in chronological order):

- _Your name could be here._

---

## PGP key (for email reports)

```
-----BEGIN PGP PUBLIC KEY BLOCK-----
[Placeholder: rotate this section with the real key before publishing.
 Use `gpg --full-generate-key` (RSA-4096, 5y expiry), publish to
 keys.openpgp.org, and replace this block.]
-----END PGP PUBLIC KEY BLOCK-----
```

Key ID: `PHANTOMRF-SECURITY-KEY-ID` (placeholder)
Fingerprint: `XXXX XXXX XXXX XXXX XXXX  XXXX XXXX XXXX XXXX XXXX` (placeholder)

---

## Legal

This policy is provided in good faith. It does not create any
contractual obligation on the PhantomRF project or its maintainers.
You remain solely responsible for ensuring that your research
complies with the laws of your jurisdiction.
