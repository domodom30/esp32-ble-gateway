#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "gw_settings.h"
#include "web.h"
#include "noble_api.h"
#include "util.h"

#define WIFI_CONNECT_RETRY 5
#define WIFI_CONFIGURE_DNS_PORT 53

// Pile de la tâche Arduino loop = 16 Ko. NE PAS utiliser
// -DCONFIG_ARDUINO_LOOP_STACK_SIZE : le sdkconfig.h du framework le redéfinit
// à 8192 après le define ligne de commande, donc le flag est silencieusement
// ignoré. SET_LOOP_TASK_STACK_SIZE (Arduino.h) définit un
// getArduinoLoopTaskStackSize() fort qui écrase le faible — immunisé contre la
// collision. Marge nécessaire : sérialisation JSON en VLA pile (noble_api.cpp)
// + callbacks NimBLE / HTTPS.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

DNSServer *dnsServer = nullptr;
bool connected = false;

// Diagnostic: log the WiFi disconnect reason code (beacon timeout / auth /
// 4-way handshake / coexistence) to pinpoint the root cause of instability.
void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
  {
    Serial.printf("WiFi STA disconnected, reason=%d\n",
                  info.wifi_sta_disconnected.reason);
  }
  else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
  {
    Serial.print("WiFi STA got IP: ");
    Serial.println(WiFi.localIP());
  }
}

// Apply the configured static IP. Returns true only if a valid static config
// was applied; on any malformed field it logs and returns false (caller stays
// on DHCP) instead of pushing 0.0.0.0 into the IP stack — which would
// associate but be unreachable, locking the device out. Reused by setup and
// the runtime watchdog so a reconnect keeps the same address.
static bool applyStaticIp()
{
  if (!GwSettings::hasStaticIp())
  {
    return false;
  }
  IPAddress ip, mask, gw, dns;
  if (!ip.fromString(GwSettings::getStaticIp()) ||
      !mask.fromString(GwSettings::getStaticMask()) ||
      !gw.fromString(GwSettings::getStaticGw()))
  {
    Serial.println("WARNING: malformed static IP config, falling back to DHCP");
    return false;
  }
  // DNS is optional: fall back to the gateway as resolver if absent/invalid.
  if (!dns.fromString(GwSettings::getStaticDns()))
  {
    dns = gw;
  }
  WiFi.config(ip, gw, mask, dns);
  return true;
}

bool setupWifi()
{
  // wifi logic:
  // - are credentials set then try to connect
  //    - after few faild connect attemtps, revert to config mode
  //    - retry connection after a while if no new configuration provided (or restart ?)
  // - credentials not set, enter config mode

  // we have stored wifi credentials, use them
  if (GwSettings::isConfigured())
  {
    Serial.printf("Connecting to configured WiFi [%s]\n", GwSettings::getSsid());
    WiFi.mode(WIFI_STA);

    if (applyStaticIp())
    {
      Serial.printf("Static IP configured: %s\n", GwSettings::getStaticIp());
    }

    uint8_t wifiResult;
    int8_t wifiRetry = WIFI_CONNECT_RETRY;
    do
    {
      WiFi.begin(GwSettings::getSsid(), GwSettings::getPass());
      wifiResult = WiFi.waitForConnectResult();
      if (wifiResult != WL_CONNECTED && wifiRetry != WIFI_CONNECT_RETRY && wifiRetry > 1)
      {
        Serial.println("Retrying to connect to WiFi");
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      wifiRetry--;
    } while (wifiResult != WL_CONNECTED && wifiRetry > 0);

    // WL_CONNECTED only means *association* succeeded. Require a usable IP
    // (DHCP lease obtained, or valid static) before declaring success —
    // otherwise an associated-but-unreachable device would skip the AP
    // fallback and lock the user out permanently.
    if (wifiResult == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0)
    {
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      WiFi.setAutoReconnect(true);
      // NOTE: do NOT call WiFi.setSleep(false) here. WIFI_PS_NONE is
      // incompatible with the ESP32 WiFi/BLE software coexistence and makes
      // esp_bt_controller_enable() abort in coex_core_enable() when NimBLE
      // starts (boot loop). Keep the Arduino default WIFI_PS_MIN_MODEM.
      // BLE-scan-induced beacon misses are tuned via ESP_GW_SCAN_*, not here.
      connected = true;
      return true;
    }

    if (wifiResult == WL_CONNECTED)
    {
      Serial.println("WiFi associated but no usable IP (no DHCP lease / bad static IP) — falling back to AP");
    }
    else
    {
      Serial.printf("Failed to connect to configured WiFi [%s]\n", GwSettings::getSsid());
    }
  }

  // no credentials or connect failed, setup AP
  Serial.println("Starting AP for configuration");
  WiFi.softAP("ESP32GW", "87654321");
  std::string dnsName = "";
  dnsName += GwSettings::getName();
  dnsName += ".local";
  dnsServer = new DNSServer();
  dnsServer->start(WIFI_CONFIGURE_DNS_PORT, dnsName.c_str(), WiFi.softAPIP());

  return true;
}

bool setupWeb()
{
  return WebManager::init();
}

void setup()
{
  Serial.begin(921600);
  delay(200);
  Serial.println();
  // esp_log_level_set("*", ESP_LOG_VERBOSE);
  GwSettings::init();

  WiFi.onEvent(onWifiEvent);
  setupWifi();

  setupWeb();

  bool mdnsSuccess = false;
  uint8_t mdnsRetry = 5;
  do
  {
    mdnsSuccess = MDNS.begin(GwSettings::getName());
    if (!mdnsSuccess)
    {
      Serial.println("mDNS start failed, retrying...");
      delay(500);
    }
  } while (!mdnsSuccess && --mdnsRetry > 0);

  if (mdnsSuccess)
  {
    MDNS.addService("http", "tcp", ESP_GW_WEBSERVER_PORT);
  }
  else
  {
    Serial.println("WARNING: mDNS unavailable; reach the gateway by IP address");
  }

  NobleApi::init();

  if (mdnsSuccess)
  {
    MDNS.addService("ws", "tcp", ESP_GW_WEBSOCKET_PORT);
  }

  Serial.println("Setup complete");
  meminfo();
}

// Non-blocking WiFi watchdog. setAutoReconnect(true) already recovers brief
// drops on its own; this only forces a hard reconnect after a sustained
// outage (~30 s) so it never tears down a recovery already in progress, and
// re-applies the static IP so the gateway keeps the same address.
void maintainWifi()
{
  static uint32_t lastCheck = 0;
  static uint8_t failedChecks = 0;
  uint32_t now = millis();
  if (now - lastCheck < 10000)
  {
    return;
  }
  lastCheck = now;

  if (WiFi.status() == WL_CONNECTED)
  {
    failedChecks = 0;
    return;
  }

  // Let the built-in auto-reconnect try for ~30 s before intervening.
  if (++failedChecks < 3)
  {
    return;
  }
  failedChecks = 0;
  Serial.println("WiFi down >30s, forcing reconnect");
  applyStaticIp();
  WiFi.disconnect();
  WiFi.begin(GwSettings::getSsid(), GwSettings::getPass());
}

void loop()
{
  if (dnsServer != nullptr)
  {
    // when in configuration mode handle DNS requests
    dnsServer->processNextRequest();
  }
  else
  {
    maintainWifi();
  }
  NobleApi::loop();
  WebManager::loop();

  // periodic heap/fragmentation sample for diagnostics
  static uint32_t lastMemLog = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastMemLog >= 30000)
  {
    lastMemLog = nowMs;
    meminfo();
  }
}