/**
 * PhantomRF — file browser.
 *
 * Lists /api/files?path=..., supports download, delete, and upload of
 * .sub / .json files via either the click-to-pick input or drag-and-drop.
 */

const SUPPORTED_UPLOAD_EXTS = ['.sub', '.json', '.txt', '.bin'];

class Files {
  constructor() {
    this.path = '/recordings';
    this.items = [];
    this.bindOnce = false;
  }

  onReady(app) {
    this.app = app;
    this._bind();
    this.refresh();
  }

  _bind() {
    if (this.bindOnce) return;
    this.bindOnce = true;

    document.querySelectorAll('[data-path]').forEach((btn) => {
      btn.addEventListener('click', () => {
        this.path = btn.dataset.path;
        document.querySelectorAll('[data-path]').forEach((b) => b.classList.remove('btn--primary'));
        btn.classList.add('btn--primary');
        document.getElementById('files-path-title').textContent = this.path;
        this.refresh();
      });
    });
    // Default highlight
    const def = document.querySelector('[data-path="/recordings"]');
    if (def) def.classList.add('btn--primary');

    document.getElementById('files-refresh')?.addEventListener('click', () => this.refresh());

    const input = document.getElementById('files-input');
    const drop  = document.getElementById('files-drop');
    if (input) input.addEventListener('change', (e) => this._handleFiles(e.target.files));
    if (drop) {
      ['dragenter', 'dragover'].forEach((evt) => {
        drop.addEventListener(evt, (e) => { e.preventDefault(); drop.classList.add('files__upload--drag'); });
      });
      ['dragleave', 'drop'].forEach((evt) => {
        drop.addEventListener(evt, (e) => { e.preventDefault(); drop.classList.remove('files__upload--drag'); });
      });
      drop.addEventListener('drop', (e) => {
        e.preventDefault();
        this._handleFiles(e.dataTransfer.files);
      });
    }
  }

  async refresh() {
    const body = document.getElementById('files-list');
    const count = document.getElementById('files-count');
    if (body) body.innerHTML = '<tr><td colspan="4" class="text-tertiary text-center text-xs">Loading…</td></tr>';
    try {
      const data = await fetchJson(`/api/files?path=${encodeURIComponent(this.path)}`);
      this.items = Array.isArray(data) ? data : [];
    } catch (e) {
      this.items = [];
      if (body) body.innerHTML = `<tr><td colspan="4" class="text-danger text-center text-xs">${escape(e.message)}</td></tr>`;
      if (count) count.textContent = '0 files';
      return;
    }
    this._render();
  }

  _render() {
    const body = document.getElementById('files-list');
    const count = document.getElementById('files-count');
    if (!body) return;
    if (!this.items.length) {
      body.innerHTML = '<tr><td colspan="4" class="files__empty">No files in this directory</td></tr>';
      if (count) count.textContent = '0 files';
      return;
    }
    body.innerHTML = this.items.map((f) => `
      <tr>
        <td class="truncate" title="${escapeAttr(f.name)}">${escape(f.name)}</td>
        <td class="num">${escape(formatBytes(f.size))}</td>
        <td>${escape(formatDate(f.date))}</td>
        <td class="actions">
          <button class="btn btn--sm btn--ghost" data-action="download" data-name="${escapeAttr(f.name)}">Download</button>
          <button class="btn btn--sm btn--ghost" data-action="replay" data-name="${escapeAttr(f.name)}" ${canReplay(f) ? '' : 'disabled'}>Replay</button>
          <button class="btn btn--sm btn--danger" data-action="delete" data-name="${escapeAttr(f.name)}">Delete</button>
        </td>
      </tr>
    `).join('');
    if (count) count.textContent = `${this.items.length} file${this.items.length === 1 ? '' : 's'}`;

    body.querySelectorAll('[data-action]').forEach((btn) => {
      btn.addEventListener('click', (e) => {
        const name = e.currentTarget.dataset.name;
        const act  = e.currentTarget.dataset.action;
        if (act === 'download') this._download(name);
        if (act === 'delete')   this._delete(name);
        if (act === 'replay')   this._replay(name);
      });
    });
  }

