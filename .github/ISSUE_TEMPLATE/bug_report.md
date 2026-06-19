---
name: 🐛 Bug report
description: Report incorrect behavior, crashes, build failures or anything that does not work as documented.
title: "[bug] "
labels: ["bug", "needs-triage"]
assignees: []
---

<!--
  Thanks for taking the time to file a bug. A well-written report gets
  fixed faster. Please fill out the sections below — anything you leave
  blank will be asked about anyway, which slows things down.
-->

## What happened?

A clear and concise description of what the bug is. **What** went wrong,
not **how** you fixed it.

## Reproduction steps

Minimal, deterministic steps to reproduce the bug. Numbered list, please.

1. `pio run -e esp32s3-n16r8 -t upload`
2. Open the web UI at `http://192.168.4.1/`
3. Click "Start BLE jam"
4. Observe: the device panics and reboots

## Expected behaviour

What you expected to happen, contrasted with what actually happened.

## Actual behaviour

What really happened. Include serial-monitor output, panic backtrace, web
UI screenshots, etc.

```text
# Paste relevant serial output here. Use ```text fences.
```

## Environment

| Item | Value |
|---|---|
| Board / variant | (e.g. ESP32-S3-WROOM-1-N16R8) |
| PlatformIO env | (e.g. `esp32s3-n16r8`) |
| Firmware version | (e.g. `v0.1.0`, `abc1234` build) |
| `platformio.ini` `build_flags` | (paste custom flags, if any) |
| Operating system | (Linux / macOS / Windows, version) |
| arduino-esp32 core | (e.g. `3.3.10`) |
| `RF24` library version | (e.g. `1.6.1`) |
| `SmartRC-CC1101-Driver-Lib` | (e.g. `3.0.2`) |
| `NimBLE-Arduino` | (e.g. `2.5.0`) |
| External modules | (which nRF24 / CC1101 / OLED you used) |

## Severity

How bad is the bug, in practice?

- [ ] **S0 — Critical** — bricks the device, loses settings, kills OTA, opens a security hole
- [ ] **S1 — High** — major feature broken, no workaround
- [ ] **S2 — Medium** — feature broken, workaround exists
- [ ] **S3 — Low** — cosmetic, doc typo, edge case

## Possible cause / guess

If you have an idea why this happens, share it. You might be right.

## Workaround

Did you find a way to work around the bug while we fix it?

## Screenshots / recordings

Drag-and-drop images or attach `.gif` / `.mp4` clips.

## Checklist

- [ ] I searched [existing issues](../../issues) and did not find a duplicate
- [ ] I read [SECURITY.md](../../blob/main/SECURITY.md) and this is **not** a security vulnerability
- [ ] I can reproduce the bug on the latest `main` commit
- [ ] I attached serial-monitor output or a panic backtrace
