<template>
  <div class="page">
    <div class="card">
      <h1 class="title">
        <svg viewBox="0 0 24 24" class="logo-icon" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M12 2L8 6l4 4-4 4 4 4M14 6l4 4-4 4"/>
        </svg>
        BLE Gateway
        <span class="status" :class="online ? 'on' : 'off'" :title="online ? 'Connected' : 'Disconnected'">
          <span class="dot"></span>{{ online ? 'Online' : 'Offline' }}
        </span>
      </h1>

      <!-- Loading -->
      <div v-if="!ready" class="loader-wrap">
        <div class="spinner"></div>
        <span>Loading configuration…</span>
      </div>

      <!-- Form -->
      <form v-else @submit.prevent="saveConfig" novalidate>
        <div class="grid">
          <!-- Gateway Name (full width) -->
          <div class="field full">
            <label for="name">Gateway name</label>
            <input id="name" type="text" v-model="name" placeholder="my-gateway" />
            <span class="hint" v-if="name.length">MDNS: <code>{{ name }}.local</code></span>
          </div>

          <!-- Admin Login | Admin Password -->
          <div class="field">
            <label for="login">Admin login</label>
            <input id="login" type="text" v-model="login" placeholder="admin" autocomplete="username" />
          </div>

          <div class="field">
            <label for="password">Admin password</label>
            <div class="input-group">
              <input id="password" :type="show.password ? 'text' : 'password'" v-model="password" placeholder="Leave empty to keep" />
              <button type="button" class="eye-btn" @click="show.password = !show.password" :aria-label="show.password ? 'Hide' : 'Show'">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                  <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                  <circle cx="12" cy="12" r="3"/>
                  <line v-if="show.password" x1="2" y1="2" x2="22" y2="22"/>
                </svg>
              </button>
            </div>
          </div>

          <!-- WiFi SSID | WiFi Password -->
          <div class="field">
            <label for="ssid">WiFi SSID</label>
            <input id="ssid" type="text" v-model="wifi_ssid" placeholder="Network name" />
          </div>

          <div class="field">
            <label for="wpass">WiFi password</label>
            <div class="input-group">
              <input id="wpass" :type="show.wifi ? 'text' : 'password'" v-model="wifi_pass" placeholder="Network password" />
              <button type="button" class="eye-btn" @click="show.wifi = !show.wifi" :aria-label="show.wifi ? 'Hide' : 'Show'">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                  <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                  <circle cx="12" cy="12" r="3"/>
                  <line v-if="show.wifi" x1="2" y1="2" x2="22" y2="22"/>
                </svg>
              </button>
            </div>
          </div>

          <!-- AES Key (full width) -->
          <div class="field full">
            <label for="aes">AES Key</label>
            <div class="input-group">
              <input id="aes" :type="show.aes ? 'text' : 'password'" v-model="aes_key" placeholder="32 hex chars" />
              <button type="button" class="eye-btn" @click="show.aes = !show.aes" :aria-label="show.aes ? 'Hide' : 'Show'">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                  <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                  <circle cx="12" cy="12" r="3"/>
                  <line v-if="show.aes" x1="2" y1="2" x2="22" y2="22"/>
                </svg>
              </button>
            </div>
            <span class="hint">16-byte key — exactly 32 hexadecimal characters</span>
          </div>

          <!-- BLE token (full width) -->
          <div class="field full">
            <label for="ble">BLE token</label>
            <div class="input-group">
              <input id="ble" :type="show.ble ? 'text' : 'password'" v-model="ble_token" placeholder="WebSocket auth token" />
              <button type="button" class="eye-btn" @click="show.ble = !show.ble" :aria-label="show.ble ? 'Hide' : 'Show'">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                  <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                  <circle cx="12" cy="12" r="3"/>
                  <line v-if="show.ble" x1="2" y1="2" x2="22" y2="22"/>
                </svg>
              </button>
            </div>
            <span class="hint">Secret required by BLE clients (noble). Independent of the web admin login.</span>
          </div>

          <!-- Section separator -->
          <div class="field full section-sep">
            <span class="section-label">Static IP <code>(optional — leave empty for DHCP)</code></span>
          </div>

          <!-- Static IP -->
          <div class="field">
            <label for="static_ip">IP address</label>
            <input id="static_ip" type="text" v-model="static_ip" placeholder="192.168.1.50" />
          </div>

          <!-- Subnet Mask -->
          <div class="field">
            <label for="static_mask">Subnet mask</label>
            <input id="static_mask" type="text" v-model="static_mask" placeholder="255.255.255.0" />
          </div>

          <!-- Gateway -->
          <div class="field">
            <label for="static_gw">Gateway</label>
            <input id="static_gw" type="text" v-model="static_gw" placeholder="192.168.1.1" />
          </div>

          <!-- DNS -->
          <div class="field">
            <label for="static_dns">DNS server</label>
            <input id="static_dns" type="text" v-model="static_dns" placeholder="8.8.8.8" />
          </div>
        </div>

        <!-- Submit -->
        <div class="actions">
          <button type="submit" class="btn btn-primary" :class="{ saving }" :disabled="!configChanged || saving">
            <span v-if="saving" class="spinner sm"></span>
            <span>{{ saving ? 'Saving…' : 'Save configuration' }}</span>
          </button>
        </div>
      </form>

      <!-- System section -->
      <div v-if="ready" class="system">
        <div class="section-sep">
          <span class="section-label">System</span>
        </div>
        <div class="sys-actions">
          <button type="button" class="btn btn-ghost" :disabled="sysBusy" @click="askRestart">
            <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M23 4v6h-6M1 20v-6h6"/>
              <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>
            </svg>
            Restart
          </button>
          <button type="button" class="btn btn-danger" :disabled="sysBusy" @click="askFactoryReset">
            <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>
            </svg>
            Factory reset
          </button>
        </div>
      </div>
    </div>

    <!-- Confirmation modal -->
    <div v-if="dialog.open" class="modal-backdrop" @click.self="closeDialog">
      <div class="modal" role="dialog" aria-modal="true" :aria-label="dialog.title">
        <h2 :class="{ danger: dialog.danger }">{{ dialog.title }}</h2>
        <p>{{ dialog.message }}</p>
        <div class="modal-actions">
          <button type="button" class="btn btn-ghost" @click="closeDialog" :disabled="sysBusy">Cancel</button>
          <button type="button" class="btn" :class="dialog.danger ? 'btn-danger' : 'btn-primary'"
                  @click="dialog.onConfirm" :disabled="sysBusy">
            <span v-if="sysBusy" class="spinner sm"></span>
            <span>{{ dialog.confirmLabel }}</span>
          </button>
        </div>
      </div>
    </div>

    <!-- Reboot overlay -->
    <div v-if="reboot.active" class="modal-backdrop">
      <div class="modal" role="status">
        <div class="spinner" style="margin:0 auto"></div>
        <h2>{{ reboot.title }}</h2>
        <p>Redirecting in {{ reboot.seconds }}s…</p>
        <a class="btn btn-primary" :href="reboot.url">Open now</a>
      </div>
    </div>

    <!-- Toasts -->
    <div class="toast-area" v-if="errors.length || notice">
      <div v-if="errors.length" class="toast error">
        <ul>
          <li v-for="e in errors" :key="e">{{ e }}</li>
        </ul>
        <button class="close-btn" @click="errors = []" aria-label="Close">✕</button>
      </div>
      <div v-if="notice" class="toast success">
        <span>{{ notice }}</span>
        <button class="close-btn" @click="notice = ''" aria-label="Close">✕</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'

