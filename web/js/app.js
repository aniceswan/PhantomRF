/**
 * PhantomRF — main application controller.
 *
 * Owns the router, status polling, and exposes a small API for feature
 * modules. Feature modules (spectrum, attack, settings, files, terminal)
 * register themselves via the onReady hook.
 */

const POLL_STATUS_MS  = 200;   // 5 Hz per design §20.5
const POLL_SPECTRUM_MS = 1000; // 1 Hz per design §20.5
const POLL_ATTACKS_MS = 2000;  // attack list rarely changes
const TOAST_TTL_MS = 4500;
const FETCH_TIMEOUT_MS = 8000;

const PAGES = [
  'dashboard', 'bluetooth', 'wifi', '2.4ghz', 'subghz',
  'spectrum', 'files', 'terminal', 'ota', 'settings', 'help',
];

const KNOWN_PAGES = new Set(PAGES);

/**
 * Format helpers (shared by all modules).
 */
const fmt = {
  bytes(n) {
    if (n == null) return '--';
    if (n < 1024) return `${n} B`;
    if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
    return `${(n / 1024 / 1024).toFixed(2)} MB`;
  },
  duration(ms) {
    if (ms == null || ms < 0) return '--';
    const s = Math.floor(ms / 1000);
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    if (d > 0) return `${d}d ${h}h`;
    if (h > 0) return `${h}h ${m}m`;
    if (m > 0) return `${m}m ${sec}s`;
    return `${sec}s`;
  },
  voltage(v) {
    if (v == null) return '--';
    return v.toFixed(2);
  },
  temp(t) {
    if (t == null) return '--';
    return t.toFixed(1);
  },
  pct(p) {
    if (p == null) return '--';
    return `${Math.round(p)}%`;
  },
  date(ts) {
    if (ts == null) return '--';
    try {
      const d = new Date(ts * 1000);
      return d.toLocaleString();
    } catch {
      return String(ts);
    }
  },
  escape(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  },
};

/**
 * Fetch wrapper that always returns JSON, throws on error, and has a timeout.
 */
async function api(path, opts = {}) {
  const { timeout = FETCH_TIMEOUT_MS, method = 'GET', body, headers = {}, signal } = opts;
  const ctl = new AbortController();
  const tid = setTimeout(() => ctl.abort(), timeout);
  if (signal) signal.addEventListener('abort', () => ctl.abort());
  try {
    const init = {
      method,
      headers: { Accept: 'application/json', ...headers },
      signal: ctl.signal,
    };
    if (body !== undefined) {
      if (body instanceof FormData || body instanceof Blob) {
        init.body = body;
      } else {
        init.body = JSON.stringify(body);
        init.headers['Content-Type'] = 'application/json';
      }
    }
    const res = await fetch(path, init);
    const ct  = res.headers.get('content-type') || '';
    const payload = ct.includes('application/json') ? await res.json() : await res.text();
    if (!res.ok) {
      const msg = (payload && payload.error) || res.statusText || `HTTP ${res.status}`;
      throw new Error(msg);
    }
    return payload;
  } finally {
    clearTimeout(tid);
  }
}

/**
 * Single-instance toast manager.
 */
class Toaster {
  constructor(root) {
    this.root = root;
  }
  show({ message, title, level = 'info', ttl = TOAST_TTL_MS }) {
    const el = document.createElement('div');
    el.className = `toast toast--${level}`;
    el.innerHTML = `
      <div class="flex-1">
        ${title ? `<div class="toast__title">${fmt.escape(title)}</div>` : ''}
        <div class="toast__text">${fmt.escape(message)}</div>
      </div>
      <button class="modal__close" type="button" aria-label="Dismiss">×</button>
    `;
    const dismiss = () => {
      el.style.opacity = '0';
      el.style.transition = 'opacity 150ms';
      setTimeout(() => el.remove(), 180);
    };
    el.querySelector('.modal__close').addEventListener('click', dismiss);
    this.root.appendChild(el);
    if (ttl > 0) setTimeout(dismiss, ttl);
    return dismiss;
  }
}

/**
 * Modal manager — one modal at a time.
 */
