<template>
  <div class="app">
    <!-- Sidebar -->
    <aside class="sidebar">
      <div class="brand">
        <span class="brand-avatar">
          <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="5" y="11" width="14" height="10" rx="2"/>
            <path d="M8 11V7a4 4 0 0 1 8 0v4"/>
          </svg>
        </span>
        <div class="brand-text">
          <div class="brand-name">Gateway Bluetooth</div>
          <div class="brand-sub">v{{ version }}</div>
        </div>
      </div>

      <nav class="nav">
        <button
          v-for="item in navItems"
          :key="item.id"
          type="button"
          class="nav-item"
          :class="{ active: view === item.id }"
          @click="view = item.id"
        >
          <span class="nav-icon" v-html="item.icon"></span>
          <span>{{ item.label }}</span>
        </button>
      </nav>

      <div class="sidebar-foot">
        <button type="button" class="nav-item" :disabled="sysBusy" @click="askRestart">
          <span class="nav-icon" v-html="icons.restart"></span>
          <span>Redémarrer</span>
        </button>
        <button type="button" class="nav-item danger" :disabled="sysBusy" @click="askFactoryReset">
          <span class="nav-icon" v-html="icons.factory"></span>
          <span>Réinitialiser</span>
        </button>
        <button type="button" class="nav-item" @click="toggleTheme">
          <span class="nav-icon" v-html="theme === 'dark' ? icons.sun : icons.moon"></span>
          <span>{{ theme === 'dark' ? 'Clair' : 'Sombre' }}</span>
        </button>
      </div>
    </aside>

    <!-- Main -->
    <div class="main">
      <header class="topbar">
        <div class="crumbs">{{ pageTitle }}</div>
        <div class="topbar-actions">
          <span class="chip" :class="online ? 'chip-ok' : 'chip-err'" :title="online ? 'Gateway en ligne' : 'Gateway hors ligne'">
            <span class="dot"></span>{{ online ? 'En ligne' : 'Hors ligne' }}
          </span>
        </div>
      </header>

      <main class="content">
        <div class="page-head">
          <h1>{{ activeNav.label }}</h1>
          <p>{{ activeNav.subtitle }}</p>
        </div>

        <!-- Loading (config-backed views) -->
        <div v-if="(view === 'identifiants' || view === 'reseau') && !ready" class="panel loader-wrap">
          <div class="spinner"></div>
          <span>Chargement de la configuration…</span>
        </div>

        <!-- Identifiants : accès admin + secrets -->
        <form v-else-if="view === 'identifiants'" class="panel" @submit.prevent="saveConfig" novalidate>
          <div class="grid">
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

            <div class="field">
              <label for="ble_login">BLE login</label>
              <input id="ble_login" type="text" v-model="ble_login" placeholder="admin" autocomplete="off" />
            </div>

            <div class="field">
              <label for="ble_pass">BLE password</label>
              <div class="input-group">
                <input id="ble_pass" :type="show.ble ? 'text' : 'password'" v-model="ble_pass" placeholder="admin" />
                <button type="button" class="eye-btn" @click="show.ble = !show.ble" :aria-label="show.ble ? 'Hide' : 'Show'">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                    <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                    <circle cx="12" cy="12" r="3"/>
                    <line v-if="show.ble" x1="2" y1="2" x2="22" y2="22"/>
                  </svg>
                </button>
              </div>
            </div>

            <div class="field full">
              <span class="hint">Secret required by BLE clients (noble). Independent of the web admin login.</span>
            </div>
          </div>

          <div class="actions">
            <button type="submit" class="btn btn-primary" :class="{ saving }" :disabled="!configChanged || saving">
              <span v-if="saving" class="spinner sm"></span>
              <span>{{ saving ? 'Saving…' : 'Save configuration' }}</span>
            </button>
          </div>
        </form>

        <!-- Réseau : WiFi + configuration réseau -->
        <form v-else-if="view === 'reseau'" class="panel" @submit.prevent="saveConfig" novalidate>
          <div class="grid">
            <div class="field full">
              <label for="name">Gateway name</label>
              <input id="name" type="text" v-model="name" placeholder="my-gateway" />
              <span class="hint" v-if="name.length">MDNS: <code>{{ name }}.local</code></span>
            </div>

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

            <div class="field full section-sep">
              <span class="section-label">Static IP <code>(optional — leave empty for DHCP)</code></span>
            </div>

            <div class="field">
              <label for="static_ip">IP address</label>
              <input id="static_ip" type="text" v-model="static_ip" placeholder="192.168.1.50" />
            </div>

            <div class="field">
              <label for="static_mask">Subnet mask</label>
              <input id="static_mask" type="text" v-model="static_mask" placeholder="255.255.255.0" />
            </div>

            <div class="field">
              <label for="static_gw">Gateway</label>
              <input id="static_gw" type="text" v-model="static_gw" placeholder="192.168.1.1" />
            </div>

            <div class="field">
              <label for="static_dns">DNS server</label>
              <input id="static_dns" type="text" v-model="static_dns" placeholder="8.8.8.8" />
            </div>
          </div>

          <div class="actions">
            <button type="submit" class="btn btn-primary" :class="{ saving }" :disabled="!configChanged || saving">
              <span v-if="saving" class="spinner sm"></span>
              <span>{{ saving ? 'Saving…' : 'Save configuration' }}</span>
            </button>
          </div>
        </form>

        <!-- Radar : appareils Bluetooth à proximité -->
        <div v-else-if="view === 'radar'" class="panel radar-panel">
          <div class="radar-wrap">
            <svg class="radar-svg" viewBox="0 0 240 240" role="img" aria-label="Radar Bluetooth">
              <circle class="rd-ring" cx="120" cy="120" r="110" />
              <circle class="rd-ring" cx="120" cy="120" r="74" />
              <circle class="rd-ring" cx="120" cy="120" r="38" />
              <line class="rd-axis" x1="120" y1="10" x2="120" y2="230" />
              <line class="rd-axis" x1="10" y1="120" x2="230" y2="120" />
              <g class="rd-sweep" style="transform-origin:120px 120px">
                <path d="M120 120 L120 12 A108 108 0 0 1 196 44 Z" />
              </g>
              <circle
                v-for="b in radarBlips"
                :key="b.id"
                class="rd-blip"
                :cx="b.x"
                :cy="b.y"
                r="4.5"
              >
                <title>{{ b.label }}</title>
              </circle>
              <circle class="rd-center" cx="120" cy="120" r="3" />
            </svg>
          </div>

          <ul class="radar-list" v-if="radarBlips.length">
            <li v-for="b in radarBlips" :key="'l' + b.id">
              <span class="rd-li-name">{{ b.name || b.id }}</span>
              <span class="rd-li-rssi">{{ b.rssi }} dBm</span>
            </li>
          </ul>
          <p v-else class="hint rd-empty">Aucun appareil détecté… (scan en cours)</p>

          <div class="radar-foot">
            <span class="hint">{{ radarBlips.length }} appareil(s) · actualisé toutes les 2 s · scan autonome</span>
          </div>
        </div>

        <!-- OTA : mise à jour firmware -->
        <div v-else-if="view === 'ota'" class="panel">
          <div class="fw-update">
            <label class="fw-label">Firmware update <code>(.bin)</code></label>
            <div class="fw-row">
              <input type="file" accept=".bin" @change="onFwSelect" :disabled="sysBusy" />
              <button type="button" class="btn btn-ghost" :disabled="!fwFile || sysBusy" @click="askFlash">
                <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/>
                </svg>
                Flash
              </button>
            </div>
            <span class="hint">Téléversez le binaire compilé (.bin) ; l'appareil redémarre après la mise à jour. Ne coupez pas l'alimentation.</span>
          </div>
        </div>

      </main>
    </div>

    <!-- Confirmation modal -->
    <div v-if="dialog.open" class="modal-backdrop" @click.self="closeDialog">
      <modal class="modal" role="dialog" aria-modal="true" :aria-label="dialog.title">
        <h2 :class="{ danger: dialog.danger }">{{ dialog.title }}</h2>
        <p>{{ dialog.message }}</p>
        <div v-if="ota.uploading" class="progress" aria-label="Upload progress">
          <div class="bar" :style="{ width: ota.progress + '%' }"></div>
        </div>
        <p v-if="ota.uploading">Uploading… {{ ota.progress }}%</p>
        <div class="modal-actions">
          <button type="button" class="btn btn-ghost" @click="closeDialog" :disabled="sysBusy">Cancel</button>
          <button type="button" class="btn" :class="dialog.danger ? 'btn-danger' : 'btn-primary'"
                  @click="dialog.onConfirm" :disabled="sysBusy">
            <span v-if="sysBusy" class="spinner sm"></span>
            <span>{{ dialog.confirmLabel }}</span>
          </button>
        </div>
      </modal>
    </div>

    <!-- Reboot overlay -->
    <div v-if="reboot.active" class="modal-backdrop">
      <modal class="modal" role="status">
        <div class="spinner" style="margin:0 auto"></div>
        <h2>{{ reboot.title }}</h2>
        <p>Redirecting in {{ reboot.seconds }}s…</p>
        <a class="btn btn-primary" :href="reboot.url">Open now</a>
      </modal>
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
import { ref, reactive, computed, onMounted, watch, onBeforeUnmount } from 'vue'
import { version } from '../package.json'

