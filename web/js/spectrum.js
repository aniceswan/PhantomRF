/**
 * PhantomRF — spectrum visualization.
 *
 * Renders an RSSI bar chart (current frame) and a scrolling waterfall
 * (last 100 frames) on HTML5 canvases. Designed for 126 channels × 1 MHz
 * (2400–2525 MHz) by default; the module also handles arbitrary lengths.
 */

const NUM_CHANNELS = 126;
const WATERFALL_FRAMES = 100;

// dBm -> RGB colour. Designed to look like a thermal/heatmap.
// -100 (weakest) -> red, -80 -> yellow, -60 -> green, -40 (strongest) -> blue.
function dBmToColor(dbm) {
  // clamp to range
  if (dbm == null || isNaN(dbm)) dbm = -110;
  if (dbm < -100) dbm = -100;
  if (dbm > -40)  dbm = -40;

  // Stops at -100 (red), -80 (yellow), -60 (green), -40 (blue).
  const stops = [
    { dbm: -100, r: 248, g: 81,  b: 73  },
    { dbm: -80,  r: 210, g: 153, b: 34  },
    { dbm: -60,  r: 63,  g: 185, b: 80  },
    { dbm: -40,  r: 88,  g: 166, b: 255 },
  ];
  for (let i = 0; i < stops.length - 1; i++) {
    const a = stops[i], b = stops[i + 1];
    if (dbm >= a.dbm && dbm <= b.dbm) {
      const t = (dbm - a.dbm) / (b.dbm - a.dbm);
      return [
        Math.round(a.r + (b.r - a.r) * t),
        Math.round(a.g + (b.g - a.g) * t),
        Math.round(a.b + (b.b - a.b) * t),
      ];
    }
  }
  return [stops[stops.length - 1].r, stops[stops.length - 1].g, stops[stops.length - 1].b];
}

class Spectrum {
  constructor() {
    this.chart = null;
    this.chartCtx = null;
    this.waterfall = null;
    this.waterfallCtx = null;
    this.hoverEl = null;
    this.dpr = window.devicePixelRatio || 1;
    this.history = [];                 // rolling buffer of {rssi, ts}
    this.fpsCounter = { frames: 0, last: Date.now(), value: 0 };
    this.band = '2.4ghz';
    this.paused = false;
    this.lastFrameTs = 0;
    this.bindOnce = false;
  }

  onReady(app) {
    this.app = app;
    this.chart    = document.getElementById('spectrum-canvas');
    this.waterfall = document.getElementById('waterfall-canvas');
    this.hoverEl  = document.getElementById('spectrum-hover');
    this.chartCtx    = this.chart.getContext('2d');
    this.waterfallCtx = this.waterfall.getContext('2d');
    this._bind();
    this._resize();

    // ResizeObserver keeps the canvases sharp on orientation/resize
    // and after the page becomes visible (initial size may be 0).
    if (typeof ResizeObserver !== 'undefined') {
      const ro = new ResizeObserver(() => this._resize());
      if (this.chart.parentElement) ro.observe(this.chart.parentElement);
      if (this.waterfall.parentElement) ro.observe(this.waterfall.parentElement);
    }
  }

  onPageEnter(page) {
    if (page !== 'spectrum') return;
    // Force a fresh resize when entering the page (was hidden, may be 0×0).
    requestAnimationFrame(() => this._resize());
  }

  onSpectrum(payload, band) {
    if (this.paused) return;
    this.band = band;
    const rssi = Array.isArray(payload?.rssi) ? payload.rssi : [];
    if (!rssi.length) return;
    this._renderChart(rssi);
    this._renderWaterfall(rssi);
    this._tickFps();
  }

