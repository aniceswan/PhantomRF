/**
 * PhantomRF — attack control module.
 *
 * Renders the per-page attack cards from the attacks registry, wires
 * up options + start/stop buttons, and handles the double-confirmation
 * pattern for 2.4 GHz attacks that may disconnect the WebSocket.
 */

// Pages that require 2.4 GHz double-confirmation (DESIGN §20.1).
const SELF_JAMMING_PAGES = new Set(['2.4ghz', 'wifi', 'bluetooth']);

// Module IDs that occupy the 2.4 GHz band and may disrupt WiFi control.
const WIFI_DISRUPTING_MODULES = new Set(['nrf24', 'wifi', 'ble', 'bt']);

class Attack {
  constructor() {
    this.cards = new Map(); // id -> DOM root
    this.attacks = [];
    this.bindOnce = false;
  }

  onReady(app) {
    this.app = app;
    this._bind();
  }

  onAttacks(list) {
    this.attacks = Array.isArray(list) ? list : [];
    this._renderAll();
  }

  onStatus(status) {
    if (!status) return;
    const running = status.state === 'running' && status.attack?.module;
    for (const [id, refs] of this.cards) {
      const isRunning = running && status.attack.module === id;
      const isError   = isRunning && !!status.attack.error;
      refs.root.classList.toggle('attack--running', isRunning && !isError);
      refs.root.classList.toggle('attack--error',   isError);
      refs.statusText.textContent = isError ? 'error' : isRunning ? 'running' : 'idle';
      refs.statusDot.classList.remove('dot--idle', 'dot--running', 'dot--ready', 'dot--error');
      refs.statusDot.classList.add(
        isError   ? 'dot--error'   :
        isRunning ? 'dot--running' : 'dot--idle'
      );
      // Toggle Start vs Stop
      if (refs.startBtn) {
        refs.startBtn.disabled = isRunning;
        refs.startBtn.textContent = isRunning ? 'Running…' : 'Start';
      }
      if (refs.stopBtn) {
        refs.stopBtn.classList.toggle('hidden', !isRunning);
      }
      if (refs.progress) {
        refs.progress.classList.toggle('hidden', !isRunning);
      }
    }
  }

  // -------------------------------------------------------------
  //  Rendering
  // -------------------------------------------------------------
  _renderAll() {
    // Group by page
    const groups = { bluetooth: [], wifi: [], '2.4ghz': [], subghz: [] };
    for (const a of this.attacks) {
      const page = this._pageFor(a);
      if (groups[page]) groups[page].push(a);
    }
    for (const [page, list] of Object.entries(groups)) {
      const host = document.getElementById(`attacks-${page}`);
      if (!host) continue;
      host.innerHTML = '';
      if (!list.length) {
        host.innerHTML = '<p class="text-tertiary text-sm">No attacks registered.</p>';
        continue;
      }
      for (const a of list) {
        host.appendChild(this._renderCard(a));
      }
    }
  }

  _pageFor(attack) {
    const m = (attack.module || attack.id || '').toLowerCase();
    if (m === 'bt' || m === 'ble') return 'bluetooth';
    if (m === 'wifi')             return 'wifi';
    if (m === 'nrf24')            return '2.4ghz';
    if (m === 'cc1101')           return 'subghz';
    if (SELF_JAMMING_PAGES.has(m)) return m;
    return 'subghz';
  }

  _renderCard(a) {
    const isDanger = WIFI_DISRUPTING_MODULES.has((a.module || a.id || '').toLowerCase());
    const id = a.id || a.module;
    const methods = Array.isArray(a.methods) ? a.methods : [];
    const opts = Array.isArray(a.options) ? a.options : [];

    const root = document.createElement('article');
    root.className = 'attack' + (isDanger ? ' attack--danger' : '');
    root.dataset.attackId = id;
    root.innerHTML = `
      <header class="attack__head">
        <div>
          <h3 class="attack__name">${escape(a.name || id)}</h3>
          <span class="attack__id text-tertiary text-xs text-mono">${escape(id)}</span>
        </div>
        <span class="attack__status">
          <span class="dot dot--idle status-dot"></span>
          <span class="status-text">idle</span>
        </span>
      </header>
      <div class="attack__body">
        <p class="attack__desc">${escape(a.description || '')}</p>
        ${opts.length ? this._renderOptions(opts) : ''}
        ${methods.length > 1 ? this._renderMethodSelect(id, methods) : ''}
        <div class="progress hidden" aria-hidden="true"><div class="progress__bar"></div></div>
        <div class="attack__actions">
          <button class="btn ${isDanger ? 'btn--danger' : 'btn--primary'} start-btn" type="button">Start</button>
          <button class="btn btn--ghost stop-btn hidden" type="button">Stop</button>
        </div>
      </div>
    `;

    const startBtn = root.querySelector('.start-btn');
    const stopBtn  = root.querySelector('.stop-btn');
    const statusDot = root.querySelector('.status-dot');
    const statusText = root.querySelector('.status-text');
    const progress = root.querySelector('.progress');
    const progressBar = progress.querySelector('.progress__bar');
    const methodSelect = root.querySelector('select[data-role="method"]');

    startBtn.addEventListener('click', () => this._onStart(a, root));
    stopBtn.addEventListener('click',  () => this._onStop(a, root));

    // simple progress indeterminate while running; replaced by onStatus ticks
    this.cards.set(id, { root, startBtn, stopBtn, statusDot, statusText, progress, progressBar, methodSelect, attack: a });
    return root;
  }