class Modal {
  constructor({ backdrop, dialog, titleEl, bodyEl, footerEl, closeBtn }) {
    this.backdrop = backdrop;
    this.dialog   = dialog;
    this.titleEl  = titleEl;
    this.bodyEl   = bodyEl;
    this.footerEl = footerEl;
    this.closeBtn = closeBtn;
    this._onClose = null;
    this._keyHandler = (e) => { if (e.key === 'Escape') this.close(); };
    this.closeBtn.addEventListener('click', () => this.close());
    this.backdrop.addEventListener('click', (e) => {
      if (e.target === this.backdrop) this.close();
    });
  }
  /**
   * Show the modal. `actions` is an array of { label, level, value, autofocus }.
   * Returns a Promise resolving to the chosen value (or null if dismissed).
   */
  open({ title, body, actions, danger = false, wide = false }) {
    this.titleEl.textContent = title;
    this.bodyEl.innerHTML = '';
    if (typeof body === 'string') {
      this.bodyEl.innerHTML = body;
    } else if (body instanceof Node) {
      this.bodyEl.appendChild(body);
    }
    this.footerEl.innerHTML = '';
    return new Promise((resolve) => {
      this._onClose = resolve;
      this.dialog.classList.toggle('modal--danger', !!danger);
      this.dialog.classList.toggle('modal--wide', !!wide);
      for (const a of actions) {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `btn ${a.level ? `btn--${a.level}` : 'btn--ghost'}`;
        btn.textContent = a.label;
        if (a.autofocus) btn.autofocus = true;
        btn.addEventListener('click', () => {
          this._closeWith(a.value);
        });
        this.footerEl.appendChild(btn);
      }
      this.backdrop.classList.add('modal-backdrop--open');
      document.addEventListener('keydown', this._keyHandler);
      // Move focus to first button
      setTimeout(() => {
        const first = this.footerEl.querySelector('button');
        if (first) first.focus();
      }, 30);
    });
  }
  _closeWith(value) {
    this.backdrop.classList.remove('modal-backdrop--open');
    document.removeEventListener('keydown', this._keyHandler);
    if (this._onClose) { const fn = this._onClose; this._onClose = null; fn(value); }
  }
  close() { this._closeWith(null); }
}

/**
 * Main App class.
 */
class App {
  constructor() {
    this.state = {
      status: null,    // last /api/status
      spectrum: null,  // last /api/spectrum
      attacks: [],     // last /api/attack/list
      channel: 'ok',   // 'ok' | 'degraded' | 'lost'
    };
    this.polling = { status: true, spectrum: false };
    this.modules = [];
    this.eventLog = []; // in-memory recent activity (cap 20)

    this.toaster = new Toaster(document.getElementById('toasts'));
    this.modal = new Modal({
      backdrop: document.getElementById('modal-backdrop'),
      dialog:   document.getElementById('modal'),
      titleEl:  document.getElementById('modal-title'),
      bodyEl:   document.getElementById('modal-body'),
      footerEl: document.getElementById('modal-footer'),
      closeBtn: document.getElementById('modal-close'),
    });
  }

  /**
   * Allow feature modules to hook in once the DOM is ready.
   */
  registerModule(mod) {
    this.modules.push(mod);
    if (typeof mod.onReady === 'function') mod.onReady(this);
  }

  async init() {
    this._wireRouter();
    this._wireSidebar();
    this._wireDashboard();
    this._wireKeyboard();
    this._wireBanner();
    await this._loadAttacks();
    this._startStatusPolling();
    this._startAttackListPolling();
    this._startSpectrumWhenActive();
  }

  // -------------------------------------------------------------
  //  Routing
  // -------------------------------------------------------------
  _wireRouter() {
    const initial = (location.hash || '#dashboard').replace('#', '');
    this.navigate(KNOWN_PAGES.has(initial) ? initial : 'dashboard');
  }

  _wireSidebar() {
    document.querySelectorAll('.sidebar__item[data-page]').forEach((el) => {
      el.addEventListener('click', () => this.navigate(el.dataset.page));
    });
    const backdrop = document.getElementById('sidebar-backdrop');
    const sidebar  = document.getElementById('sidebar');
    const toggle   = document.getElementById('sidebar-toggle');
    const closeSidebar = () => {
      sidebar.classList.remove('sidebar--open');
      backdrop.classList.remove('sidebar-backdrop--open');
    };
    toggle.addEventListener('click', () => {
      const open = sidebar.classList.toggle('sidebar--open');
      backdrop.classList.toggle('sidebar-backdrop--open', open);
    });
    backdrop.addEventListener('click', closeSidebar);
    sidebar.addEventListener('click', (e) => {
      if (e.target.closest('.sidebar__item') && window.innerWidth <= 768) closeSidebar();
    });
  }

