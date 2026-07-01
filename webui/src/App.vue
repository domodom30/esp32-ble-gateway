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
          <span>{{ t('sidebar.restart') }}</span>
        </button>
        <button type="button" class="nav-item danger" :disabled="sysBusy" @click="askFactoryReset">
          <span class="nav-icon" v-html="icons.factory"></span>
          <span>{{ t('sidebar.reset') }}</span>
        </button>
        <button type="button" class="nav-item" @click="toggleLocale">
          <span class="nav-icon" v-html="icons.lang"></span>
          <span>{{ t('sidebar.langTarget') }}</span>
        </button>
        <button type="button" class="nav-item" @click="toggleTheme">
          <span class="nav-icon" v-html="theme === 'dark' ? icons.sun : icons.moon"></span>
          <span>{{ theme === 'dark' ? t('sidebar.themeLight') : t('sidebar.themeDark') }}</span>
        </button>
      </div>
    </aside>

    <!-- Main -->
    <div class="main">
      <header class="topbar">
        <div class="crumbs">{{ pageTitle }}</div>
        <div class="topbar-actions">
          <span class="chip" :class="online ? 'chip-ok' : 'chip-err'" :title="online ? t('status.onlineTitle') : t('status.offlineTitle')">
            <span class="dot"></span>{{ online ? t('status.online') : t('status.offline') }}
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
          <span>{{ t('common.loading') }}</span>
        </div>

        <!-- Identifiants : accès admin + secrets -->
        <form v-else-if="view === 'identifiants'" class="panel" @submit.prevent="saveConfig" novalidate>
          <div class="grid">
            <div class="field">
              <label for="login">{{ t('cred.adminLogin') }}</label>
              <input id="login" type="text" v-model="login" placeholder="admin" autocomplete="username" />
            </div>

            <div class="field">
              <label for="password">{{ t('cred.adminPassword') }}</label>
              <div class="input-group">
                <input id="password" :type="show.password ? 'text' : 'password'" v-model="password" :placeholder="t('cred.leaveEmpty')" />
                <button type="button" class="eye-btn" @click="show.password = !show.password" :aria-label="show.password ? t('common.hide') : t('common.show')">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                    <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                    <circle cx="12" cy="12" r="3"/>
                    <line v-if="show.password" x1="2" y1="2" x2="22" y2="22"/>
                  </svg>
                </button>
              </div>
            </div>

            <div class="field full">
              <label for="aes">{{ t('cred.aesKey') }}</label>
              <div class="input-group">
                <input id="aes" :type="show.aes ? 'text' : 'password'" v-model="aes_key" :placeholder="t('cred.aesPlaceholder')" />
                <button type="button" class="eye-btn" @click="show.aes = !show.aes" :aria-label="show.aes ? t('common.hide') : t('common.show')">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                    <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                    <circle cx="12" cy="12" r="3"/>
                    <line v-if="show.aes" x1="2" y1="2" x2="22" y2="22"/>
                  </svg>
                </button>
              </div>
              <span class="hint">{{ t('cred.aesHint') }}</span>
            </div>

            <div class="field">
              <label for="ble_login">{{ t('cred.bleLogin') }}</label>
              <input id="ble_login" type="text" v-model="ble_login" placeholder="admin" autocomplete="off" />
            </div>

            <div class="field">
              <label for="ble_pass">{{ t('cred.blePassword') }}</label>
              <div class="input-group">
                <input id="ble_pass" :type="show.ble ? 'text' : 'password'" v-model="ble_pass" placeholder="admin" />
                <button type="button" class="eye-btn" @click="show.ble = !show.ble" :aria-label="show.ble ? t('common.hide') : t('common.show')">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                    <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                    <circle cx="12" cy="12" r="3"/>
                    <line v-if="show.ble" x1="2" y1="2" x2="22" y2="22"/>
                  </svg>
                </button>
              </div>
            </div>

            <div class="field full">
              <span class="hint">{{ t('cred.bleHint') }}</span>
            </div>
          </div>

          <div class="actions">
            <button type="submit" class="btn btn-primary" :class="{ saving }" :disabled="!configChanged || saving">
              <span v-if="saving" class="spinner sm"></span>
              <span>{{ saving ? t('common.saving') : t('common.save') }}</span>
            </button>
          </div>
        </form>

        <!-- Réseau : WiFi + configuration réseau -->
        <form v-else-if="view === 'reseau'" class="panel" @submit.prevent="saveConfig" novalidate>
          <div class="grid">
            <div class="field full">
              <label for="name">{{ t('net.gatewayName') }}</label>
              <input id="name" type="text" v-model="name" placeholder="my-gateway" />
              <span class="hint" v-if="name.length">MDNS: <code>{{ name }}.local</code></span>
            </div>

            <div class="field">
              <label for="ssid">{{ t('net.wifiSsid') }}</label>
              <input id="ssid" type="text" v-model="wifi_ssid" :placeholder="t('net.wifiSsidPlaceholder')" />
            </div>

            <div class="field">
              <label for="wpass">{{ t('net.wifiPassword') }}</label>
              <div class="input-group">
                <input id="wpass" :type="show.wifi ? 'text' : 'password'" v-model="wifi_pass" :placeholder="t('net.wifiPasswordPlaceholder')" />
                <button type="button" class="eye-btn" @click="show.wifi = !show.wifi" :aria-label="show.wifi ? t('common.hide') : t('common.show')">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18">
                    <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                    <circle cx="12" cy="12" r="3"/>
                    <line v-if="show.wifi" x1="2" y1="2" x2="22" y2="22"/>
                  </svg>
                </button>
              </div>
            </div>

            <div class="field full section-sep">
              <span class="section-label">{{ t('net.staticIp') }} <code>{{ t('net.staticIpOptional') }}</code></span>
            </div>

            <div class="field">
              <label for="static_ip">{{ t('net.ipAddress') }}</label>
              <input id="static_ip" type="text" v-model="static_ip" placeholder="192.168.1.50" />
            </div>

            <div class="field">
              <label for="static_mask">{{ t('net.subnetMask') }}</label>
              <input id="static_mask" type="text" v-model="static_mask" placeholder="255.255.255.0" />
            </div>

            <div class="field">
              <label for="static_gw">{{ t('net.gateway') }}</label>
              <input id="static_gw" type="text" v-model="static_gw" placeholder="192.168.1.1" />
            </div>

            <div class="field">
              <label for="static_dns">{{ t('net.dnsServer') }}</label>
              <input id="static_dns" type="text" v-model="static_dns" placeholder="8.8.8.8" />
            </div>
          </div>

          <div class="actions">
            <button type="submit" class="btn btn-primary" :class="{ saving }" :disabled="!configChanged || saving">
              <span v-if="saving" class="spinner sm"></span>
              <span>{{ saving ? t('common.saving') : t('common.save') }}</span>
            </button>
          </div>
        </form>

        <!-- OTA : mise à jour firmware -->
        <div v-else-if="view === 'ota'" class="panel">
          <div class="fw-update">
            <label class="fw-label">{{ t('ota.title') }} <code>(.bin)</code></label>
            <div class="fw-row">
              <input type="file" accept=".bin" @change="onFwSelect" :disabled="sysBusy" />
              <button type="button" class="btn btn-ghost" :disabled="!fwFile || sysBusy" @click="askFlash">
                <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/>
                </svg>
                {{ t('ota.flash') }}
              </button>
            </div>
            <span class="hint">{{ t('ota.hint') }}</span>
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
        <p v-if="ota.preparing">{{ t('dlg.preparing') }}</p>
        <p v-if="ota.uploading">{{ t('dlg.uploading', { n: ota.progress }) }}</p>
        <div class="modal-actions">
          <button type="button" class="btn btn-ghost" @click="closeDialog" :disabled="sysBusy">{{ t('common.cancel') }}</button>
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
        <p>{{ reboot.seconds > 0 ? t('reboot.redirecting', { s: reboot.seconds }) : t('reboot.waiting') }}</p>
        <a class="btn btn-primary" :href="reboot.url">{{ t('reboot.openNow') }}</a>
      </modal>
    </div>

    <!-- Toasts -->
    <div class="toast-area" v-if="errors.length || notice">
      <div v-if="errors.length" class="toast error">
        <ul>
          <li v-for="e in errors" :key="e">{{ e }}</li>
        </ul>
        <button class="close-btn" @click="errors = []" :aria-label="t('common.close')">✕</button>
      </div>
      <div v-if="notice" class="toast success">
        <span>{{ notice }}</span>
        <button class="close-btn" @click="notice = ''" :aria-label="t('common.close')">✕</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { version } from '../package.json'

