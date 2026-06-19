/**
 * PhantomRF — settings module.
 *
 * Loads /api/settings, hydrates the form defined in index.html, validates
 * changes, posts the partial update to /api/settings, and re-displays
 * the values. Password change requires the current password.
 */

const DEFAULTS = {
  wifi:    { ssid: 'PhantomRF', password: '', channel: 1, hidden: false },
  nrf24:   { count: 1, pa: 'MAX', rate: '1MBPS' },
  cc1101:  { count: 1, freq: 433.92, power: 10 },
  sweep:   { method: 'list', direction: 'together', delay: 2 },
  buttons: { mode: 3, long: 600 },
  display: { flip: false, contrast: 128 },
  admin:   { user: 'admin' },
};

class Settings {
  constructor() {
    this.form = null;
    this.bindOnce = false;
    this.current = null;
  }

  onReady(app) {
    this.app = app;
    this.form = document.getElementById('settings-form');
    if (this.form) this._bind();
    this._load();
  }

  async _load() {
    try {
      const data = await fetchJson('/api/settings');
      this.current = mergeDeep(structuredClone(DEFAULTS), data || {});
    } catch {
      this.current = structuredClone(DEFAULTS);
    }
    this._populate();
  }

  _populate() {
    const c = this.current;
    if (!c) return;
    setVal('cfg-ap-ssid',          c.wifi?.ssid);
    setVal('cfg-ap-channel',       c.wifi?.channel);
    setChecked('cfg-ap-hidden',    !!c.wifi?.hidden);
    setVal('cfg-nrf24-count',      c.nrf24?.count);
    setVal('cfg-nrf24-pa',         c.nrf24?.pa);
    setVal('cfg-nrf24-rate',       c.nrf24?.rate);
    setVal('cfg-cc1101-count',     c.cc1101?.count);
    setVal('cfg-cc1101-freq',      c.cc1101?.freq);
    setVal('cfg-cc1101-power',     c.cc1101?.power);
    setVal('cfg-sweep-method',     c.sweep?.method);
    setVal('cfg-sweep-direction',  c.sweep?.direction);
    setVal('cfg-sweep-delay',      c.sweep?.delay);
    setVal('cfg-btn-mode',         c.buttons?.mode);
    setVal('cfg-btn-long',         c.buttons?.long);
    setChecked('cfg-display-flip', !!c.display?.flip);
    setVal('cfg-display-contrast', c.display?.contrast);
    const v = document.getElementById('cfg-display-contrast-value');
    if (v) v.textContent = c.display?.contrast ?? 128;
  }

  _bind() {
    if (this.bindOnce) return;
    this.bindOnce = true;

    // Contrast range live update
    const range = document.getElementById('cfg-display-contrast');
    const rv    = document.getElementById('cfg-display-contrast-value');
    if (range && rv) {
      range.addEventListener('input', () => { rv.textContent = range.value; });
    }

    // Form submit
    this.form.addEventListener('submit', (e) => {
      e.preventDefault();
      this._submit();
    });

    // Reset to defaults
    document.getElementById('settings-reset')?.addEventListener('click', async () => {
      const v = await this.app.confirm({
        title: 'Reset settings',
        body: '<p>This will restore all settings to factory defaults and reboot the device. Continue?</p>',
        actions: [
          { label: 'Cancel', value: false },
          { label: 'Reset', level: 'danger', value: true, autofocus: true },
        ],
        danger: true,
      });
      if (!v) return;
      try {
        await fetchJson('/api/reset', { method: 'POST', body: {} });
        this.app.showToast({ title: 'Resetting…', message: 'Device will reboot shortly', level: 'warning' });
      } catch (e) {
        this.app.showToast({ message: e.message, level: 'danger' });
      }
    });
  }

  _collect() {
    return {
      wifi: {
        ssid:    val('cfg-ap-ssid'),
        channel: Number(val('cfg-ap-channel')),
        hidden:  checked('cfg-ap-hidden'),
        // password is sent only when the field is non-empty
        ...(val('cfg-ap-password') ? { password: val('cfg-ap-password') } : {}),
      },
      nrf24: {
        count: Number(val('cfg-nrf24-count')),
        pa:    val('cfg-nrf24-pa'),
        rate:  val('cfg-nrf24-rate'),
      },
      cc1101: {
        count: Number(val('cfg-cc1101-count')),
        freq:  Number(val('cfg-cc1101-freq')),
        power: Number(val('cfg-cc1101-power')),
      },
      sweep: {
        method:    val('cfg-sweep-method'),
        direction: val('cfg-sweep-direction'),
        delay:     Number(val('cfg-sweep-delay')),
      },
      buttons: {
        mode: Number(val('cfg-btn-mode')),
        long: Number(val('cfg-btn-long')),
      },
      display: {
        flip:      checked('cfg-display-flip'),
        contrast:  Number(val('cfg-display-contrast')),
      },
    };
  }