  _wireDashboard() {
    document.getElementById('dash-refresh')?.addEventListener('click', () => {
      this._loadStatus();
      this.showToast({ message: 'Refreshed', level: 'info', ttl: 1500 });
    });
    document.getElementById('dash-emergency-stop')?.addEventListener('click', async () => {
      const v = await this.modal.open({
        title: 'Emergency stop',
        body: '<p>All running attacks will be halted. Continue?</p>',
        actions: [
          { label: 'Cancel', value: false },
          { label: 'Stop', level: 'danger', value: true, autofocus: true },
        ],
        danger: true,
      });
      if (v) await this.attackStop();
    });
    document.querySelectorAll('[data-quick-attack]').forEach((btn) => {
      btn.addEventListener('click', () => this._quickAttack(btn.dataset.quickAttack));
    });
  }

  _wireKeyboard() {
    let pending = null;
    document.addEventListener('keydown', (e) => {
      // Don't intercept while typing in inputs
      if (e.target.matches('input, textarea, select')) return;
      if (e.metaKey || e.ctrlKey || e.altKey) return;
      if (e.key === 'g') { pending = 'g'; setTimeout(() => { pending = null; }, 800); return; }
      if (pending === 'g') {
        const map = { d: 'dashboard', s: 'spectrum', t: 'terminal', w: 'wifi', b: 'bluetooth' };
        if (map[e.key]) { e.preventDefault(); this.navigate(map[e.key]); }
        pending = null;
      }
    });
  }

  _wireBanner() {
    // Placeholder: control-channel banner shown in _loadStatus when degraded.
  }

  navigate(page) {
    if (!KNOWN_PAGES.has(page)) page = 'dashboard';
    document.querySelectorAll('.page').forEach((el) => {
      el.classList.toggle('page--active', el.id === `page-${page}`);
    });
    document.querySelectorAll('.sidebar__item').forEach((el) => {
      el.classList.toggle('sidebar__item--active', el.dataset.page === page);
    });
    location.hash = `#${page}`;
    this.polling.spectrum = (page === 'spectrum');
    this._startSpectrumWhenActive();
    for (const m of this.modules) {
      if (typeof m.onPageEnter === 'function') {
        try { m.onPageEnter(page); } catch (e) { /* swallow module errors */ }
      }
    }
  }

  // -------------------------------------------------------------
  //  Polling
  // -------------------------------------------------------------
  _startStatusPolling() {
    this._statusTimer && clearInterval(this._statusTimer);
    const tick = async () => {
      if (!this.polling.status) return;
      try { await this._loadStatus(); } catch { /* keep polling */ }
    };
    tick();
    this._statusTimer = setInterval(tick, POLL_STATUS_MS);
  }

  _startAttackListPolling() {
    this._attackTimer && clearInterval(this._attackTimer);
    const tick = async () => {
      try { await this._loadAttacks(); } catch { /* ignore */ }
    };
    tick();
    this._attackTimer = setInterval(tick, POLL_ATTACKS_MS);
  }

  _startSpectrumWhenActive() {
    this._spectrumTimer && clearInterval(this._spectrumTimer);
    if (!this.polling.spectrum) return;
    const tick = async () => {
      if (!this.polling.spectrum) return;
      try { await this._loadSpectrum(); } catch { /* keep polling */ }
    };
    tick();
    this._spectrumTimer = setInterval(tick, POLL_SPECTRUM_MS);
  }

  async _loadStatus() {
    const s = await api('/api/status');
    this.state.status = s;
    this.updateUI(s);
  }

  async _loadSpectrum() {
    const band = this.state.spectrumBand || '2.4ghz';
    const s = await api(`/api/spectrum?band=${encodeURIComponent(band)}`);
    this.state.spectrum = s;
    for (const m of this.modules) {
      if (typeof m.onSpectrum === 'function') {
        try { m.onSpectrum(s, band); } catch { /* ignore */ }
      }
    }
  }

  async _loadAttacks() {
    try {
      const list = await api('/api/attack/list');
      this.state.attacks = Array.isArray(list) ? list : [];
    } catch {
      this.state.attacks = [];
    }
    for (const m of this.modules) {
      if (typeof m.onAttacks === 'function') {
        try { m.onAttacks(this.state.attacks); } catch { /* ignore */ }
      }
    }
  }