const REQUEST_TIMEOUT = 8000
const THEME_KEY = 'ttlock_theme'
const IPV4_RE = /^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$/

const icons = {
  credentials: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><circle cx="8" cy="15" r="4"/><path d="M10.8 12.2 20 3M16 6l3 3M13 9l2.5 2.5"/></svg>',
  network: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M2 9a16 16 0 0 1 20 0"/><path d="M5 12.5a10 10 0 0 1 14 0"/><path d="M8.5 16a5 5 0 0 1 7 0"/><line x1="12" y1="19.5" x2="12.01" y2="19.5"/></svg>',
  ota: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/></svg>',
  restart: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M23 4v6h-6M1 20v-6h6"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/></svg>',
  factory: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>',
  sun: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>',
  moon: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>',
  lang: '<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M2 12h20"/><path d="M12 2a15 15 0 0 1 0 20M12 2a15 15 0 0 0 0 20"/></svg>',
}

const LOCALE_KEY = 'ttlock_locale'
const messages = {
  en: {
    'nav.identifiants.label': 'Credentials',
    'nav.identifiants.subtitle': 'Administrator access and encryption secrets',
    'nav.reseau.label': 'Network',
    'nav.reseau.subtitle': 'WiFi and gateway network configuration',
    'nav.ota.label': 'OTA',
    'nav.ota.subtitle': 'Firmware update',

    'sidebar.restart': 'Restart',
    'sidebar.reset': 'Reset',
    'sidebar.themeLight': 'Light',
    'sidebar.themeDark': 'Dark',
    'sidebar.langTarget': 'Français',

    'status.online': 'Online',
    'status.offline': 'Offline',
    'status.onlineTitle': 'Gateway online',
    'status.offlineTitle': 'Gateway offline',

    'common.loading': 'Loading configuration…',
    'common.save': 'Save',
    'common.saving': 'Saving…',
    'common.cancel': 'Cancel',
    'common.show': 'Show',
    'common.hide': 'Hide',
    'common.close': 'Close',

    'cred.adminLogin': 'Admin login',
    'cred.adminPassword': 'Admin password',
    'cred.leaveEmpty': 'Leave empty to keep',
    'cred.aesKey': 'AES Key',
    'cred.aesPlaceholder': '32 hex chars',
    'cred.aesHint': '16-byte key — exactly 32 hexadecimal characters',
    'cred.bleLogin': 'BLE login',
    'cred.blePassword': 'BLE password',
    'cred.bleHint': 'Secret required by BLE clients (noble). Independent of the web admin login.',

    'net.gatewayName': 'Gateway name',
    'net.wifiSsid': 'WiFi SSID',
    'net.wifiSsidPlaceholder': 'Network name',
    'net.wifiPassword': 'WiFi password',
    'net.wifiPasswordPlaceholder': 'Network password',
    'net.staticIp': 'Static IP',
    'net.staticIpOptional': '(optional — leave empty for DHCP)',
    'net.ipAddress': 'IP address',
    'net.subnetMask': 'Subnet mask',
    'net.gateway': 'Gateway',
    'net.dnsServer': 'DNS server',

    'ota.title': 'Firmware update',
    'ota.flash': 'Flash',
    'ota.hint': 'Upload the compiled binary (.bin); the device reboots after the update. Do not power off.',

    'dlg.restartTitle': 'Restart the gateway?',
    'dlg.restartMsg': 'The ESP32 will reboot. The web interface will be unavailable for a few seconds.',
    'dlg.restartConfirm': 'Restart',
    'dlg.factoryTitle': 'Factory reset?',
    'dlg.factoryMsg': 'All settings (WiFi, login, AES key, static IP…) will be erased and the device will reboot in setup mode. This cannot be undone.',
    'dlg.factoryConfirm': 'Erase & reset',
    'dlg.flashTitle': 'Flash new firmware?',
    'dlg.flashMsg': 'Upload "{name}" ({size} KB) and reboot. Do not power off during the update.',
    'dlg.flashConfirm': 'Flash & reboot',
    'dlg.preparing': 'Preparing… (freeing BLE memory)',
    'dlg.uploading': 'Uploading… {n}%',

    'reboot.redirecting': 'Redirecting in {s}s…',
    'reboot.waiting': 'Reconnecting…',
    'reboot.openNow': 'Open now',
    'reboot.configSaved': 'Configuration saved — ESP32 rebooting',
    'reboot.restarting': 'ESP32 restarting',
    'reboot.factoryDone': 'Factory reset done — device rebooting',
    'reboot.firmwareDone': 'Firmware updated — ESP32 rebooting',

    'err.auth': 'Authentication failed — check admin login/password',
    'err.http': 'Request failed (HTTP {status})',
    'err.timeout': 'Timed out — the ESP32 is not responding',
    'err.network': 'Cannot reach the ESP32 (network error)',
    'err.fetchConfig': 'Error fetching configuration',
    'err.aesFormat': 'AES key must be exactly 32 hexadecimal characters',
    'err.staticIp': 'Static IP: enter a valid IP, mask and gateway (and DNS if set), or leave all empty for DHCP',
    'err.saveConfig': 'Error saving configuration',
    'err.restart': 'Restart failed',
    'err.factory': 'Factory reset failed',
    'err.updateHttp': 'Update failed (HTTP {status})',
    'err.firmware': 'Firmware update failed',
  },
  fr: {
    'nav.identifiants.label': 'Identifiants',
    'nav.identifiants.subtitle': 'Accès administrateur et secrets de chiffrement',
    'nav.reseau.label': 'Réseau',
    'nav.reseau.subtitle': 'WiFi et configuration réseau de la passerelle',
    'nav.ota.label': 'OTA',
    'nav.ota.subtitle': 'Mise à jour du firmware',

    'sidebar.restart': 'Redémarrer',
    'sidebar.reset': 'Réinitialiser',
    'sidebar.themeLight': 'Clair',
    'sidebar.themeDark': 'Sombre',
    'sidebar.langTarget': 'English',

    'status.online': 'En ligne',
    'status.offline': 'Hors ligne',
    'status.onlineTitle': 'Gateway en ligne',
    'status.offlineTitle': 'Gateway hors ligne',

    'common.loading': 'Chargement de la configuration…',
    'common.save': 'Sauvegarder',
    'common.saving': 'Sauvegarde…',
    'common.cancel': 'Annuler',
    'common.show': 'Afficher',
    'common.hide': 'Masquer',
    'common.close': 'Fermer',

    'cred.adminLogin': 'Identifiant admin',
    'cred.adminPassword': 'Mot de passe admin',
    'cred.leaveEmpty': 'Laisser vide pour conserver',
    'cred.aesKey': 'Clé AES',
    'cred.aesPlaceholder': '32 caractères hex',
    'cred.aesHint': 'Clé de 16 octets — exactement 32 caractères hexadécimaux',
    'cred.bleLogin': 'Identifiant BLE',
    'cred.blePassword': 'Mot de passe BLE',
    'cred.bleHint': 'Secret requis par les clients BLE (noble). Indépendant de l\'identifiant admin web.',

    'net.gatewayName': 'Nom de la passerelle',
    'net.wifiSsid': 'SSID WiFi',
    'net.wifiSsidPlaceholder': 'Nom du réseau',
    'net.wifiPassword': 'Mot de passe WiFi',
    'net.wifiPasswordPlaceholder': 'Mot de passe du réseau',
    'net.staticIp': 'IP statique',
    'net.staticIpOptional': '(optionnel — laisser vide pour DHCP)',
    'net.ipAddress': 'Adresse IP',
    'net.subnetMask': 'Masque de sous-réseau',
    'net.gateway': 'Passerelle',
    'net.dnsServer': 'Serveur DNS',

    'ota.title': 'Mise à jour firmware',
    'ota.flash': 'Flash',
    'ota.hint': 'Téléversez le binaire compilé (.bin) ; l\'appareil redémarre après la mise à jour. Ne coupez pas l\'alimentation.',

    'dlg.restartTitle': 'Redémarrer la passerelle ?',
    'dlg.restartMsg': 'L\'ESP32 va redémarrer. L\'interface web sera indisponible quelques secondes.',
    'dlg.restartConfirm': 'Redémarrer',
    'dlg.factoryTitle': 'Réinitialisation d\'usine ?',
    'dlg.factoryMsg': 'Tous les réglages (WiFi, identifiant, clé AES, IP statique…) seront effacés et l\'appareil redémarrera en mode configuration. Cette action est irréversible.',
    'dlg.factoryConfirm': 'Effacer & réinitialiser',
    'dlg.flashTitle': 'Flasher le nouveau firmware ?',
    'dlg.flashMsg': 'Téléverser "{name}" ({size} Ko) et redémarrer. Ne coupez pas l\'alimentation pendant la mise à jour.',
    'dlg.flashConfirm': 'Flasher & redémarrer',
    'dlg.preparing': 'Préparation… (libération de la mémoire BLE)',
    'dlg.uploading': 'Envoi… {n} %',

    'reboot.redirecting': 'Redirection dans {s} s…',
    'reboot.waiting': 'Reconnexion en cours…',
    'reboot.openNow': 'Ouvrir maintenant',
    'reboot.configSaved': 'Configuration enregistrée — redémarrage de l\'ESP32',
    'reboot.restarting': 'Redémarrage de l\'ESP32',
    'reboot.factoryDone': 'Réinitialisation effectuée — redémarrage de l\'appareil',
    'reboot.firmwareDone': 'Firmware mis à jour — redémarrage de l\'ESP32',

    'err.auth': 'Échec de l\'authentification — vérifiez l\'identifiant/mot de passe admin',
    'err.http': 'Échec de la requête (HTTP {status})',
    'err.timeout': 'Délai dépassé — l\'ESP32 ne répond pas',
    'err.network': 'Impossible de joindre l\'ESP32 (erreur réseau)',
    'err.fetchConfig': 'Erreur lors de la récupération de la configuration',
    'err.aesFormat': 'La clé AES doit contenir exactement 32 caractères hexadécimaux',
    'err.staticIp': 'IP statique : saisissez une IP, un masque et une passerelle valides (et un DNS si renseigné), ou laissez tout vide pour DHCP',
    'err.saveConfig': 'Erreur lors de l\'enregistrement de la configuration',
    'err.restart': 'Échec du redémarrage',
    'err.factory': 'Échec de la réinitialisation',
    'err.updateHttp': 'Échec de la mise à jour (HTTP {status})',
    'err.firmware': 'Échec de la mise à jour du firmware',
  },
}