const REQUEST_TIMEOUT = 8000

const config   = ref({})
const name     = ref('')
const login    = ref('')
const password = ref('')
const wifi_ssid = ref('')
const wifi_pass = ref('')
const aes_key  = ref('')
const ble_token = ref('')
const static_ip   = ref('')
const static_mask = ref('')
const static_gw   = ref('')
const static_dns  = ref('')
const ready    = ref(false)
const online   = ref(false)
const errors   = ref([])
const notice   = ref('')
const saving   = ref(false)
const sysBusy  = ref(false)
const show = reactive({ password: false, wifi: false, aes: false, ble: false })

const dialog = reactive({
  open: false, title: '', message: '', confirmLabel: 'Confirm',
  danger: false, onConfirm: () => {},
})
const reboot = reactive({ active: false, seconds: 0, url: '', title: '' })

const configChanged = computed(() => {
  const c = config.value
  return (
    c.name !== name.value ||
    c.login !== login.value ||
    password.value !== '' ||
    c.wifi_ssid !== wifi_ssid.value ||
    c.wifi_pass !== wifi_pass.value ||
    c.aes_key !== aes_key.value ||
    (c.ble_token ?? '') !== ble_token.value ||
    (c.static_ip   ?? '') !== static_ip.value ||
    (c.static_mask ?? '') !== static_mask.value ||
    (c.static_gw   ?? '') !== static_gw.value ||
    (c.static_dns  ?? '') !== static_dns.value
  )
})

