# PhantomRF — Web Flasher

A static single-page web app that flashes the PhantomRF firmware into an
ESP32-S3 (or other supported ESP32 variant) directly from the browser.
Uses [esp-web-tools](https://esphome.github.io/esp-web-tools/) which
exposes a `<esp-web-install-button>` Web Component that wraps the
WebSerial API.

The flasher ships separately from the main web UI (`web/`) and is
designed to be hosted on GitHub Pages, Cloudflare Pages, or any static
host.

## Layout

```
flasher/
├── index.html              ← the page
├── manifest_<variant>.json ← one per build variant (see below)
└── README.md
```

The main firmware images themselves are **not** committed. They are
produced by `scripts/build.sh release` and copied here by the release
pipeline (see `Releasing` below).

## Manifest format

A manifest is a JSON file following the
[esp-web-tools manifest spec](https://esphome.github.io/esp-web-tools/docs/manifest/).
One manifest per build variant:

```json
{
  "name": "PhantomRF (ESP32-S3 N16R8)",
  "version": "1.0.0",
  "home_assistant_domain": "...",
  "funding_url": "https://github.com/.../PhantomRF",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "PhantomRF-esp32s3-n16r8-v1.0.0.bootloader.bin", "offset": 0 },
        { "path": "PhantomRF-esp32s3-n16r8-v1.0.0.partitions.bin", "offset": 32768 },
        { "path": "PhantomRF-esp32s3-n16r8-v1.0.0.bin",           "offset": 65536 }
      ]
    }
  ]
}
```

The `<esp-web-install-button>` element points at the manifest that
matches the user's selected board. The `index.html` ships with a
dropdown that switches the manifest URL at runtime.

## Releasing

1. Build the firmware and produce the `dist/` artifacts:

   ```bash
   ./scripts/build.sh release
   ```

2. Copy the artifacts into `flasher/`:

   ```bash
   cp dist/PhantomRF-esp32s3-n16r8-v*.bin  flasher/
   cp dist/manifest.json                    flasher/manifest_esp32s3-n16r8.json
   # ...repeat for every variant
   ```

3. Edit each `manifest_<variant>.json` to bump the version and update
   the file paths in the `parts` array. (Or generate them from
   `scripts/build.sh`.)

4. Commit and push. GitHub Pages (or whichever host) will pick up the
   change on the next deploy.

## Deploying to GitHub Pages

1. In the repo settings, enable Pages for the `gh-pages` branch (or
   `/docs` if you prefer).
2. Either:
   - copy the contents of `flasher/` to the root of `gh-pages`, or
   - configure Pages to serve from a `/flasher` subpath of `main` and
     update the `start_url` in `web/manifest.json` accordingly.
3. The page is reachable at
   `https://<org>.github.io/PhantomRF/`.

## Local development

You can test the flasher locally with any static file server. Because
WebSerial requires a secure context, you need **HTTPS** or
**http://localhost**.

```bash
# Option 1: Python
cd flasher
python3 -m http.server 8000

# Option 2: Node
npx serve flasher
```

Then open <http://localhost:8000/> in Chrome or Edge. Pick your board,
plug in the ESP32, hold BOOT, and click "Install PhantomRF".

> The same `../web/css/style.css` is shared with the main UI, so the
> flasher inherits the dark theme automatically.

## Browser support

| Browser          | Works? | Notes                                  |
| ---------------- | ------ | -------------------------------------- |
| Chrome / Edge 89+| ✅     | Recommended                            |
| Opera 75+        | ✅     | Chromium-based                         |
| Firefox          | ⚠️     | Enable `dom.webserial.enabled` in about:config |
| Safari           | ❌     | No WebSerial; use `esptool.py` instead |
| Mobile browsers  | ⚠️     | Limited — most mobile OSes hide WebSerial |

## Cross-references

- esp-web-tools documentation:
  <https://esphome.github.io/esp-web-tools/docs/getting-started/>
- WebSerial API:
  <https://developer.mozilla.org/en-US/docs/Web/API/WebSerial_API>
- PhantomRF main project docs: see `/docs/`