const locale = ref('en')
function t(key, params) {
  let s = (messages[locale.value] && messages[locale.value][key]) ?? messages.en[key] ?? key
  if (params) for (const k in params) s = s.replaceAll(`{${k}}`, params[k])
  return s
}
function applyLocale() {
  document.documentElement.setAttribute('lang', locale.value)
}
function setLocale(l) {
  locale.value = l
  try { localStorage.setItem(LOCALE_KEY, l) } catch (_) { /* ignore */ }
  applyLocale()
}
function toggleLocale() {
  setLocale(locale.value === 'en' ? 'fr' : 'en')
}

const navItems = computed(() => [
  { id: 'identifiants', label: t('nav.identifiants.label'), icon: icons.credentials, subtitle: t('nav.identifiants.subtitle') },
  { id: 'reseau',       label: t('nav.reseau.label'),       icon: icons.network,     subtitle: t('nav.reseau.subtitle') },
  { id: 'ota',          label: t('nav.ota.label'),          icon: icons.ota,         subtitle: t('nav.ota.subtitle') },
])

const view = ref('identifiants')
const activeNav = computed(() => navItems.value.find(n => n.id === view.value) || navItems.value[0])
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
const ota = reactive({ uploading: false, preparing: false, progress: 0 })

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

// Build a tagged Error so callers can tell an expected reboot-disconnect
// (timeout/network) apart from a real failure (auth/http).
function apiError(msgKey, kind, params) {
  const e = new Error(t(msgKey, params))
  e.kind = kind
  return e
}

