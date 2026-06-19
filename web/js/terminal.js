/**
 * PhantomRF — in-browser terminal emulator.
 *
 * Talks to /api/cli, supports client-side tab completion from a small
 * built-in command list, and provides up/down history navigation.
 */

// Built-in command list (mirrors the firmware's serial CLI; the firmware
// is the source of truth, this list is for client-side completion only).
const COMMANDS = [
  'help', 'status', 'info', 'heap', 'tasks', 'uptime', 'reboot', 'reset',
  'attack', 'stop', 'list', 'modules', 'spectrum', 'scan', 'record', 'replay',
  'save', 'delete', 'ls', 'cat', 'cd', 'pwd', 'settings', 'set', 'get',
  'nrf24', 'cc1101', 'wifi', 'ble', 'bt', 'subghz', 'channel', 'pa',
  'pin', 'gpio', 'log', 'dump', 'version', 'about', 'clear', 'history',
];

export class Terminal {
  constructor() {
    this.output = null;
    this.input  = null;
    this.prompt = null;
    this.history = [];
    this.histIdx = -1;
    this.pending = '';
    this.bindOnce = false;
    this.app = null;
  }

  onReady(app) {
    this.app = app;
    this.init('terminal');
  }

  init(rootId) {
    const root = document.getElementById(rootId);
    if (!root) return;
    this.output = root.querySelector('.terminal__output');
    this.input  = root.querySelector('.terminal__input');
    this.prompt = root.querySelector('.terminal__prompt');
    if (!this.bindOnce) {
      this._bind();
      this.bindOnce = true;
    }
    this._banner();
  }

  // -------------------------------------------------------------------
  //  I/O
  // -------------------------------------------------------------------
  println(text, className = '') {
    if (!this.output) return;
    const line = document.createElement('span');
    line.className = `terminal__line ${className ? 'terminal__line--' + className : ''}`;
    line.textContent = text + '\n';
    this.output.appendChild(line);
    this._scroll();
  }

  printRaw(html, className = '') {
    if (!this.output) return;
    const line = document.createElement('span');
    line.className = `terminal__line ${className ? 'terminal__line--' + className : ''}`;
    line.innerHTML = html;
    this.output.appendChild(line);
    this._scroll();
  }

  clear() {
    if (this.output) this.output.innerHTML = '';
  }

  _scroll() {
    if (this.output) this.output.scrollTop = this.output.scrollHeight;
  }

  // -------------------------------------------------------------------
  //  Event wiring
  // -------------------------------------------------------------------
  _bind() {
    if (!this.input) return;

    this.input.addEventListener('keydown', async (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        await this._handleSubmit();
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        this._historyPrev();
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        this._historyNext();
      } else if (e.key === 'Tab') {
        e.preventDefault();
        this._tabComplete();
      } else if (e.key === 'l' && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        this.clear();
      } else if (e.key === 'c' && e.ctrlKey) {
        e.preventDefault();
        this.println('^C', 'dim');
        this.input.value = '';
      }
    });

    // Click on output to focus input
    this.output?.addEventListener('click', () => this.input.focus());