  _renderMethodSelect(id, methods) {
    return `
      <div class="form__group">
        <label class="form__label" for="method-${id}">
          <span class="form__label-text">Method</span>
        </label>
        <select class="select" id="method-${id}" data-role="method">
          ${methods.map((m) => `<option value="${escapeAttr(m.id || m)}">${escape(m.name || m.id || m)}</option>`).join('')}
        </select>
      </div>
    `;
  }

  _renderOptions(opts) {
    return `
      <div class="attack__opts">
        ${opts.map((o) => this._renderOption(o)).join('')}
      </div>
    `;
  }

  _renderOption(o) {
    const id = `opt-${o.id || o.name}`;
    const label = escape(o.label || o.name || o.id);
    if (o.type === 'select' || o.choices) {
      const choices = o.choices || [];
      return `
        <div class="form__group" style="min-width: 120px;">
          <label class="form__label" for="${id}"><span class="form__label-text">${label}</span></label>
          <select class="select" id="${id}" data-opt="${escapeAttr(o.id || o.name)}">
            ${choices.map((c) => `<option value="${escapeAttr(c.id ?? c.value ?? c)}"${c.default ? ' selected' : ''}>${escape(c.name ?? c.label ?? c)}</option>`).join('')}
          </select>
        </div>
      `;
    }
    if (o.type === 'bool' || o.type === 'boolean') {
      return `
        <div class="form__group" style="min-width: 120px;">
          <label class="checkbox" for="${id}">
            <input type="checkbox" id="${id}" data-opt="${escapeAttr(o.id || o.name)}" ${o.default ? 'checked' : ''}>
            <span class="form__label-text">${label}</span>
          </label>
        </div>
      `;
    }
    if (o.type === 'number' || o.type === 'range') {
      return `
        <div class="form__group" style="min-width: 120px;">
          <label class="form__label" for="${id}"><span class="form__label-text">${label}</span></label>
          <input class="input" type="number" id="${id}" data-opt="${escapeAttr(o.id || o.name)}"
                 min="${o.min ?? 0}" max="${o.max ?? 9999}" step="${o.step ?? 1}"
                 value="${o.default ?? ''}">
        </div>
      `;
    }
    return `
      <div class="form__group" style="min-width: 120px;">
        <label class="form__label" for="${id}"><span class="form__label-text">${label}</span></label>
        <input class="input" type="text" id="${id}" data-opt="${escapeAttr(o.id || o.name)}" value="${escapeAttr(o.default ?? '')}">
      </div>
    `;
  }

  // -------------------------------------------------------------
  //  Start / Stop with double-confirm
  // -------------------------------------------------------------
  async _onStart(attack, card) {
    const moduleId = attack.module || attack.id;
    const isDanger = WIFI_DISRUPTING_MODULES.has(String(moduleId).toLowerCase());
    const refs = this.cards.get(attack.id || moduleId);
    const method = refs?.methodSelect?.value || '';

    // Gather arbitrary options
    const opts = {};
    card.querySelectorAll('[data-opt]').forEach((el) => {
      const k = el.dataset.opt;
      if (el.type === 'checkbox') opts[k] = el.checked;
      else if (el.type === 'number') opts[k] = Number(el.value);
      else opts[k] = el.value;
    });

    if (isDanger) {
      const ok = await this._doubleConfirm(moduleId, method);
      if (!ok) return;
    }

    try {
      await this.app.attackStart(moduleId, method, opts);
    } catch (e) {
      this.app.showToast({ title: 'Attack failed', message: e.message, level: 'danger' });
    }
  }

  async _onStop(attack) {
    try { await this.app.attackStop(); }
    catch (e) { this.app.showToast({ message: e.message, level: 'danger' }); }
  }

  async _doubleConfirm(moduleId, method) {
    // First confirm — must understand self-jamming
    const body1 = document.createElement('div');
    body1.innerHTML = `
      <p>You're about to start a <strong>${escape(moduleId)}</strong> attack${method ? ` (method <code>${escape(method)}</code>)` : ''} on the 2.4 GHz band.</p>
      <p class="text-warning text-bold">This will likely disconnect this web UI.</p>
      <p>Use the <strong>USB serial</strong> (115200 baud) or the <strong>OLED + buttons</strong> menu to stop the attack.</p>
    `;
    const v1 = await this.app.confirm({
      title: 'Self-jamming warning',
      body: body1,
      actions: [
        { label: 'Cancel', value: false },
        { label: 'I understand', level: 'danger', value: true, autofocus: true },
      ],
      danger: true,
    });
    if (!v1) return false;

    // Second confirm — typed confirmation? Keep it simple with explicit button.
    const body2 = document.createElement('div');
    body2.innerHTML = `
      <p>Final confirmation. The device will transmit on the 2.4 GHz band until stopped or power-cycled.</p>
      <p class="text-tertiary text-sm">Make sure you are within USB or OLED range.</p>
    `;
    const v2 = await this.app.confirm({
      title: 'Start 2.4 GHz attack',
      body: body2,
      actions: [
        { label: 'Cancel', value: false },
        { label: 'Start anyway', level: 'danger', value: true, autofocus: true },
      ],
      danger: true,
    });
    return !!v2;
  }

  _bind() {
    if (this.bindOnce) return;
    this.bindOnce = true;
  }
}

function escape(s) {
  return String(s == null ? '' : s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}
function escapeAttr(s) { return escape(s); }

export { Attack };
export default Attack;