// A dropped connection or timeout after a reboot-triggering action is the
// expected outcome, not a failure.
function isConnLost(e) {
  return e && (e.kind === 'timeout' || e.kind === 'network')
}

// fetch wrapper: aborts on timeout and maps failures to readable messages
async function apiFetch(url, opts = {}) {
  const ctrl = new AbortController()
  const timer = setTimeout(() => ctrl.abort(), REQUEST_TIMEOUT)
  try {
    const res = await fetch(url, { credentials: 'include', signal: ctrl.signal, ...opts })
    if (res.status === 401) throw apiError('err.auth', 'auth')
    if (!res.ok) throw apiError('err.http', 'http', { status: res.status })
    return res
  } catch (e) {
    if (e.name === 'AbortError') throw apiError('err.timeout', 'timeout')
    if (e instanceof TypeError) throw apiError('err.network', 'network')
    throw e
  } finally {
    clearTimeout(timer)
  }
}

// The device comes back at the address we're already on. Prefer it over the
// mDNS name (.local is unreliable on Windows / without Bonjour); probing it is a
// same-origin request, so no self-signed-cert hurdle. A configured static IP is
// preferred in case the device moves there after reboot.
function rebootTarget() {
  return IPV4_RE.test(static_ip.value) ? `https://${static_ip.value}` : globalThis.location.origin
}

