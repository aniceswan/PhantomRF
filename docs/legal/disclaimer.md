# Legal disclaimer

> :warning: **READ THIS BEFORE USING PHANTOMRF.** Operating a
> radio transmitter outside the bands and power levels
> permitted by your local regulator is a criminal offence in
> most jurisdictions. The authors and contributors of this
> project disclaim all responsibility for any damage caused
> by the use of this software or hardware.

PhantomRF is a **research and education platform**. It is
designed to be used by:

- **Security researchers** evaluating the security of their
  own wireless networks.
- **RF hobbyists** learning about 2.4 GHz and sub-GHz radio.
- **Students** studying wireless protocols and digital
  signal processing.
- **Penetration testers** operating under a signed
  contract that authorises their activity.

It is **not** designed for, and must not be used for,
disrupting or intercepting communications on networks you
do not own or have explicit authorisation to test.

---

## "I understand" requirement

Before you can access the web UI for the first time, you
must affirm that you understand and accept the following:

1. **I am at least 18 years old**, or I have the express
   consent of a parent or legal guardian.
2. **I will only use PhantomRF on networks and devices that
   I own** or have explicit, written authorisation to test.
3. **I understand that operating a radio transmitter
   without a licence may be illegal** in my jurisdiction,
   and that the penalties can include fines, equipment
   seizure, and criminal prosecution.
4. **I will not use PhantomRF to cause intentional
   interference** with safety-critical systems (avionics,
   medical devices, emergency services, etc.).
5. **I will comply with all applicable local, state,
   national, and international laws** governing radio
   transmission and computer network access.

If you cannot affirm all of the above, **do not use
PhantomRF**. Power down the device, return it to the
seller, and find a different hobby.

---

## Jurisdiction table (summary)

A more detailed per-country table is in
[Jurisdictions](jurisdictions.md). This is just the summary:

| Jurisdiction | Transmit licence required? | What you can do without one |
|---|---|---|
| **USA (FCC)** | Varies by band | 2.4 GHz at < 1 W, sub-GHz at < 1 W, only on your own networks |
| **EU (CEPT/ETSI)** | Yes (for most transmission) | Receive-only; lab use with shielding |
| **UK (Ofcom)** | Yes (for most transmission) | Receive-only; lab use with shielding |
| **Canada (ISED)** | Varies | 2.4 GHz at < 4 W EIRP for ISM; sub-GHz varies |
| **Australia (ACMA)** | Yes | 2.4 GHz ISM at < 4 W EIRP with low-power device class |
| **Japan (MIC)** | Yes | 2.4 GHz with "construction design certification" |
| **Other** | Varies widely | Check your local regulator's website |

> :information_source: **The transmit power limits above are
> the *maximum* allowed for unlicensed devices.** They do
> *not* authorise intentional interference. Even at the
> legal limit, you can only transmit on channels and
> frequencies allocated to your service.

---

## "Intentional interference" — what is it?

In every jurisdiction we are aware of, it is illegal to
**intentionally** interfere with radio communications.
"Intentional" means you knew you were doing it. PhantomRF
makes interference very easy, but that doesn't make it
legal.

Examples of **illegal** activity:

- Running a Bluetooth jam in a public space to disrupt other
  people's devices.
- Sending deauth frames at a coffee shop's WiFi to free up
  bandwidth.
- Jamming a neighbour's IoT devices because they annoy you.
- Interfering with safety-critical systems (airports,
  hospitals, emergency services).

Examples of **legal** activity:

- Jamming your *own* devices in a lab setting, inside a
  Faraday cage, on a channel you have a licence for.
- Testing the resilience of a client's WiFi as part of a
  signed penetration test.
- Recording and replaying a sub-GHz signal in your own
  workshop, on a frequency you own a licence for.
- Studying the spectrum analyzer in a remote area with no
  other radio users.

If you're unsure whether your planned use is legal, **don't
do it**. Find a local RF lawyer or contact the regulator
in your jurisdiction.

---

## What we will and won't add

See [Ethics](ethics.md) for the full list. The short
version:

**We will add:**

- Features that help users understand RF protocols.
- Defensive tools (e.g. detection of WiFi deauth attacks
  against your own network).
- Better documentation of the legal picture.
- Compatibility with more hardware variants.

**We will not add:**

- Pre-configured "jamming-as-a-service" payloads targeting
  specific commercial devices.
- Anything designed to evade law enforcement.
- Features that target safety-critical systems (avionics,
  medical, etc.).
- Brute-force tools for breaking encryption on devices you
  don't own.

---

## Compliance

The PhantomRF project does its best to comply with the laws
of the jurisdictions where it is developed and used. We
will:

- Provide a "safe mode" build that disables all transmission
  (spectrum analyzer only) for users in jurisdictions where
  unlicensed transmission is illegal.
- Refuse to add features that are clearly illegal in our
  primary development jurisdictions (USA, EU).
- Respond promptly to legal requests from regulators.

The PhantomRF project is not a "jamming-as-a-service"
platform. The hardware can be used for jamming, but the
firmware does not enable it by default — every attack
requires a deliberate user action.

---

## Reporting illegal use

If you encounter PhantomRF being used for illegal
purposes, please report it:

- To the local law enforcement agency in the jurisdiction
  where the illegal use is occurring.
- To the regulator (FCC, Ofcom, etc.) if the illegal use
  involves radio transmission.
- To the project maintainers (GitHub issue) if you see
  the firmware being used to develop new attacks.

We do not have the resources to investigate illegal use
ourselves, but we will cooperate with any law enforcement
inquiry.

---

## No warranty

THE SOFTWARE AND HARDWARE ARE PROVIDED "AS IS", WITHOUT
WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

See the [LICENSE](https://github.com/anomalyco/PhantomRF/blob/main/LICENSE)
file for the full text.

---

## Next steps

- The per-country table is in
  [Jurisdictions](jurisdictions.md).
- The ethical-use case list is in
  [Ethics](ethics.md).
- If you've read this and still want to use PhantomRF,
  head to [Installation](../getting-started/installation.md).