const REQUEST_TIMEOUT = 8000
const THEME_KEY = 'ttlock_theme'
const IPV4_RE = /^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$/

const icons = {
  credentials: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><circle cx="8" cy="15" r="4"/><path d="M10.8 12.2 20 3M16 6l3 3M13 9l2.5 2.5"/></svg>',
  network: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M2 9a16 16 0 0 1 20 0"/><path d="M5 12.5a10 10 0 0 1 14 0"/><path d="M8.5 16a5 5 0 0 1 7 0"/><line x1="12" y1="19.5" x2="12.01" y2="19.5"/></svg>',
  ota: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/></svg>',
  radar: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="5"/><circle cx="12" cy="12" r="1"/><path d="M12 12 19 7"/></svg>',
  restart: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M23 4v6h-6M1 20v-6h6"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/></svg>',
  factory: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>',
  sun: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>',
  moon: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>',
}

const navItems = [
  { id: 'identifiants', label: 'Identifiants', icon: icons.credentials, subtitle: 'Accès administrateur et secrets de chiffrement' },
  { id: 'reseau',       label: 'Réseau',       icon: icons.network,     subtitle: 'WiFi et configuration réseau de la passerelle' },
  { id: 'ota',          label: 'OTA',          icon: icons.ota,         subtitle: 'Mise à jour du firmware' },
  { id: 'radar',        label: 'Radar',        icon: icons.radar,       subtitle: 'Appareils Bluetooth à proximité' },
]