function targetUrl(gwName, ip) {
  return ip ? `https://${ip}` : `https://${gwName}.local`
}

// Show the reboot overlay, then redirect only once the device answers again —
// not on a blind countdown that can land on a still-booting server (which the
// browser reports as a timeout, making a healthy device look dead).
function startReboot(url, title, seconds) {
  reboot.url = url
  reboot.title = title
  reboot.seconds = seconds
  reboot.active = true
  let done = false
  let probe = null
  const go = () => {
    if (done) return
    done = true
    clearInterval(tick)
    if (probe) clearInterval(probe)
    globalThis.location.href = url
  }
  // Countdown is now purely indicative; at 0 we keep waiting (no premature
  // redirect) — the "Open now" link stays available for a manual jump.
  const tick = setInterval(() => {
    if (reboot.seconds > 0) reboot.seconds -= 1
  }, 1000)
  // Grace delay so we don't catch the old server still up (flush window ~700ms
  // + shutdown), which would redirect before the reboot even started.
  setTimeout(() => {
    probe = setInterval(async () => {
      try {
        await fetch(url + '/config', { credentials: 'include', cache: 'no-store' })
        go() // any response (even 401) means the server is serving again
      } catch (_) { /* still rebooting */ }
    }, 1500)
  }, 4000)
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
    errors.value.push(e.message || t('err.fetchConfig'))
  }
}