    // Clear / reconnect
    document.getElementById('terminal-clear')?.addEventListener('click', () => this.clear());
    document.getElementById('terminal-reconnect')?.addEventListener('click', async () => {
      this.clear();
      this._banner();
      try { await this.handleInput('status'); } catch { /* ignore */ }
    });
  }

  // -------------------------------------------------------------------
  //  Submission + history + completion
  // -------------------------------------------------------------------
  async _handleSubmit() {
    const value = this.input.value;
    this.input.value = '';
    if (!value.trim()) return;
    this.history.push(value);
    this.histIdx = this.history.length;
    this.println(`${this._currentPrompt()}${value}`, 'cmd');
    try { await this.handleInput(value); }
    catch (e) { this.println(`error: ${e.message}`, 'err'); }
  }

  _historyPrev() {
    if (!this.history.length) return;
    if (this.histIdx === this.history.length) this.pending = this.input.value;
    this.histIdx = Math.max(0, this.histIdx - 1);
    this.input.value = this.history[this.histIdx] || '';
    setTimeout(() => this.input.setSelectionRange(this.input.value.length, this.input.value.length), 0);
  }

  _historyNext() {
    if (!this.history.length) return;
    this.histIdx = Math.min(this.history.length, this.histIdx + 1);
    this.input.value = this.histIdx === this.history.length
      ? (this.pending || '')
      : (this.history[this.histIdx] || '');
    setTimeout(() => this.input.setSelectionRange(this.input.value.length, this.input.value.length), 0);
  }

  _tabComplete() {
    const value = this.input.value;
    const head  = value.lastIndexOf(' ') + 1;
    const token = value.slice(head);
    if (!token) return;
    const matches = COMMANDS.filter((c) => c.startsWith(token));
    if (matches.length === 1) {
      this.input.value = value.slice(0, head) + matches[0] + ' ';
    } else if (matches.length > 1) {
      // common prefix
      const cp = commonPrefix(matches);
      this.input.value = value.slice(0, head) + cp;
      this.println(matches.join('  '), 'dim');
    }
  }

  // -------------------------------------------------------------------
  //  Network
  // -------------------------------------------------------------------
  async handleInput(line) {
    const trimmed = line.trim();
    if (!trimmed) return;
    if (trimmed === 'clear') { this.clear(); return; }
    if (trimmed === 'help' || trimmed === '?') { this._help(); return; }
    if (trimmed === 'history') { this._showHistory(); return; }
    try {
      const res = await fetchJson('/api/cli', { method: 'POST', body: { cmd: trimmed } });
      if (res && typeof res.output === 'string' && res.output.length) {
        // Each line gets a "recv" class for visual consistency.
        res.output.split(/\r?\n/).forEach((ln) => {
          if (ln.length) this.println(ln, classifyLine(ln));
        });
      }
    } catch (e) {
      this.println(`error: ${e.message}`, 'err');
    }
  }

  // -------------------------------------------------------------------
  //  Helpers
  // -------------------------------------------------------------------
  _currentPrompt() { return this.prompt ? this.prompt.textContent : '>'; }

  _banner() {
    this.printRaw(
      `<span class="terminal__line--dim">PhantomRF web terminal &mdash; type </span>` +
      `<span class="terminal__line--cmd">help</span>` +
      `<span class="terminal__line--dim"> for commands, </span>` +
      `<span class="terminal__line--cmd">Tab</span>` +
      `<span class="terminal__line--dim"> to complete, </span>` +
      `<span class="terminal__line--cmd">&uarr;/&darr;</span>` +
      `<span class="terminal__line--dim"> for history, </span>` +
      `<span class="terminal__line--cmd">Ctrl+L</span>` +
      `<span class="terminal__line--dim"> to clear.</span>`
    );
  }

  _help() {
    const lines = [
      'Built-in commands (more available on the device):',
      '  status                  show current state',
      '  info                    system information',
      '  heap                    free heap memory',
      '  uptime                  time since boot',
      '  modules                 list registered attack modules',
      '  attack <mod> [method]   start an attack',
      '  stop                    stop current attack',
      '  spectrum [band]         toggle spectrum sweep',
      '  settings                show settings',
      '  set <key> <value>       change a setting',
      '  nrf24 <count|pa|rate>   configure nRF24',
      '  cc1101 <freq|power>     configure CC1101',
      '  record [seconds]        record sub-GHz signal',
      '  replay <file>           replay .sub recording',
      '  ls [path]               list files',
      '  cat <file>              print file contents',
      '  delete <file>           delete a file',
      '  reboot                  reboot device',
      '  reset                   factory reset',
      '  log [N]                 show last N log lines',
      '  version                 firmware version',
    ];
    lines.forEach((l) => this.println(l, l.endsWith(':') ? 'info' : 'dim'));
  }

  _showHistory() {
    if (!this.history.length) { this.println('(empty)', 'dim'); return; }
    this.history.forEach((h, i) => this.println(`${String(i + 1).padStart(3, ' ')}  ${h}`, 'dim'));
  }
}

function classifyLine(line) {
  const t = line.trim();
  if (t.startsWith('OK') || t.startsWith('[ok]')) return 'ok';
  if (t.startsWith('ERR') || t.startsWith('error') || t.startsWith('[err]')) return 'err';
  if (t.startsWith('WARN') || t.startsWith('[warn]')) return 'warn';
  if (t.startsWith('>') || t.startsWith('$')) return 'cmd';
  return 'recv';
}

function commonPrefix(arr) {
  if (!arr.length) return '';
  let p = arr[0];
  for (const s of arr) {
    while (!s.startsWith(p)) p = p.slice(0, -1);
    if (!p) return '';
  }
  return p;
}

async function fetchJson(path, opts = {}) {
  const res = await fetch(path, {
    method: opts.method || 'GET',
    headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
    body: opts.body ? JSON.stringify(opts.body) : undefined,
  });
  const ct = res.headers.get('content-type') || '';
  const data = ct.includes('json') ? await res.json() : await res.text();
  if (!res.ok) throw new Error((data && data.error) || res.statusText || `HTTP ${res.status}`);
  return data;
}

export { Terminal as default };