  // -------------------------------------------------------------
  //  UI updates
  // -------------------------------------------------------------
  updateUI(s) {
    // Top bar
    setText('status-heap',   s.heap ? fmt.bytes(s.heap.free) : '--');
    setText('status-uptime', fmt.duration(s.uptime));
    setText('status-temp',   fmt.temp(s.temp));
    setText('status-vbat',   fmt.voltage(s.vbat));
    setText('status-wifi-clients', s.clients ?? 0);

    if (s.batteryPct != null) {
      const pct = s.batteryPct;
      const fill = document.getElementById('battery-fill');
      if (fill) fill.style.setProperty('--bat', `${Math.max(0, Math.min(100, pct))}%`);
      const wrap = document.getElementById('battery-wrap');
      if (wrap) {
        wrap.classList.remove('battery--high', 'battery--mid', 'battery--low', 'battery--crit');
        wrap.classList.add(
          pct > 60 ? 'battery--high' :
          pct > 30 ? 'battery--mid'  :
          pct > 10 ? 'battery--low'  : 'battery--crit'
        );
      }
    }

    // Channel health
    const ch = s.channel || 'ok';
    this.state.channel = ch;
    const dot = document.getElementById('status-channel');
    if (dot) {
      dot.classList.remove('dot--ready', 'dot--warning', 'dot--error');
      dot.classList.add(
        ch === 'ok' ? 'dot--ready' :
        ch === 'degraded' ? 'dot--warning' : 'dot--error'
      );
    }
    setText('status-channel-name', ch.toUpperCase());

    // Dashboard
    setText('dash-state', (s.state || 'idle').toUpperCase());
    setText('dash-state-sub', s.state === 'running' ? `Module: ${s.attack?.module || '--'}` : 'No active module');
    setText('dash-module', s.attack?.module?.toUpperCase() || '—');
    setText('dash-module-method', s.attack?.method ? `method: ${s.attack.method}` : '--');
    setText('dash-uptime', fmt.duration(s.uptime));
    setText('dash-heap',   s.heap ? fmt.bytes(s.heap.free) : '--');
    if (s.heap?.total) {
      setText('dash-heap-sub', `of ${fmt.bytes(s.heap.total)} total`);
    }
    setText('dash-temp',   fmt.temp(s.temp));
    setText('dash-vbat',   fmt.voltage(s.vbat));
    if (s.vbat && s.batteryPct != null) {
      setText('dash-vbat-sub', `${fmt.pct(s.batteryPct)} remaining`);
    } else if (s.vbat) {
      setText('dash-vbat-sub', 'external / USB');
    }

    // Per-module page indicators
    const moduleToPage = { bt: 'bluetooth', ble: 'bluetooth', wifi: 'wifi', nrf24: '2.4ghz', cc1101: 'subghz' };
    const pageToDot    = { bluetooth: 'bt', wifi: 'wifi', '2.4ghz': 'nrf24', subghz: 'cc1101' };
    for (const [page, key] of Object.entries(pageToDot)) {
      const dot = document.getElementById(`${key}-dot`);
      const txt = document.getElementById(`${key}-status`);
      if (!dot || !txt) continue;
      const running = s.state === 'running' && s.attack?.module === key;
      const isError = s.attack?.error;
      dot.classList.remove('dot--idle', 'dot--running', 'dot--error', 'dot--ready');
      dot.classList.add(running ? (isError ? 'dot--error' : 'dot--running') : 'dot--idle');
      txt.textContent = running ? (isError ? 'error' : 'running') : 'idle';
    }

    // Sidebar badges reflect per-module running state
    const moduleToBadge = {
      bt: 'bluetooth', ble: 'bluetooth',
      wifi: 'wifi', nrf24: '2.4ghz', cc1101: 'subghz',
    };
    document.querySelectorAll('[data-badge]').forEach((el) => {
      el.textContent = '—';
      el.classList.remove('sidebar__badge--idle', 'sidebar__badge--run', 'sidebar__badge--warn');
      el.classList.add('sidebar__badge--idle');
    });
    if (s.state === 'running' && s.attack?.module) {
      const page = moduleToBadge[s.attack.module];
      if (page) {
        const badge = document.querySelector(`[data-badge="${page}"]`);
        if (badge) {
          badge.textContent = s.attack.method ? s.attack.method[0].toUpperCase() : '•';
          badge.classList.remove('sidebar__badge--idle');
          badge.classList.add(s.attack.error ? 'sidebar__badge--warn' : 'sidebar__badge--run');
        }
      }
    }

    // Banner for control channel degradation (M §20.1)
    this._renderChannelBanner(s);

    // Firmware info
    if (s.version) setText('fw-version', s.version);
    if (s.build)   setText('fw-build', `build ${s.build}`);

    // Propagate to modules
    for (const m of this.modules) {
      if (typeof m.onStatus === 'function') {
        try { m.onStatus(s); } catch { /* ignore */ }
      }
    }
  }