async function saveConfig() {
  if (!configChanged.value || saving.value) return
  if (aes_key.value !== '' && !/^[0-9a-fA-F]{32}$/.test(aes_key.value)) {
    errors.value = [t('err.aesFormat')]
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
      errors.value = [t('err.staticIp')]
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
      startReboot(rebootTarget(), t('reboot.configSaved'), 12)
    } else {
      errors.value.push(t('err.saveConfig'))
    }
  } catch (e) {
    // Connexion coupée après l'envoi = l'ESP32 a enregistré puis redémarre.
    if (isConnLost(e)) {
      config.value = c
      startReboot(rebootTarget(), t('reboot.configSaved'), 12)
    } else {
      errors.value.push(e.message || t('err.saveConfig'))
    }
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
    title: t('dlg.restartTitle'),
    message: t('dlg.restartMsg'),
    confirmLabel: t('dlg.restartConfirm'),
    onConfirm: doRestart,
  })
}

function askFactoryReset() {
  Object.assign(dialog, {
    open: true,
    danger: true,
    title: t('dlg.factoryTitle'),
    message: t('dlg.factoryMsg'),
    confirmLabel: t('dlg.factoryConfirm'),
    onConfirm: doFactoryReset,
  })
}

async function doRestart() {
  sysBusy.value = true
  errors.value = []
  try {
    await apiFetch('/restart')
  } catch (e) {
    // Connexion coupée = l'ESP32 redémarre (attendu) ; sinon vraie erreur.
    if (!isConnLost(e)) {
      errors.value.push(e.message || t('err.restart'))
      sysBusy.value = false
      return
    }
  }
  dialog.open = false
  startReboot(rebootTarget(), t('reboot.restarting'), 12)
  sysBusy.value = false
}