// fetch wrapper: aborts on timeout and maps failures to readable messages
async function apiFetch(url, opts = {}) {
  const ctrl = new AbortController()
  const timer = setTimeout(() => ctrl.abort(), REQUEST_TIMEOUT)
  try {
    const res = await fetch(url, { credentials: 'include', signal: ctrl.signal, ...opts })
    if (res.status === 401) throw new Error('Authentication failed — check admin login/password')
    if (!res.ok) throw new Error(`Request failed (HTTP ${res.status})`)
    return res
  } catch (e) {
    if (e.name === 'AbortError') throw new Error('Timed out — the ESP32 is not responding')
    if (e instanceof TypeError) throw new Error('Cannot reach the ESP32 (network error)')
    throw e
  } finally {
    clearTimeout(timer)
  }
}

function targetUrl(gwName) {
  return `https://${gwName}.local`
}

function startReboot(url, title, seconds) {
  reboot.url = url
  reboot.title = title
  reboot.seconds = seconds
  reboot.active = true
  const tick = setInterval(() => {
    reboot.seconds -= 1
    if (reboot.seconds <= 0) {
      clearInterval(tick)
      globalThis.location.href = url
    }
  }, 1000)
}

async function loadConfig() {
  try {
    const res = await apiFetch('/config')
    const data = await res.json()
    config.value   = data
    name.value     = data.name
    login.value    = data.login ?? 'admin'
    wifi_ssid.value = data.wifi_ssid
    wifi_pass.value = data.wifi_pass
    aes_key.value  = data.aes_key
    ble_token.value = data.ble_token ?? ''
    static_ip.value   = data.static_ip   ?? ''
    static_mask.value = data.static_mask ?? ''
    static_gw.value   = data.static_gw   ?? ''
    static_dns.value  = data.static_dns  ?? ''
    online.value   = true
    ready.value    = true
  } catch (e) {
    online.value = false
    errors.value.push(e.message || 'Error fetching configuration')
  }
}

