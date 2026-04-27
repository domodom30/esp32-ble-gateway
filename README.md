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
| 📦 **Custom partitions** | `partitions_custom.csv` replaces `min_spiffs.csv` |
| 🔒 **RSA-1024 certificate** | Avoids stack overflow during TLS cert generation on first boot |

---

## Features

- ✅ WiFi configuration via HTTPS web interface (AP mode on first boot)
- ✅ WebSocket communication with AES-128-CBC authentication
- ✅ BLE scan start / stop
- ✅ Device discovery
- ✅ Read / Write GATT characteristics
- ✅ Subscribe to characteristic notifications

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

This uploads the web interface (~29 kB gzipped) to the ESP32's SPIFFS partition.

### 2 — Flash the firmware

```text
env:esp-wrover > General > Upload and Monitor
```

> On **first boot**, the ESP32 generates a self-signed RSA-1024 TLS certificate.
> This can take up to ~30 seconds — be patient.

### 3 — First configuration

1. Connect to the WiFi AP **`ESP32GW`** (password: `87654321`)
2. Open <https://esp32gw.local> and accept the self-signed certificate warning
3. Log in with **`admin` / `admin`** (default credentials)
4. Configure the settings below and click **Save**

| Setting | Description |
| --- | --- |
| **Gateway name** | mDNS hostname: `<name>.local` |
| **Admin login / password** | Protects the web interface and WebSocket connections |
| **WiFi SSID / password** | Network to connect to after reboot |
| **AES Key** | Shared key for WebSocket client authentication — copy this to the HA addon |
| **Static IP** _(optional)_ | IP, mask, gateway, DNS — leave empty for DHCP |

After saving, the ESP reboots, connects to your WiFi, and prints its **IP address** on the serial port.
It will be reachable at `<name>.local` via mDNS, or directly by IP.

### 4 — Configure the HA addon

```yaml
gateway: noble
gateway_host: IP_ADDRESS_OF_YOUR_ESP
gateway_port: 8080
gateway_key: AES_KEY_FROM_ESP_CONFIG
gateway_user: YOUR_ADMIN_LOGIN
gateway_pass: YOUR_ADMIN_PASSWORD
```

Add `gateway_debug: true` for verbose logging in Home Assistant.

---

## WebUI development (optional)

The pre-built WebUI (`data/index.html.gz`) is included — no Node.js required to flash the device.

To modify the interface:

```bash
cd webui
npm install
npm run build     # generates data/index.html.gz (~29 kB)
```

Then re-flash the filesystem (step 1 above).

Built with **Vue 3 + Vite**,
[`vite-plugin-singlefile`](https://github.com/richardtallent/vite-plugin-singlefile) (inline assets)
and [`vite-plugin-compression2`](https://github.com/nonzzz/vite-plugin-compression) (gzip for SPIFFS).

---

## Roadmap

- [ ] Multiple simultaneous BLE device connections
- [ ] Service UUID filtering for scan
- [ ] Timeout for non-authenticated WebSocket connections
- [ ] Investigate intermittent WiFi instability
- [ ] Memory fragmentation optimization
- [ ] OTA firmware update via web interface
