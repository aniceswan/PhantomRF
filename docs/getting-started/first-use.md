# First use

> From "I just flashed the firmware" to "I'm running my first
> attack" — a guided tour that won't get you arrested.

This page assumes the firmware is flashed and the device is booting
without errors. If not, jump to
[Troubleshooting](../reference/troubleshooting.md).

---

## Table of contents

- [Connect to the WiFi AP](#connect-to-the-wifi-ap)
- [Log in to the web UI](#log-in-to-the-web-ui)
- [What the dashboard shows](#what-the-dashboard-shows)
- [Run a test attack](#run-a-test-attack)
- [Verify with the spectrum analyzer](#verify-with-the-spectrum-analyzer)
- [Clean up](#clean-up)
- [What's next](#whats-next)

---

## Connect to the WiFi AP

After the first boot, the firmware broadcasts a WiFi access point
named **`PhantomRF-XXXX`** (where `XXXX` is the last 2 bytes of the
AP MAC address, upper-case hex). The password was printed to the USB
serial console during boot. If you missed it, hold the BOOT button
for 5 seconds and the device will reboot with a new password.

**On your phone or laptop:**

1. Open the WiFi settings.
2. Connect to `PhantomRF-XXXX`.
3. Enter the password from the serial console.
4. Open <http://192.168.4.1/> in your browser.

> :warning: **Your device will lose internet access while connected
> to the AP.** This is normal — the ESP32 is acting as a router
> without an upstream link. To regain internet, disconnect from the
> PhantomRF AP and rejoin your home network.

![WiFi connect — placeholder](../../assets/wifi-connect.png)

## Log in to the web UI

The first time you open the web UI, your browser will pop up an
HTTP Basic Auth dialog. The default credentials are:

- **Username:** `admin`
- **Password:** the 16-character string from the serial console

After you type the password, the browser caches the credential for
the session, so you won't be prompted again until you close the tab
or the session times out (30 minutes of idle).

> :warning: **Do not use the default password in public.** If you
> plan to take the device outside your home, change the password
> from the **Settings** page first. See
> [Settings](../reference/settings.md#appassword) for how.

![HTTP basic auth dialog — placeholder](../../assets/auth-dialog.png)

## What the dashboard shows

The web UI's main page is a dashboard with five panels:

| Panel | What it shows |
|---|---|
| **Status** | Current state (Idle / Running), uptime, free heap, WiFi clients |
| **Radio health** | nRF24 / CC1101 detection state, RSSI on the last channel |
| **Recent log** | Last 20 log messages (info, warn, error) |
| **Quick actions** | Buttons to start / stop each attack |
| **Spectrum** | Live RSSI waterfall for 2.4 GHz (and sub-GHz if available) |

The top-right corner shows the current WebSocket client count
(usually 1 — your browser). The bottom-left shows the device's
internal temperature; if it climbs above 70 °C, the firmware will
auto-throttle.

![Dashboard — placeholder](../../assets/dashboard.png)

## Run a test attack

The safest test attacks are the ones that don't actually transmit
anything. Start with the **Spectrum** attack, which only listens.

> :information_source: **A "spectrum analyzer" attack is read-only.**
> The radio enters RX mode, sweeps the channels, and reports the
> RSSI back. No frames are transmitted. The user-visible effect is
> zero.

1. Open the **Spectrum** page from the sidebar.
2. Pick "2.4 GHz" from the band dropdown.
3. Click **Start**.
4. The waterfall should start scrolling. Bright spots indicate RF
   activity — your WiFi AP, neighbouring WiFi networks, and any
   nearby Bluetooth devices.
5. Click **Stop** when you're done.

If you see a flat line with no bright spots, your area is RF-quiet
(which is unusual). If you see a wall of bright spots on channels 1,
6, and 11, those are the WiFi networks around you.

> :warning: **Now would be a good time to read
> [Legal disclaimer](../legal/disclaimer.md).** The next step is
> to start a transmitting attack, and that crosses a legal line
> in most jurisdictions without explicit authorisation.

## Verify with the spectrum analyzer

A second pass: start a WiFi deauth on a **non-existent target** and
look at the spectrum. The deauth frames will appear as a spike on
the WiFi channel you targeted. If you see the spike, the radio is
transmitting. If you don't, check the wiring.

> :warning: **Even a "non-existent target" deauth frames are
> transmitted on air.** This is illegal in many jurisdictions. If
> you're in a country where transmission without a licence is
> illegal, **stop here** and do this test inside a Faraday cage or
> in a country that allows it (e.g. on a test bench, in a
> licensed lab, etc.).

1. Open the **WiFi** page.
2. Set the channel to **1** (or whatever your test network uses).
3. Click **Deauth**.
4. Watch the spectrum. You should see a brief, intense spike on
   channel 1's centre frequency (2412 MHz).
5. Click **Stop** immediately.

The deauth will run for as long as you let it. To avoid jamming a
real network, set the duration to 5 seconds in the **Settings**
page or in the CLI (`set wifi.deauth_method 0` then start, then
stop after 5 s).

## Clean up

After your test, you should:

1. Click **Stop** on any running attack.
2. Go to **Settings** and change the AP password to something
   memorable (don't use the default).
3. (Optional) Disable the OLED or the AP if you don't need them.
4. Power-cycle the board to make sure your settings are persisted.

The NVS namespace is committed on every `set *` command, so your
settings should survive a reboot.

## What's next

- You now have a working device. The rest of the docs are about
  *using* it.
- [Features](../features/) — what each attack actually does, with
  screenshots.
- [Algorithms](../algorithms/) — how the underlying tricks work,
  in deep technical detail.
- [Reference](../reference/commands.md) — every CLI command and
  every REST endpoint.