async function saveConfig() {
  if (!configChanged.value || saving.value) return
  if (aes_key.value !== '' && !/^[0-9a-fA-F]{32}$/.test(aes_key.value)) {
    errors.value = ['AES key must be exactly 32 hexadecimal characters']
    return
  }
  saving.value = true
  errors.value = []
  const c = { ...config.value }
  const payload = {}
  if (c.name !== name.value)         { payload.name     = name.value;     c.name     = name.value }
  if (c.login !== login.value)       { payload.login    = login.value;    c.login    = login.value }
  if (password.value !== '')           { payload.password = password.value }
  if (c.wifi_ssid !== wifi_ssid.value) { payload.wifi_ssid = wifi_ssid.value; c.wifi_ssid = wifi_ssid.value }
  if (c.wifi_pass !== wifi_pass.value) { payload.wifi_pass = wifi_pass.value; c.wifi_pass = wifi_pass.value }
  if (c.aes_key !== aes_key.value)   { payload.aes_key  = aes_key.value;  c.aes_key  = aes_key.value }
  if ((c.ble_token ?? '') !== ble_token.value) { payload.ble_token = ble_token.value; c.ble_token = ble_token.value }
  // IP statique : toujours envoyée pour permettre la suppression (vide = DHCP)
  payload.static_ip   = static_ip.value
  payload.static_mask = static_mask.value
  payload.static_gw   = static_gw.value
  payload.static_dns  = static_dns.value
  try {
    const res = await apiFetch('/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    })
    const text = await res.text()
    if (text === 'OK') {
      config.value = c
      startReboot(targetUrl(c.name), 'Configuration saved — ESP32 rebooting', 12)
    } else {
      errors.value.push('Error saving configuration')
    }
  } catch (e) {
    errors.value.push(e.message || 'Error saving configuration')
  }
  saving.value = false
}

function closeDialog() {
  if (sysBusy.value) return
  dialog.open = false
}

function askRestart() {
  Object.assign(dialog, {
    open: true,
    danger: false,
    title: 'Restart the gateway?',
    message: 'The ESP32 will reboot. The web interface will be unavailable for a few seconds.',
    confirmLabel: 'Restart',
    onConfirm: doRestart,
  })
}

function askFactoryReset() {
  Object.assign(dialog, {
    open: true,
    danger: true,
    title: 'Factory reset?',
    message: 'All settings (WiFi, login, AES key, static IP…) will be erased and the device will reboot in setup mode. This cannot be undone.',
    confirmLabel: 'Erase & reset',
    onConfirm: doFactoryReset,
  })
}

async function doRestart() {
  sysBusy.value = true
  errors.value = []
  try {
    await apiFetch('/restart')
    dialog.open = false
    startReboot(targetUrl(config.value.name), 'ESP32 restarting', 12)
  } catch (e) {
    errors.value.push(e.message || 'Restart failed')
  }
  sysBusy.value = false
}

async function doFactoryReset() {
  sysBusy.value = true
  errors.value = []
  try {
    await apiFetch('/factoryReset')
    dialog.open = false
    // après reset le nom revient à la valeur par défaut et le WiFi est effacé
    startReboot(targetUrl('esp32gw'), 'Factory reset done — device rebooting', 20)
  } catch (e) {
    errors.value.push(e.message || 'Factory reset failed')
  }
  sysBusy.value = false
}

onMounted(loadConfig)
</script>

<style>
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0 }

:root {
  --bg: #0f172a;
  --surface: #1e293b;
  --border: #334155;
  --accent: #38bdf8;
  --accent-hover: #7dd3fc;
  --text: #e2e8f0;
  --muted: #94a3b8;
  --error: #ef4444;
  --success: #22c55e;
  --radius: 10px;
  --font: system-ui, -apple-system, sans-serif;
}

body { background: var(--bg); color: var(--text); font-family: var(--font); min-height: 100vh }

.page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1.5rem;
}

.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 2rem;
  width: 100%;
  max-width: 640px;
  box-shadow: 0 8px 32px rgba(0,0,0,.4);
}

.title {
  font-size: 1.4rem;
  font-weight: 700;
  color: var(--accent);
  display: flex;
  align-items: center;
  gap: .5rem;
  margin-bottom: 1.75rem;
}

.logo-icon { width: 1.4em; height: 1.4em; flex-shrink: 0 }

.status {
  margin-left: auto;
  display: inline-flex;
  align-items: center;
  gap: .35rem;
  font-size: .72rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: .04em;
  color: var(--muted);
}
.status .dot { width: 8px; height: 8px; border-radius: 50% }
.status.on  .dot { background: var(--success); box-shadow: 0 0 6px var(--success) }
.status.off .dot { background: var(--error) }
.status.on  { color: var(--success) }
.status.off { color: var(--error) }

.loader-wrap {
  display: flex;
  align-items: center;
  gap: 1rem;
  color: var(--muted);
  padding: 1rem 0;
}

.grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 1.2rem;
}

.field { display: flex; flex-direction: column; gap: .4rem }
.field.full { grid-column: 1 / -1 }

label { font-size: .85rem; font-weight: 600; color: var(--muted); text-transform: uppercase; letter-spacing: .04em }

input {
  width: 100%;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 6px;
  color: var(--text);
  padding: .55rem .75rem;
  font-size: .95rem;
  outline: none;
  transition: border-color .15s;
}
input:focus { border-color: var(--accent) }

.input-group { display: flex }
.input-group input { border-radius: 6px 0 0 6px }