const view = ref('identifiants')
const activeNav = computed(() => navItems.find(n => n.id === view.value) || navItems[0])
const pageTitle = computed(() => activeNav.value.label)

// Theme: persisted, defaults to dark (matches the reference design) or OS pref
const theme = ref('dark')
function applyTheme() {
  document.documentElement.setAttribute('data-theme', theme.value)
}
function toggleTheme() {
  theme.value = theme.value === 'dark' ? 'light' : 'dark'
  try { localStorage.setItem(THEME_KEY, theme.value) } catch (_) { /* ignore */ }
  applyTheme()
}

const config   = ref({})
const name     = ref('')
const login    = ref('')
const password = ref('')
const wifi_ssid = ref('')
const wifi_pass = ref('')
const aes_key  = ref('')
const ble_login = ref('')
const ble_pass  = ref('')
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
const fwFile = ref(null)
const ota = reactive({ uploading: false, progress: 0 })

// noble auth secret is stored server-side as a single "login:password" string
const bleToken = computed(() =>
  (ble_login.value || ble_pass.value) ? `${ble_login.value}:${ble_pass.value}` : ''
)

const configChanged = computed(() => {
  const c = config.value
  return (
    c.name !== name.value ||
    c.login !== login.value ||
    password.value !== '' ||
    c.wifi_ssid !== wifi_ssid.value ||
    c.wifi_pass !== wifi_pass.value ||
    c.aes_key !== aes_key.value ||
    (c.ble_token ?? '') !== bleToken.value ||
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

// Prefer the configured static IP (always resolvable) over the mDNS name,
// which is unreliable on Windows / without Bonjour.
function staticTarget() {
  return IPV4_RE.test(static_ip.value) ? static_ip.value : ''
}

function targetUrl(gwName, ip) {
  return ip ? `https://${ip}` : `https://${gwName}.local`
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
    const tok = data.ble_token ?? ''
    const sep = tok.indexOf(':')
    ble_login.value = sep === -1 ? tok : tok.slice(0, sep)
    ble_pass.value  = sep === -1 ? '' : tok.slice(sep + 1)
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
  // Static IP is all-or-nothing: a partial/invalid set bricks the device.
  const ipSet  = static_ip.value.trim()   !== ''
  const mskSet = static_mask.value.trim() !== ''
  const gwSet  = static_gw.value.trim()   !== ''
  const dnsSet = static_dns.value.trim()  !== ''
  if (ipSet || mskSet || gwSet) {
    if (!ipSet || !mskSet || !gwSet ||
        !IPV4_RE.test(static_ip.value) || !IPV4_RE.test(static_mask.value) ||
        !IPV4_RE.test(static_gw.value) || (dnsSet && !IPV4_RE.test(static_dns.value))) {
      errors.value = ['Static IP: enter a valid IP, mask and gateway (and DNS if set), or leave all empty for DHCP']
      return
    }
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
  if ((c.ble_token ?? '') !== bleToken.value) { payload.ble_token = bleToken.value; c.ble_token = bleToken.value }
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
      startReboot(targetUrl(c.name, staticTarget()), 'Configuration saved — ESP32 rebooting', 12)
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
    startReboot(targetUrl(config.value.name, staticTarget()), 'ESP32 restarting', 12)
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

function onFwSelect(e) {
  fwFile.value = e.target.files && e.target.files[0] ? e.target.files[0] : null
}

function askFlash() {
  if (!fwFile.value) return
  Object.assign(dialog, {
    open: true,
    danger: true,
    title: 'Flash new firmware?',
    message: `Upload "${fwFile.value.name}" (${Math.round(fwFile.value.size / 1024)} KB) and reboot. Do not power off during the update.`,
    confirmLabel: 'Flash & reboot',
    onConfirm: doFlash,
  })
}

// XMLHttpRequest is used instead of fetch to expose upload progress
function uploadFirmware(file) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest()
    xhr.open('POST', '/update', true)
    xhr.withCredentials = true
    xhr.timeout = 120000
    xhr.setRequestHeader('Content-Type', 'application/octet-stream')
    xhr.upload.onprogress = (ev) => {
      if (ev.lengthComputable) ota.progress = Math.round((ev.loaded / ev.total) * 100)
    }
    xhr.onload = () => {
      if (xhr.status === 200 && xhr.responseText.startsWith('OK')) resolve()
      else if (xhr.status === 401) reject(new Error('Authentication failed — check admin login/password'))
      else reject(new Error(xhr.responseText || `Update failed (HTTP ${xhr.status})`))
    }
    xhr.onerror = () => reject(new Error('Cannot reach the ESP32 (network error)'))
    xhr.ontimeout = () => reject(new Error('Timed out — the ESP32 is not responding'))
    xhr.send(file)
  })
}

async function doFlash() {
  if (!fwFile.value) return
  sysBusy.value = true
  errors.value = []
  ota.uploading = true
  ota.progress = 0
  try {
    await uploadFirmware(fwFile.value)
    dialog.open = false
    startReboot(targetUrl(config.value.name, staticTarget()), 'Firmware updated — ESP32 rebooting', 20)
  } catch (e) {
    errors.value.push(e.message || 'Firmware update failed')
  }
  ota.uploading = false
  sysBusy.value = false
}

// --- Bluetooth radar ---
const radarDevices = ref([])
let radarTimer = null

const radarBlips = computed(() => radarDevices.value.map((d) => {
  const rssi = typeof d.rssi === 'number' ? d.rssi : -100
  // map RSSI [-30 (close) .. -100 (far)] to radius [6 .. 108]
  const clamped = Math.min(-30, Math.max(-100, rssi))
  const radius = ((-clamped - 30) / 70) * 102 + 6
  // stable pseudo-angle from the device id hash
  let h = 0
  for (let i = 0; i < d.id.length; i++) h = (h * 31 + d.id.charCodeAt(i)) >>> 0
  const ang = (h % 360) * Math.PI / 180
  return {
    id: d.id,
    name: d.name || '',
    rssi,
    x: +(120 + radius * Math.cos(ang)).toFixed(1),
    y: +(120 + radius * Math.sin(ang)).toFixed(1),
    label: `${d.name || d.id} · ${rssi} dBm`,
  }
}))

async function pollRadar() {
  try {
    const res = await apiFetch('/radar')
    radarDevices.value = await res.json()
  } catch (e) {
    const msg = e.message || 'Radar error'
    if (!errors.value.includes(msg)) errors.value.push(msg)
  }
}

function startRadarPoll() {
  if (radarTimer) return
  pollRadar()
  radarTimer = setInterval(pollRadar, 2000)
}

function stopRadarPoll() {
  if (radarTimer) { clearInterval(radarTimer); radarTimer = null }
}

watch(view, (v) => {
  if (v === 'radar') startRadarPoll()
  else stopRadarPoll()
})

onBeforeUnmount(stopRadarPoll)

let initial = 'dark'
try {
  const saved = localStorage.getItem(THEME_KEY)
  if (saved === 'light' || saved === 'dark') initial = saved
  else if (globalThis.matchMedia && !globalThis.matchMedia('(prefers-color-scheme: dark)').matches) initial = 'light'
} catch (_) { /* ignore */ }
theme.value = initial
applyTheme()

onMounted(loadConfig)
</script>

<style>
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0 }

:root,
[data-theme="dark"] {
  --bg: #0A0A0A;
  --surface: #171717;
  --surface-bright: #1F1F1F;
  --surface-variant: #262626;
  --border: #262626;
  --primary: #10B981;
  --primary-hover: #34D399;
  --on-primary: #052e22;
  --text: #FAFAFA;
  --muted: #A1A1AA;
  --error: #F87171;
  --success: #10B981;
  --radius: 12px;
  --font: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
}

[data-theme="light"] {
  --bg: #FAFAFA;
  --surface: #FFFFFF;
  --surface-bright: #FFFFFF;
  --surface-variant: #F4F4F5;
  --border: #E4E4E7;
  --primary: #10B981;
  --primary-hover: #059669;
  --on-primary: #FFFFFF;
  --text: #171717;
  --muted: #52525B;
  --error: #EF4444;
  --success: #10B981;
}

body { background: var(--bg); color: var(--text); font-family: var(--font) }

.app { display: flex; min-height: 100vh }

/* Sidebar */
.sidebar {
  width: 260px;
  flex-shrink: 0;
  background: var(--surface);
  border-right: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  position: sticky;
  top: 0;
  height: 100vh;
}

.brand {
  display: flex;
  align-items: center;
  gap: .75rem;
  height: 64px;
  padding: 0 1rem;
  border-bottom: 1px solid var(--border);
}
.brand-avatar {
  width: 36px; height: 36px;
  border-radius: 50%;
  background: var(--primary);
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}
.brand-text { overflow: hidden }
.brand-name { font-size: .95rem; font-weight: 700; line-height: 1.2 }
.brand-sub {
  font-size: .75rem; color: var(--muted);
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}

.nav { padding: .75rem .5rem; display: flex; flex-direction: column; gap: .25rem; flex: 1 }
.sidebar-foot {
  padding: .5rem;
  border-top: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  gap: .25rem;
}

.nav-item {
  display: flex;
  align-items: center;
  gap: .75rem;
  width: 100%;
  padding: .6rem .75rem;
  background: transparent;
  border: none;
  border-radius: 8px;
  color: var(--muted);
  font: inherit;
  font-size: .9rem;
  font-weight: 500;
  text-align: left;
  cursor: pointer;
  transition: background .15s, color .15s;
}
.nav-item:hover { background: var(--surface-variant); color: var(--text) }
.nav-item.active { background: rgba(16,185,129,.12); color: var(--primary) }
.nav-item:disabled { opacity: .5; cursor: not-allowed }
.nav-item.danger { color: var(--error) }
.nav-item.danger:hover:not(:disabled) { background: rgba(248,113,113,.12); color: var(--error) }
.nav-icon { display: inline-flex; flex-shrink: 0 }

/* Main */
.main { flex: 1; display: flex; flex-direction: column; min-width: 0 }

.topbar {
  height: 64px;
  flex-shrink: 0;
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 1.5rem;
}
.crumbs { font-size: .95rem; color: var(--muted); font-weight: 500 }
.topbar-actions { display: flex; align-items: center; gap: .5rem }

.chip {
  display: inline-flex;
  align-items: center;
  gap: .4rem;
  font-size: .75rem;
  font-weight: 600;
  padding: .3rem .7rem;
  border-radius: 999px;
}
.chip .dot { width: 7px; height: 7px; border-radius: 50% }
.chip-ok  { background: rgba(16,185,129,.15); color: var(--success) }
.chip-ok  .dot { background: var(--success); box-shadow: 0 0 6px var(--success) }
.chip-err { background: rgba(248,113,113,.15); color: var(--error) }
.chip-err .dot { background: var(--error) }

.content { padding: 1.75rem; max-width: 1100px; width: 100%; margin: 0 auto }

.page-head { margin-bottom: 1.5rem }
.page-head h1 { font-size: 1.5rem; font-weight: 700 }
.page-head p { font-size: .9rem; color: var(--muted); margin-top: .25rem }

.panel {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1.75rem;
  margin-bottom: 1.25rem;
}

.loader-wrap {
  display: flex;
  align-items: center;
  gap: 1rem;
  color: var(--muted);
}

.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1.2rem }
.field { display: flex; flex-direction: column; gap: .4rem }
.field.full { grid-column: 1 / -1 }