async function doFactoryReset() {
  sysBusy.value = true
  errors.value = []
  try {
    await apiFetch('/factoryReset')
  } catch (e) {
    // Connexion coupée = l'ESP32 redémarre (attendu) ; sinon vraie erreur.
    if (!isConnLost(e)) {
      errors.value.push(e.message || t('err.factory'))
      sysBusy.value = false
      return
    }
  }
  dialog.open = false
  // après reset le nom revient à la valeur par défaut et le WiFi est effacé
  startReboot(targetUrl('esp32gw'), t('reboot.factoryDone'), 20)
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
    title: t('dlg.flashTitle'),
    message: t('dlg.flashMsg', { name: fwFile.value.name, size: Math.round(fwFile.value.size / 1024) }),
    confirmLabel: t('dlg.flashConfirm'),
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
    // Une fois tout le binaire transmis, si la connexion tombe sans réponse
    // c'est que l'ESP32 a flashé puis redémarré : succès, pas une erreur.
    let uploaded = false
    xhr.upload.onprogress = (ev) => {
      if (ev.lengthComputable) ota.progress = Math.round((ev.loaded / ev.total) * 100)
    }
    xhr.upload.onload = () => { uploaded = true }
    xhr.onload = () => {
      if (xhr.status === 200 && xhr.responseText.startsWith('OK')) resolve()
      else if (xhr.status === 401) reject(new Error(t('err.auth')))
      else reject(new Error(xhr.responseText || t('err.updateHttp', { status: xhr.status })))
    }
    xhr.onerror = () => uploaded ? resolve() : reject(new Error(t('err.network')))
    xhr.ontimeout = () => uploaded ? resolve() : reject(new Error(t('err.timeout')))
    xhr.send(file)
  })
}

async function doFlash() {
  if (!fwFile.value) return
  sysBusy.value = true
  errors.value = []
  ota.progress = 0
  try {
    // Libère la pile BLE côté ESP32 avant l'upload : sans ça, le heap est trop
    // bas pour absorber le flux TLS et l'upload gèle vers ~35 %.
    ota.preparing = true
    await apiFetch('/update/prepare')
    ota.preparing = false
    ota.uploading = true
    await uploadFirmware(fwFile.value)
    dialog.open = false
    startReboot(rebootTarget(), t('reboot.firmwareDone'), 20)
  } catch (e) {
    errors.value.push(e.message || t('err.firmware'))
  }
  ota.preparing = false
  ota.uploading = false
  sysBusy.value = false
}

let initial = 'dark'
try {
  const saved = localStorage.getItem(THEME_KEY)
  if (saved === 'light' || saved === 'dark') initial = saved
  else if (globalThis.matchMedia && !globalThis.matchMedia('(prefers-color-scheme: dark)').matches) initial = 'light'
} catch (_) { /* ignore */ }
theme.value = initial
applyTheme()

// Locale : anglais par défaut, choix manuel mémorisé prioritaire
try {
  const savedLocale = localStorage.getItem(LOCALE_KEY)
  if (savedLocale === 'en' || savedLocale === 'fr') locale.value = savedLocale
} catch (_) { /* ignore */ }
applyLocale()

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
  --primary: #2563EB;
  --primary-hover: #3B82F6;
  --on-primary: #FFFFFF;
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
  --primary: #2563EB;
  --primary-hover: #1D4ED8;
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
.nav-item.active { background: rgba(37,99,235,.12); color: var(--primary) }
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