  _validate(payload) {
    const errors = [];
    if (!payload.wifi.ssid || payload.wifi.ssid.length < 1 || payload.wifi.ssid.length > 32) {
      errors.push(['cfg-ap-ssid', 'SSID must be 1–32 characters']);
    }
    if (![1, 6, 11].includes(payload.wifi.channel)) {
      errors.push(['cfg-ap-channel', 'Channel must be 1, 6, or 11']);
    }
    if (payload.nrf24.count < 1 || payload.nrf24.count > 5) {
      errors.push(['cfg-nrf24-count', 'nRF24 module count must be 1–5']);
    }
    if (!['MIN', 'LOW', 'HIGH', 'MAX'].includes(payload.nrf24.pa)) {
      errors.push(['cfg-nrf24-pa', 'Invalid PA level']);
    }
    if (payload.cc1101.count < 0 || payload.cc1101.count > 2) {
      errors.push(['cfg-cc1101-count', 'CC1101 module count must be 0–2']);
    }
    if (payload.cc1101.count > 0 && (payload.cc1101.freq < 300 || payload.cc1101.freq > 928)) {
      errors.push(['cfg-cc1101-freq', 'Sub-GHz frequency must be 300–928 MHz']);
    }
    if (payload.sweep.delay < 0 || payload.sweep.delay > 1000) {
      errors.push(['cfg-sweep-delay', 'Channel dwell must be 0–1000 ms']);
    }
    if (![1, 2, 3].includes(payload.buttons.mode)) {
      errors.push(['cfg-btn-mode', 'Invalid button mode']);
    }
    if (payload.display.contrast < 0 || payload.display.contrast > 255) {
      errors.push(['cfg-display-contrast', 'Contrast must be 0–255']);
    }

    // Password change
    const pwCur  = val('cfg-pw-current');
    const pwNew  = val('cfg-pw-new');
    const pwConf = val('cfg-pw-confirm');
    if (pwNew || pwConf || pwCur) {
      if (!pwCur)  errors.push(['cfg-pw-current', 'Current password required']);
      if (!pwNew || pwNew.length < 8) errors.push(['cfg-pw-new', 'New password must be at least 8 characters']);
      if (pwNew !== pwConf) errors.push(['cfg-pw-confirm', 'Passwords do not match']);
    }
    return errors;
  }

  async _submit() {
    // Clear errors
    this.form.querySelectorAll('.form__error').forEach((e) => (e.textContent = ''));
    this.form.querySelectorAll('.input, .select').forEach((e) => e.classList.remove('input--error', 'select--error'));

    const payload = this._collect();
    const errors = this._validate(payload);
    if (errors.length) {
      for (const [id, msg] of errors) {
        const el = document.getElementById(id);
        if (el) el.classList.add(el.tagName === 'SELECT' ? 'select--error' : 'input--error');
        const errEl = el?.closest('.form__group')?.querySelector('.form__error');
        if (errEl) errEl.textContent = msg;
      }
      this.app.showToast({ title: 'Invalid input', message: errors[0][1], level: 'danger' });
      return;
    }

    // Build the actual settings object. Password is sent only on change.
    const pwNew  = val('cfg-pw-new');
    if (pwNew) {
      payload.admin = { currentPassword: val('cfg-pw-current'), newPassword: pwNew };
    }

    const saveBtn = document.getElementById('settings-save');
    saveBtn.disabled = true;
    saveBtn.innerHTML = '<span class="spinner"></span> Saving…';
    try {
      await fetchJson('/api/settings', { method: 'POST', body: payload });
      this.app.showToast({ title: 'Saved', message: 'Settings applied', level: 'success' });
      // Clear password fields
      ['cfg-pw-current', 'cfg-pw-new', 'cfg-pw-confirm'].forEach((id) => setVal(id, ''));
      await this._load();
    } catch (e) {
      this.app.showToast({ title: 'Save failed', message: e.message, level: 'danger' });
    } finally {
      saveBtn.disabled = false;
      saveBtn.textContent = 'Save settings';
    }
  }
}

function val(id) {
  const el = document.getElementById(id);
  return el ? el.value : '';
}
function checked(id) {
  const el = document.getElementById(id);
  return !!(el && el.checked);
}
function setVal(id, v) {
  const el = document.getElementById(id);
  if (el) el.value = v ?? '';
}
function setChecked(id, v) {
  const el = document.getElementById(id);
  if (el) el.checked = !!v;
}

async function fetchJson(path, opts = {}) {
  const res = await fetch(path, {
    method: opts.method || 'GET',
    headers: opts.body ? { 'Content-Type': 'application/json' } : { Accept: 'application/json' },
    body: opts.body ? JSON.stringify(opts.body) : undefined,
  });
  const ct = res.headers.get('content-type') || '';
  const data = ct.includes('json') ? await res.json() : await res.text();
  if (!res.ok) throw new Error((data && data.error) || res.statusText || `HTTP ${res.status}`);
  return data;
}

function mergeDeep(target, source) {
  if (Array.isArray(source)) return source.slice();
  if (source && typeof source === 'object') {
    for (const k of Object.keys(source)) {
      target[k] = mergeDeep(target[k] && typeof target[k] === 'object' ? target[k] : {}, source[k]);
    }
  } else if (source !== undefined) {
    return source;
  }
  return target;
}

function structuredClone(obj) {
  if (typeof structuredClone === 'function') return structuredClone(obj);
  return JSON.parse(JSON.stringify(obj));
}

export { Settings };
export default Settings;