.eye-btn {
  background: var(--bg);
  border: 1px solid var(--border);
  border-left: none;
  border-radius: 0 6px 6px 0;
  color: var(--muted);
  padding: 0 .65rem;
  cursor: pointer;
  display: flex;
  align-items: center;
  transition: color .15s;
}
.eye-btn:hover { color: var(--accent) }

.hint { font-size: .78rem; color: var(--muted) }
code { color: var(--accent); font-size: .85em }

.section-sep {
  border-top: 1px solid var(--border);
  padding-top: .75rem;
  margin-top: .25rem;
}
.section-label {
  font-size: .85rem;
  font-weight: 600;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: .04em;
}

.actions { margin-top: 1.75rem; text-align: center }

/* Buttons */
.btn {
  border: 1px solid transparent;
  border-radius: 6px;
  padding: .6rem 1.4rem;
  font-size: .95rem;
  font-weight: 700;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: .5rem;
  text-decoration: none;
  transition: background .15s, opacity .15s, border-color .15s;
}
.btn:disabled { opacity: .45; cursor: not-allowed }
.btn svg { flex-shrink: 0 }

.btn-primary { background: var(--accent); color: #0f172a }
.btn-primary:hover:not(:disabled) { background: var(--accent-hover) }

.btn-ghost {
  background: transparent;
  color: var(--text);
  border-color: var(--border);
}
.btn-ghost:hover:not(:disabled) { border-color: var(--accent); color: var(--accent) }

.btn-danger {
  background: transparent;
  color: var(--error);
  border-color: var(--error);
}
.btn-danger:hover:not(:disabled) { background: var(--error); color: #fff }

/* System section */
.system { margin-top: 1.75rem }
.sys-actions {
  display: flex;
  gap: .75rem;
  margin-top: 1rem;
  flex-wrap: wrap;
}

/* Modal */
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(15,23,42,.75);
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1.5rem;
  z-index: 200;
  animation: fade .15s ease;
}
.modal {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1.75rem;
  width: 100%;
  max-width: 420px;
  box-shadow: 0 12px 40px rgba(0,0,0,.5);
  text-align: center;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}
.modal h2 { font-size: 1.1rem; color: var(--accent) }
.modal h2.danger { color: var(--error) }
.modal p { font-size: .9rem; color: var(--muted); line-height: 1.5 }
.modal-actions {
  display: flex;
  gap: .75rem;
  justify-content: center;
}

/* Spinners */
.spinner {
  width: 36px; height: 36px;
  border: 3px solid var(--border);
  border-top-color: var(--accent);
  border-radius: 50%;
  animation: spin .7s linear infinite;
}
.spinner.sm {
  width: 16px; height: 16px;
  border-width: 2px;
  border-color: rgba(15,23,42,.3);
  border-top-color: #0f172a;
}

@keyframes spin { to { transform: rotate(360deg) } }
@keyframes fade { from { opacity: 0 } to { opacity: 1 } }

/* Toasts */
.toast-area {
  position: fixed;
  bottom: 1.5rem;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  flex-direction: column;
  gap: .6rem;
  min-width: 280px;
  max-width: 480px;
  z-index: 100;
}

.toast {
  display: flex;
  align-items: flex-start;
  gap: .75rem;
  padding: .75rem 1rem;
  border-radius: var(--radius);
  font-size: .9rem;
  animation: slide-up .2s ease;
}
.toast.error   { background: var(--error);   color: #fff }
.toast.success { background: var(--success); color: #fff }
.toast ul { list-style: none; flex: 1 }

.close-btn {
  background: transparent;
  border: none;
  color: inherit;
  font-size: 1rem;
  cursor: pointer;
  opacity: .8;
  padding: 0 .2rem;
  margin-left: auto;
}
.close-btn:hover { opacity: 1 }

@keyframes slide-up {
  from { transform: translateY(12px); opacity: 0 }
  to   { transform: translateY(0);    opacity: 1 }
}

@media (max-width: 480px) {
  .grid { grid-template-columns: 1fr }
  .field.full { grid-column: 1 }
  .card { padding: 1.25rem }
  .sys-actions .btn { flex: 1 }
}
</style>