  // -------------------------------------------------------------
  //  Setup
  // -------------------------------------------------------------
  _bind() {
    if (this.bindOnce) return;
    this.bindOnce = true;

    // Band selector
    document.querySelectorAll('[data-band]').forEach((btn) => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('[data-band]').forEach((b) => b.classList.remove('btn--primary'));
        btn.classList.add('btn--primary');
        this.app.state.spectrumBand = btn.dataset.band;
        this.history = []; // clear on band change
        this._renderEmptyWaterfall();
        this.app._loadSpectrum().catch(() => {});
      });
    });
    // default band highlight
    const def = document.querySelector('[data-band="2.4ghz"]');
    if (def) def.classList.add('btn--primary');

    // Pause / clear
    document.getElementById('spectrum-pause')?.addEventListener('click', (e) => {
      this.paused = !this.paused;
      e.currentTarget.textContent = this.paused ? 'Resume' : 'Pause';
    });
    document.getElementById('spectrum-clear')?.addEventListener('click', () => {
      this.history = [];
      this._renderEmptyWaterfall();
      this._renderEmptyChart();
    });

    // Resize
    window.addEventListener('resize', () => this._resize());

    // Hover
    this.chart.addEventListener('mousemove', (e) => this._onHover(e));
    this.chart.addEventListener('mouseleave', () => {
      if (this.hoverEl) this.hoverEl.style.display = 'none';
    });
  }

  _resize() {
    [this.chart, this.waterfall].forEach((c) => {
      if (!c) return;
      const parent = c.parentElement;
      const w = parent.clientWidth;
      const h = parent.clientHeight;
      c.width  = Math.max(1, Math.floor(w * this.dpr));
      c.height = Math.max(1, Math.floor(h * this.dpr));
      c.style.width  = w + 'px';
      c.style.height = h + 'px';
    });
    if (this.chartCtx)    this.chartCtx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    if (this.waterfallCtx) this.waterfallCtx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    this._renderEmptyChart();
    this._renderEmptyWaterfall();
  }

  // -------------------------------------------------------------
  //  Chart
  // -------------------------------------------------------------
  _renderChart(rssi) {
    const ctx = this.chartCtx;
    if (!ctx) return;
    const w = this.chart.clientWidth;
    const h = this.chart.clientHeight;
    ctx.clearRect(0, 0, w, h);

    // Background grid
    ctx.strokeStyle = '#21262d';
    ctx.lineWidth = 1;
    ctx.font = '10px monospace';
    ctx.fillStyle = '#6e7681';
    ctx.textBaseline = 'top';
    ctx.textAlign = 'right';

    const padL = 36, padR = 6, padT = 4, padB = 18;
    const plotW = w - padL - padR;
    const plotH = h - padT - padB;

    // dBm grid lines every 20 dB from -100 to -40
    for (let dbm = -100; dbm <= -40; dbm += 20) {
      const y = padT + (1 - (dbm + 100) / 60) * plotH;
      ctx.beginPath();
      ctx.moveTo(padL, y);
      ctx.lineTo(w - padR, y);
      ctx.strokeStyle = (dbm === -60) ? '#30363d' : '#21262d';
      ctx.stroke();
      ctx.fillText(`${dbm}`, padL - 4, y - 5);
    }

    // Channel grid (every 10 channels, label every 20)
    const n = rssi.length;
    const barW = plotW / n;
    for (let ch = 0; ch < n; ch += 10) {
      const x = padL + ch * barW;
      ctx.beginPath();
      ctx.moveTo(x, padT);
      ctx.lineTo(x, h - padB);
      ctx.strokeStyle = '#21262d';
      ctx.stroke();
      if (ch % 20 === 0) {
        ctx.fillStyle = '#6e7681';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';
        const freq = (this.band === 'subghz') ? null : (2400 + ch);
        const lbl = freq != null ? `${ch} (${freq})` : `${ch}`;
        ctx.fillText(lbl, x + 1, h - padB + 2);
      }
    }

    // Bars
    for (let ch = 0; ch < n; ch++) {
      const dbm = rssi[ch];
      const norm = Math.max(0, Math.min(1, (dbm + 100) / 60));   // -100..-40
      const barH = norm * plotH;
      const x = padL + ch * barW;
      const y = padT + plotH - barH;
      const [r, g, b] = dBmToColor(dbm);
      ctx.fillStyle = `rgb(${r},${g},${b})`;
      ctx.fillRect(Math.floor(x) + 1, Math.floor(y), Math.max(1, Math.floor(barW) - 1), Math.floor(barH));
    }
  }

  _renderEmptyChart() {
    if (this.chartCtx) {
      const w = this.chart.clientWidth;
      const h = this.chart.clientHeight;
      this.chartCtx.fillStyle = '#0d1117';
      this.chartCtx.fillRect(0, 0, w, h);
    }
  }

  // -------------------------------------------------------------
  //  Waterfall
  // -------------------------------------------------------------
  _renderWaterfall(rssi) {
    const ctx = this.waterfallCtx;
    if (!ctx) return;
    const w = this.waterfall.clientWidth;
    const h = this.waterfall.clientHeight;

    this.history.push(rssi);
    if (this.history.length > WATERFALL_FRAMES) this.history.shift();

    // Draw waterfall as a downscaled image then scaled up.
    // We render row-by-row (top = oldest, bottom = newest) with a small vertical pixel size.
    const n = rssi.length;
    const cellW = Math.max(1, Math.floor(w / n));
    const cellH = Math.max(2, Math.floor(h / WATERFALL_FRAMES));

    // Clear
    ctx.fillStyle = '#0d1117';
    ctx.fillRect(0, 0, w, h);

    const totalRows = this.history.length;
    for (let i = 0; i < totalRows; i++) {
      const frame = this.history[i];
      const y = h - (totalRows - i) * cellH;
      for (let ch = 0; ch < n; ch++) {
        const [r, g, b] = dBmToColor(frame[ch]);
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillRect(ch * cellW, y, cellW, cellH);
      }
    }

    // Update frame counter
    const f = document.getElementById('spectrum-frames');
    if (f) f.textContent = `${this.history.length} / ${WATERFALL_FRAMES} frames`;
  }

  _renderEmptyWaterfall() {
    if (this.waterfallCtx) {
      const w = this.waterfall.clientWidth;
      const h = this.waterfall.clientHeight;
      this.waterfallCtx.fillStyle = '#0d1117';
      this.waterfallCtx.fillRect(0, 0, w, h);
    }
    const f = document.getElementById('spectrum-frames');
    if (f) f.textContent = `0 / ${WATERFALL_FRAMES} frames`;
  }

  // -------------------------------------------------------------
  //  Hover
  // -------------------------------------------------------------
  _onHover(e) {
    if (!this.hoverEl || !this.chart) return;
    const rect = this.chart.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const w = rect.width;
    const h = rect.height;
    const padL = 36, padR = 6, padT = 4, padB = 18;
    if (x < padL || x > w - padR || y < padT || y > h - padB) {
      this.hoverEl.style.display = 'none';
      return;
    }
    const last = this.history[this.history.length - 1];
    if (!last) { this.hoverEl.style.display = 'none'; return; }
    const n = last.length;
    const barW = (w - padL - padR) / n;
    const ch = Math.floor((x - padL) / barW);
    if (ch < 0 || ch >= n) { this.hoverEl.style.display = 'none'; return; }
    const dbm = last[ch];
    const freq = (this.band === 'subghz') ? '—' : (2400 + ch);
    this.hoverEl.innerHTML = `
      <div>CH <strong>${ch}</strong> &middot; ${freq} MHz</div>
      <div>RSSI <strong>${dbm}</strong> dBm</div>
    `;
    this.hoverEl.style.display = 'block';
  }

  // -------------------------------------------------------------
  //  FPS
  // -------------------------------------------------------------
  _tickFps() {
    this.fpsCounter.frames++;
    const now = Date.now();
    if (now - this.fpsCounter.last >= 1000) {
      this.fpsCounter.value = this.fpsCounter.frames;
      this.fpsCounter.frames = 0;
      this.fpsCounter.last = now;
      const f = document.getElementById('spectrum-fps');
      if (f) f.textContent = this.fpsCounter.value;
    }
  }
}

export { Spectrum, NUM_CHANNELS, WATERFALL_FRAMES, dBmToColor };
export default Spectrum;
