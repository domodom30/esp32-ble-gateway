# esp32-ble-gateway

WiFi to BLE (Bluetooth Low Energy) gateway on ESP32 using a modified version of [Noble](https://github.com/abandonware/noble)'s [WebSocket protocol](https://github.com/abandonware/noble/blob/master/ws-slave.js). The modifications consist in an added authentication layer upon connection and some extra payloads here and there. It's designed to be used with [ttlock-sdk-js](https://github.com/kind3r/ttlock-sdk-js) at least until I will find the time to document the API and implement a separate Noble binding. Note that data flows is via **standard unencrypted websocket** as the ESP can barely handle the memory requirements for BLE, WiFi and WebSocket at the same time and besides gateway is supposed to be in your LAN and the BLE traffic can be easilly sniffed over the air so there isn't really a point in encrypting all communication at this time.

> Note: this has nothing to do with the TTLock G2 official gateway, it is basically just a GATT proxy over WiFi.

Feeling generous and want to support my work, here is [my PayPal link](https://paypal.me/kind3r).

> **This fork** includes the following changes over the [original repository](https://github.com/kind3r/esp32-ble-gateway):
>
> - Migration to **NimBLE-Arduino 2.x** (was using the standard Arduino BLE library)
> - Migration of the WebUI from **Vue 2 → Vue 3 + Vite** (single-file build, no CDN dependencies)
> - **Configurable admin login and password** via the web interface (no longer hardcoded)
> - **Static IP** configuration via the web interface (optional, DHCP by default)
> - Stack size increase handled via `build_flags` in `platformio.ini` — no manual `sdkconfig.h` edit required
> - Custom partition table (`partitions_custom.csv`) replacing `min_spiffs.csv`
> - TLS certificate generated with RSA-1024 to avoid stack overflow during generation

## What works for now

- WiFi init with AP style configuration via HTTPS web page
- Websocket communication and AES 128 CBC auth
- Start/stop BLE scan
- Discover devices
- Read characteristics
- Write characteristics
- Subscribe to characteristic

## How to install

This short guide explains how to install the gateway and configure the [TTLock Home Assistant addon](https://github.com/kind3r/hass-addons) so that you can interface it with a TTLock lock. You need to compile and upload the binary yourself, there is no pre-compiled version but the process should be fairly easy even for beginers.

### Requirements

1. [Visual Studio Code (VSCode)](https://code.visualstudio.com/) and [PlatformIO](https://platformio.org/) extension.
2. A clone of this repository
3. A working **ESP32-WROVER board**
4. Some type of TTLock lock paired to a working Home Assistant installation with [TTLock Home Assistant addon](https://github.com/kind3r/hass-addons)

### Preparing the ESP32

Open the cloned repo in VSCode and PlatformIO should automatically install all the required dependencies (it will take a couple of minutes, depending on your computer and internet speed, be patient and let it *settle*).

> **No manual `sdkconfig.h` edit is required.** The stack size is now set to `16384` bytes directly via `build_flags` in `platformio.ini` (`-DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384`). This is necessary because the HTTPS certificate generation requires more stack space than the default.

> At the moment, the project is only configured to work on **ESP32-WROVER boards**. If you have a different board, you need to edit the `platformio.ini` file and create your own env configuration. The project uses a custom partition table (`partitions_custom.csv`) to balance firmware and SPIFFS storage.

Connect your ESP32 to the PC, go to PlatformIO menu (the alien head on the VSCode's left toolbar, where you have files, search, plugins etc.) then in **Project Tasks** choose **env:esp-wrover** -> **Platform** -> **Upload Filesystem Image**. This will 'format' the storage and upload the web UI.

Next, you need to build and upload the main code. In **Project Tasks** choose **env:esp-wrover** -> **General** -> **Upload and Monitor**. This should start the build process and once it is finished the compiled result will be uploaded to the ESP32.

Once the upload finishes you should start seeing some debug output, including the status of the WiFi AP and HTTPS certificate generation status (it will take quite some time so be patient — RSA-1024 certificate generation can take up to ~30 seconds on first boot). After the startup is completed, you can connect to ESP's AP named **ESP32GW** with password **87654321** and access [https://esp32gw.local](https://esp32gw.local). The browser will complain about the self-signed certificate but you can ignore and continue. The default credentials are **admin / admin**.

In the web interface you can configure:

- **Gateway name** — used as the mDNS hostname (`<name>.local`)
- **Admin login** and **Admin password** — HTTP Basic Auth credentials to protect the web interface and WebSocket connections
- **WiFi SSID** and **WiFi password** — the network to connect to
- **AES Key** — the shared key used to authenticate WebSocket clients (copy this into the HA addon config)
- **Static IP** *(optional)* — IP address, subnet mask, gateway and DNS; leave all fields empty to use DHCP

After saving the new configuration, the ESP will reboot, connect to your WiFi and output its **IP address** on the serial port (it will also generate a new HTTPS certificate if you changed its name). It will also be accessible via `esp32gw.local` (or the new name you gave it) via MDNS if this service is working in your network. You can still make configuration changes by accessing its IP address in the browser.

### Building the WebUI (optional)

The compiled WebUI (`data/index.html.gz`) is already included in the repository. If you want to modify it:

1. Go to the `webui/` directory
2. Run `npm install`
3. Run `npm run build` — this produces `data/index.html.gz` (single-file, gzip compressed, ~29 kB)
4. Re-flash the filesystem: **PlatformIO** -> **env:esp-wrover** -> **Platform** -> **Upload Filesystem Image**

The WebUI is built with **Vue 3 + Vite** and uses `vite-plugin-singlefile` to inline everything into a single HTML file, then `vite-plugin-compression2` to gzip it for SPIFFS.

### Setting up HA

Once you have the ESP running the gateway software, go to the **TTLock Home Assistant addon** configuration options and add the following:

```yaml
gateway: noble
gateway_host: IP_ADDRESS_OF_YOUR_ESP
gateway_port: 8080
gateway_key: AES_KEY_FROM_ESP_CONFIG
gateway_user: YOUR_ADMIN_LOGIN
gateway_pass: YOUR_ADMIN_PASSWORD
```

> The **login and password are configurable** via the web interface (default: `admin` / `admin`). The port is fixed at `8080`. You will need to update the **IP address of the ESP gateway**, the **AES key**, and your **credentials** if you changed them.

For extra debug info, you can add the `gateway_debug: true` option to log all communication to and from the gateway in Home Assistant.

If everything was done correctly you should now be able to use the addon using the ESP32 device as a BLE gateway.

## Todo

- check if multiple connections to multiple devices are possible (`BLEDevice::createClient` seems to store only 1 `BLEClient`, but we could just create the client ourselves)
- Service UUID filtering for scan and allow/disallow duplicates
- Timeout for non-authenticated connections
- Investigate unstable wifi (sometimes it connects but there is no traffic; try to ping gw during setup)
- Optimize memory fragmentation
- OTA firmware update via web interface

### Random thoughts

- device discovery is always sent to all authenticated clients
- give each device a unique ID (peripheralUuid) and store ID, address and address type in a Map as it is required for connection
- connection is done based on peripheralUuid translated to address and type in the noble_api
- always stop scanning before connecting to a device
- only one client can connect to a device at a time so associate websocket with connection and cleanup on disconnect