  _renderChannelBanner(s) {
    const host = document.getElementById('dashboard-banner');
    if (!host) return;
    host.innerHTML = '';
    if (s.channel === 'ok' || !s.attack?.module) return;
    if (!['nrf24', 'ble', 'bt', 'wifi'].includes(s.attack.module)) return;
    if (s.state !== 'running') return;
    const div = document.createElement('div');
    div.className = 'banner banner--danger';
    div.innerHTML = `
      <svg class="banner__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="square" aria-hidden="true">
        <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/>
        <line x1="12" y1="9" x2="12" y2="13"/>
        <line x1="12" y1="17" x2="12.01" y2="17"/>
      </svg>
      <div class="banner__body">
        <div class="banner__title">Control channel degraded</div>
        <div class="banner__text">2.4 GHz attack active. This web UI may become unresponsive. Use USB CDC serial (115200 baud) or the OLED + buttons menu to stop the attack.</div>
      </div>
    `;
    host.appendChild(div);
  }

  // -------------------------------------------------------------
  //  Attack control
  // -------------------------------------------------------------
  async _quickAttack(name) {
    if (['2.4ghz', 'wifi', 'ble', 'bluetooth'].includes(name)) {
      const v = await this.modal.open({
        title: 'Confirm attack',
        body: `<p>Start a <strong>${name}</strong> attack? This may disconnect this UI.</p>`,
        actions: [
          { label: 'Cancel', value: false },
          { label: 'Start', level: 'danger', value: true, autofocus: true },
        ],
        danger: true,
      });
      if (!v) return;
    }
    try {
      await this.attackStart(name);
    } catch (e) {
      this.showToast({ message: e.message, level: 'danger' });
    }
  }

  async attackStart(moduleId, method) {
    const body = { module: moduleId };
    if (method) body.method = method;
    const res = await api('/api/attack/start', { method: 'POST', body });
    this._pushLog({ module: moduleId, action: 'start', method, ok: true });
    this.showToast({ title: 'Attack started', message: `${moduleId}${method ? ` · ${method}` : ''}`, level: 'warning' });
    return res;
  }

  async attackStop() {
    const res = await api('/api/attack/stop', { method: 'POST', body: {} });
    this._pushLog({ module: '—', action: 'stop', ok: true });
    this.showToast({ title: 'Stopped', message: 'Attack halted', level: 'success' });
    return res;
  }

  _pushLog(entry) {
    const t = new Date().toLocaleTimeString();
    this.eventLog.unshift({ ...entry, t });
    if (this.eventLog.length > 20) this.eventLog.pop();
    const body = document.getElementById('dash-log');
    if (!body) return;
    body.innerHTML = this.eventLog.length
      ? this.eventLog.map((r) => `
          <tr>
            <td>${fmt.escape(r.t)}</td>
            <td>${fmt.escape(r.module)}</td>
            <td>${fmt.escape(r.action)}${r.method ? ` (${fmt.escape(r.method)})` : ''}</td>
            <td><span class="badge ${r.ok ? 'badge--green' : 'badge--red'}">${r.ok ? 'OK' : 'ERR'}</span></td>
          </tr>`).join('')
      : '<tr><td colspan="4" class="text-tertiary text-center text-xs">No activity yet</td></tr>';
  }

  // -------------------------------------------------------------
  //  Toast
  // -------------------------------------------------------------
  showToast(opts) { return this.toaster.show(opts); }
  confirm(opts)   { return this.modal.open(opts); }
}

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.textContent = value;
}

// Boot -----------------------------------------------------------------------
const app = new App();
window.app = app; // for debugging in dev

// Feature modules self-register (they may not all exist on first paint)
function bootModules() {
  const loaders = [
    ['spectrum', () => import('./spectrum.js')],
    ['attack',   () => import('./attack.js')],
    ['settings', () => import('./settings.js')],
    ['files',    () => import('./files.js')],
    ['terminal', () => import('./terminal.js')],
  ];
  for (const [name, loader] of loaders) {
    loader()
      .then((mod) => {
        const Ctor = mod[name[0].toUpperCase() + name.slice(1)] || mod.default;
        if (Ctor) app.registerModule(new Ctor());
      })
      .catch((e) => {
        // eslint-disable-next-line no-console
        console.error(`[PhantomRF] failed to load ${name}:`, e);
      });
  }
}

document.addEventListener('DOMContentLoaded', () => {
  app.init().catch((e) => {
    // eslint-disable-next-line no-console
    console.error('[PhantomRF] init failed:', e);
    document.getElementById('status-channel-name').textContent = 'ERR';
  });
  bootModules();
});

// Expose helpers for inline use / debugging
export { api, fmt, App, Toaster, Modal };