  // -------------------------------------------------------------
  //  Actions
  // -------------------------------------------------------------
  _download(name) {
    const url = `/api/files/${encodeURIComponent(name)}`;
    // Use a hidden anchor to trigger the download.
    const a = document.createElement('a');
    a.href = url;
    a.download = name;
    a.rel = 'noopener';
    document.body.appendChild(a);
    a.click();
    a.remove();
  }

  async _delete(name) {
    const v = await this.app.confirm({
      title: 'Delete file',
      body: `<p>Permanently delete <code>${escape(name)}</code>? This cannot be undone.</p>`,
      actions: [
        { label: 'Cancel', value: false },
        { label: 'Delete', level: 'danger', value: true, autofocus: true },
      ],
      danger: true,
    });
    if (!v) return;
    try {
      await fetchJson(`/api/files/${encodeURIComponent(name)}`, { method: 'DELETE' });
      this.app.showToast({ title: 'Deleted', message: name, level: 'success' });
      this.refresh();
    } catch (e) {
      this.app.showToast({ message: e.message, level: 'danger' });
    }
  }

  async _replay(name) {
    try {
      await fetchJson('/api/replay', { method: 'POST', body: { file: name } });
      this.app.showToast({ title: 'Replay', message: `Replaying ${name}`, level: 'warning' });
    } catch (e) {
      this.app.showToast({ message: e.message, level: 'danger' });
    }
  }

  async _handleFiles(fileList) {
    if (!fileList || !fileList.length) return;
    const status = document.getElementById('files-upload-status');
    for (const file of fileList) {
      const ext = '.' + (file.name.split('.').pop() || '').toLowerCase();
      if (!SUPPORTED_UPLOAD_EXTS.includes(ext)) {
        this.app.showToast({ message: `Unsupported file type: ${ext}`, level: 'danger' });
        continue;
      }
      if (status) status.textContent = `Uploading ${file.name}…`;
      const fd = new FormData();
      fd.append('file', file, file.name);
      if (status) status.textContent = `Uploading ${file.name}…`;
      try {
        const url = `/api/files?path=${encodeURIComponent(this.path)}&name=${encodeURIComponent(file.name)}`;
        await fetchJson(url, { method: 'POST', body: fd });
        this.app.showToast({ title: 'Uploaded', message: file.name, level: 'success' });
      } catch (e) {
        this.app.showToast({ message: `Upload failed: ${e.message}`, level: 'danger' });
      }
    }
    if (status) status.textContent = 'No file selected';
    this.refresh();
  }
}

function canReplay(f) {
  return /\.sub$/i.test(f.name || '');
}

function formatBytes(n) {
  if (n == null) return '--';
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1024 / 1024).toFixed(2)} MB`;
}

function formatDate(v) {
  if (!v) return '--';
  // Accept seconds or ms
  const ms = v > 1e12 ? v : v * 1000;
  try { return new Date(ms).toLocaleString(); } catch { return String(v); }
}

async function fetchJson(path, opts = {}) {
  const init = { method: opts.method || 'GET' };
  if (opts.body !== undefined) {
    if (opts.body instanceof FormData) { init.body = opts.body; }
    else { init.body = JSON.stringify(opts.body); init.headers = { 'Content-Type': 'application/json' }; }
  } else {
    init.headers = { Accept: 'application/json' };
  }
  const res = await fetch(path, init);
  const ct = res.headers.get('content-type') || '';
  const data = ct.includes('json') ? await res.json() : await res.text();
  if (!res.ok) throw new Error((data && data.error) || res.statusText || `HTTP ${res.status}`);
  return data;
}

function escape(s) {
  return String(s == null ? '' : s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}
function escapeAttr(s) { return escape(s); }

export { Files };
export default Files;
