<template>
  <div class="page">
    <div class="card">
      <h1 class="title">
        <svg viewBox="0 0 24 24" class="logo-icon" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M12 2L8 6l4 4-4 4 4 4M14 6l4 4-4 4"/>
        </svg>
        BLE Gateway
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
              <input id="aes" :type="show.aes ? 'text' : 'password'" v-model="aes_key" placeholder="AES encryption key" />
              <button type="button" class="eye-btn" @click="show.aes = !show.aes" :aria-label="show.aes ? 'Hide' : 'Show'">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                  <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                  <circle cx="12" cy="12" r="3"/>
                  <line v-if="show.aes" x1="2" y1="2" x2="22" y2="22"/>
                </svg>
              </button>
            </div>
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
          <button type="submit" class="save-btn" :class="{ saving }" :disabled="!configChanged || saving">
            <span v-if="saving" class="spinner sm"></span>
            <span>{{ saving ? 'Saving…' : 'Save configuration' }}</span>
          </button>
        </div>
      </form>
    </div>

    <!-- Toasts -->
    <div class="toast-area" v-if="errors.length || savingSuccess">
      <div v-if="errors.length" class="toast error">
        <ul>
          <li v-for="e in errors" :key="e">{{ e }}</li>
        </ul>
        <button class="close-btn" @click="errors = []" aria-label="Close">✕</button>
      </div>
      <div v-if="savingSuccess" class="toast success">
        <span>✓ Configuration saved. ESP rebooting…</span>
        <button class="close-btn" @click="savingSuccess = false" aria-label="Close">✕</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'

const config   = ref({})
const name     = ref('')
const login    = ref('')
const password = ref('')
const wifi_ssid = ref('')
const wifi_pass = ref('')
const aes_key  = ref('')
const static_ip   = ref('')
const static_mask = ref('')
const static_gw   = ref('')
const static_dns  = ref('')
const ready    = ref(false)
const errors   = ref([])
const saving   = ref(false)
const savingSuccess = ref(false)
const show = reactive({ password: false, wifi: false, aes: false })

const configChanged = computed(() => {
  const c = config.value
  return (
    c.name !== name.value ||
    c.login !== login.value ||
    password.value !== '' ||
    c.wifi_ssid !== wifi_ssid.value ||
    c.wifi_pass !== wifi_pass.value ||
    c.aes_key !== aes_key.value ||
    (c.static_ip   ?? '') !== static_ip.value ||
    (c.static_mask ?? '') !== static_mask.value ||
    (c.static_gw   ?? '') !== static_gw.value ||
    (c.static_dns  ?? '') !== static_dns.value
  )
})

async function loadConfig() {
  try {
    const res = await fetch('/config', { credentials: 'include' })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    const data = await res.json()
    config.value   = data
    name.value     = data.name
    login.value    = data.login ?? 'admin'
    wifi_ssid.value = data.wifi_ssid
    wifi_pass.value = data.wifi_pass
    aes_key.value  = data.aes_key
    static_ip.value   = data.static_ip   ?? ''
    static_mask.value = data.static_mask ?? ''
    static_gw.value   = data.static_gw   ?? ''
    static_dns.value  = data.static_dns  ?? ''
    ready.value    = true
  } catch (e) {
    errors.value.push(e.message || 'Error fetching configuration')
  }
}

async function saveConfig() {
  if (!configChanged.value || saving.value) return
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
  // IP statique : toujours envoyée pour permettre la suppression (vide = DHCP)
  payload.static_ip   = static_ip.value
  payload.static_mask = static_mask.value
  payload.static_gw   = static_gw.value
  payload.static_dns  = static_dns.value
  try {
    const res = await fetch('/config', {
      method: 'POST',
      credentials: 'include',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    })
    const text = await res.text()
    if (text === 'OK') {
      config.value = c
      savingSuccess.value = true
      setTimeout(() => { globalThis.location.href = `https://${c.name}.local` }, 2000)
    } else {
      errors.value.push('Error saving configuration')
    }
  } catch (e) {
    errors.value.push(e.message || 'Error saving configuration')
  }
  saving.value = false
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
.section-opt {
  text-transform: none;
  letter-spacing: 0;
  font-weight: 400;
  font-size: .78rem;
  color: var(--border);
}

.actions { margin-top: 1.75rem; text-align: center }

.save-btn {
  background: var(--accent);
  color: #0f172a;
  border: none;
  border-radius: 6px;
  padding: .65rem 2rem;
  font-size: .95rem;
  font-weight: 700;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: .5rem;
  transition: background .15s, opacity .15s;
}
.save-btn:hover:not(:disabled) { background: var(--accent-hover) }
.save-btn:disabled { opacity: .45; cursor: not-allowed }

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
}
</style>