label { font-size: .8rem; font-weight: 600; color: var(--muted); text-transform: uppercase; letter-spacing: .04em }

input {
  width: 100%;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 8px;
  color: var(--text);
  padding: .6rem .8rem;
  font-size: .95rem;
  outline: none;
  transition: border-color .15s;
}
input:focus { border-color: var(--primary) }

.input-group { display: flex }
.input-group input { border-radius: 8px 0 0 8px }

.eye-btn {
  background: var(--bg);
  border: 1px solid var(--border);
  border-left: none;
  border-radius: 0 8px 8px 0;
  color: var(--muted);
  padding: 0 .65rem;
  cursor: pointer;
  display: flex;
  align-items: center;
  transition: color .15s;
}
.eye-btn:hover { color: var(--primary) }

.hint { font-size: .78rem; color: var(--muted) }
code { color: var(--primary); font-size: .85em }

.section-sep {
  border-top: 1px solid var(--border);
  padding-top: .75rem;
  margin-top: .25rem;
}
.section-label {
  font-size: .8rem;
  font-weight: 600;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: .04em;
}

.actions { margin-top: 1.75rem; text-align: right }

/* Buttons */
.btn {
  border: 1px solid transparent;
  border-radius: 8px;
  padding: .6rem 1.4rem;
  font-size: .9rem;
  font-weight: 600;
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

.btn-primary { background: var(--primary); color: var(--on-primary) }
.btn-primary:hover:not(:disabled) { background: var(--primary-hover) }

.btn-ghost {
  background: transparent;
  color: var(--text);
  border-color: var(--border);
}
.btn-ghost:hover:not(:disabled) { border-color: var(--primary); color: var(--primary) }

.btn-danger {
  background: transparent;
  color: var(--error);
  border-color: var(--error);
}
.btn-danger:hover:not(:disabled) { background: var(--error); color: #fff }

.fw-label {
  display: block;
  font-size: .8rem;
  font-weight: 600;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: .04em;
  margin-bottom: .5rem;
}
.fw-row {
  display: flex;
  gap: .75rem;
  align-items: center;
  flex-wrap: wrap;
  margin-bottom: .5rem;
}
.fw-row input[type=file] {
  flex: 1;
  min-width: 180px;
  font-size: .85rem;
  color: var(--muted);
}
.fw-row input[type=file]::file-selector-button {
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 6px;
  color: var(--text);
  padding: .4rem .7rem;
  margin-right: .6rem;
  cursor: pointer;
}

.progress {
  height: 8px;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 999px;
  overflow: hidden;
}
.progress .bar {
  height: 100%;
  background: var(--primary);
  transition: width .2s ease;
}

/* Bluetooth radar */
.radar-panel { display: flex; flex-direction: column; align-items: center; gap: 1rem }
.radar-wrap { width: 100%; max-width: 360px }
.radar-svg { width: 100%; height: auto; display: block }
.rd-ring { fill: none; stroke: var(--border); stroke-width: 1 }
.rd-axis { stroke: var(--border); stroke-width: 1; opacity: .5 }
.rd-center { fill: var(--primary) }
.rd-sweep { animation: rd-rotate 4s linear infinite }
.rd-sweep path { fill: var(--primary); opacity: .14 }
.rd-blip { fill: var(--primary); animation: rd-pulse 2s ease-out infinite }
.rd-blip:hover { fill: var(--primary-hover) }
@keyframes rd-rotate { to { transform: rotate(360deg) } }
@keyframes rd-pulse {
  0%   { opacity: .35 }
  50%  { opacity: 1 }
  100% { opacity: .35 }
}
.radar-list {
  list-style: none;
  width: 100%;
  max-width: 360px;
  display: flex;
  flex-direction: column;
  gap: .35rem;
}
.radar-list li {
  display: flex;
  justify-content: space-between;
  gap: 1rem;
  padding: .5rem .7rem;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 8px;
  font-size: .85rem;
}
.rd-li-name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap }
.rd-li-rssi { color: var(--muted); flex-shrink: 0; font-variant-numeric: tabular-nums }
.rd-empty { text-align: center }
.radar-foot { text-align: center }

/* Modal */
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,.7);
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
.modal h2 { font-size: 1.1rem; color: var(--primary) }
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
  border-top-color: var(--primary);
  border-radius: 50%;
  animation: spin .7s linear infinite;
}
.spinner.sm {
  width: 16px; height: 16px;
  border-width: 2px;
  border-color: rgba(255,255,255,.35);
  border-top-color: currentColor;
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

@media (max-width: 720px) {
  .sidebar { width: 64px }
  .brand-text, .nav-item span:not(.nav-icon) { display: none }
  .brand { justify-content: center; padding: 0 }
  .nav-item { justify-content: center }
  .grid { grid-template-columns: 1fr }
  .field.full { grid-column: 1 }
  .content { padding: 1.25rem }
}
</style>
