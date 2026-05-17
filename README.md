# 📡 esp32-ble-gateway

[![Platform](https://img.shields.io/badge/platform-ESP32--WROVER-blue?logo=espressif)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![NimBLE](https://img.shields.io/badge/NimBLE--Arduino-2.x-green)](https://github.com/h2zero/NimBLE-Arduino)
[![WebUI](https://img.shields.io/badge/WebUI-Vue%203%20%2B%20Vite-42b883?logo=vue.js)](https://vuejs.org/)
[![License](https://img.shields.io/github/license/domodom30/esp32-ble-gateway)](LICENSE)

> _Fork of [kind3r/esp32-ble-gateway](https://github.com/kind3r/esp32-ble-gateway) with modernized stack and new features._

---

## Overview

Bridges WiFi and BLE using a modified version of [Noble](https://github.com/abandonware/noble)'s
[WebSocket protocol](https://github.com/abandonware/noble/blob/master/ws-slave.js).
Designed to work with [ttlock-sdk-js](https://github.com/kind3r/ttlock-sdk-js) and the
[TTLock Home Assistant addon](https://github.com/kind3r/hass-addons).

Data flows over **standard WebSocket** (unencrypted) — the ESP32 cannot handle BLE + WiFi + TLS
simultaneously, and since the gateway is inside your LAN, BLE traffic can already be sniffed over the air.

> ℹ️ This is **not** related to the official TTLock G2 gateway — it is a GATT proxy over WiFi.
>
> Feeling generous? Support the original author: [PayPal](https://paypal.me/kind3r)

---

## What's new in this fork

| Change | Details |
| --- | --- |
| 🔧 **NimBLE-Arduino 2.x** | Migrated from the standard Arduino BLE library |
| 🖥️ **Vue 3 + Vite WebUI** | Replaced Vue 2 / Bootstrap — single-file build, no CDN |
| 🔑 **Configurable credentials** | Admin login & password editable via the web interface |
| 🌐 **Static IP support** | Optional static IP / mask / gateway / DNS (DHCP by default) |
| ⚙️ **No `sdkconfig.h` edit** | Stack size set via `build_flags` in `platformio.ini` |
| 📦 **Custom partitions** | `partitions_custom.csv` (OTA-ready: `app0` / `app1` + `otadata`) |
| 🔒 **RSA-1024 certificate** | Avoids stack overflow during TLS cert generation on first boot |
| 🔄 **OTA firmware updates** | Upload a new `.bin` from the web interface — no serial cable needed |
| 🛡️ **Dedicated BLE auth secret** | WebSocket clients authenticate with a separate login/password, independent of the web admin login |
| 🩹 **Memory-corruption fixes** | Fixed crash on first configuration, stack overflows and out-of-bounds writes |
| 📡 **WiFi resilience** | Auto-reconnect watchdog + disconnect reason-code logging |
| ⏱️ **WebSocket auth timeout** | Unauthenticated WebSocket clients are dropped after 10 s |
| 🧱 **ArduinoJson 7** | Migrated from v6 — elastic documents, lower stack pressure |
| ♻️ **Restart & factory reset** | Triggerable directly from the sidebar |
| 📡 **Bluetooth radar** | Animated web view of nearby BLE devices (RSSI), with a self-contained scan — no Home Assistant addon required |

---

## Features

- ✅ WiFi configuration via HTTPS web interface (AP mode on first boot)
- ✅ WebSocket communication with AES-128-CBC authentication
- ✅ Dedicated BLE auth secret (separate from the web admin login)
- ✅ BLE scan start / stop
- ✅ Device discovery
- ✅ Read / Write GATT characteristics
- ✅ Subscribe to characteristic notifications
- ✅ OTA firmware update via the web interface
- ✅ Remote restart & factory reset from the sidebar
- ✅ Bluetooth radar — animated nearby-device view (auth-protected, autonomous scan)
- ✅ Automatic WiFi reconnection
- ✅ Unauthenticated WebSocket clients dropped after 10 s

---

## Installation

> **Requirements:** [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/),
> an **ESP32-WROVER** board, and a TTLock device paired with Home Assistant.

### 1 — Flash the filesystem

Open the project in VS Code. PlatformIO will automatically install all dependencies.

In **PlatformIO Project Tasks**, go to:

```text
env:esp-wrover > Platform > Upload Filesystem Image
```

This uploads the web interface (~33 kB gzipped) to the ESP32's SPIFFS partition.

### 2 — Flash the firmware

```text
env:esp-wrover > General > Upload and Monitor
```

> On **first boot**, the ESP32 generates a self-signed RSA-1024 TLS certificate.
> This can take up to ~30 seconds — be patient.

### 3 — First configuration

1. Connect to the WiFi AP **`ESP32GW`** (password: `87654321`)
2. Open <https://esp32gw.local> and accept the self-signed certificate warning
3. Log in with **`admin` / `admin`** (default web admin credentials)
4. Fill in the settings (organized in the sidebar) and click **Save**

The web interface is split into a sidebar with three sections, plus **Restart**,
**Factory reset** and a light/dark theme toggle at the bottom.

**🔑 Credentials**

| Setting | Description |
| --- | --- |
| **Admin login / password** | Protects the HTTPS web interface (config, OTA, restart) — default `admin` / `admin` |
| **AES Key** | Exactly **32 hexadecimal characters** — shared key for WebSocket client authentication; copy this to the HA addon |
| **BLE login / password** | Secret required by BLE/WebSocket clients (noble), independent of the web admin login — default `admin` / `admin` |

**🌐 Network**

| Setting | Description |
| --- | --- |
| **Gateway name** | mDNS hostname: `<name>.local` |
| **WiFi SSID / password** | Network to connect to after reboot |
| **Static IP** _(optional)_ | IP, mask, gateway, DNS — **all-or-nothing**: set IP + mask + gateway together (DNS optional), or leave all empty for DHCP. Invalid/partial entries are rejected; a malformed config can no longer lock you out — the device falls back to AP mode if it cannot obtain a usable IP. |

**🔄 OTA** — upload a compiled `.bin` to update the firmware over the air (see below).

After saving, the ESP reboots, connects to your WiFi, and prints its **IP address** on the serial port.
It will be reachable at `<name>.local` via mDNS, or directly by IP.

### 4 — Configure the HA addon

```yaml
gateway: noble
gateway_host: IP_ADDRESS_OF_YOUR_ESP
gateway_port: 8080
gateway_key: AES_KEY_FROM_ESP_CONFIG
gateway_user: YOUR_BLE_LOGIN
gateway_pass: YOUR_BLE_PASSWORD
```

> ⚠️ `gateway_user` / `gateway_pass` are the **BLE login / password** from the
> *Credentials* tab (default `admin` / `admin`) — **not** the web admin login.
> The WebSocket auth secret is now independent of the web interface credentials.

Add `gateway_debug: true` for verbose logging in Home Assistant.

---

## WebUI development (optional)

The pre-built WebUI (`data/index.html.gz`) is included — no Node.js required to flash the device.

To modify the interface:

```bash
cd webui
npm install
npm run build     # generates data/index.html.gz (~33 kB)
```

The build outputs **only** the gzipped file — the uncompressed `data/index.html`
is removed automatically so it never ends up on the SPIFFS partition.

Then re-flash the filesystem (step 1 above).

Built with **Vue 3 + Vite**,
[`vite-plugin-singlefile`](https://github.com/richardtallent/vite-plugin-singlefile) (inline assets)
and [`vite-plugin-compression2`](https://github.com/nonzzz/vite-plugin-compression) (gzip for SPIFFS).

---

## Firmware OTA update

Once the gateway is on your network, the firmware can be updated without a serial cable:

1. Build the firmware (`env:esp-wrover > General > Build`) and locate
   `.pio/build/esp-wrover/firmware.bin`
2. Open the web interface → **OTA** tab
3. Select the `.bin`, click **Flash**, and confirm
4. Watch the progress bar; the device reboots automatically into the new firmware

The partition table (`partitions_custom.csv`) reserves two app slots (`app0` / `app1`)
plus `otadata`, so the active firmware stays untouched until the upload is verified.

> ⚠️ Application-level rollback is **not** enabled. If a flashed image fails to boot,
> a serial reflash is required — keep USB access as a safety net.

---

## Roadmap

- [x] Timeout for non-authenticated WebSocket connections
- [x] OTA firmware update via web interface
- [ ] Multiple simultaneous BLE device connections _(hardened in code — pending multi-device validation)_
- [ ] Investigate intermittent WiFi instability _(static-IP-safe reconnect + reason-code logging added; WiFi/BLE coexistence requires modem-sleep kept on — tune via the BLE scan duty cycle, `ESP_GW_SCAN_*`)_
- [ ] Memory fragmentation optimization _(`meminfo` heap/fragmentation logging + `addressTypes` cache bounded — broader refactor still open)_
- [ ] Service UUID filtering for scan
