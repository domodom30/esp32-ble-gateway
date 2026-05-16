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

DNSServer *dnsServer = nullptr;
bool connected = false;

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

    if (GwSettings::hasStaticIp())
    {
      IPAddress ip, mask, gw, dns;
      ip.fromString(GwSettings::getStaticIp());
      mask.fromString(GwSettings::getStaticMask());
      gw.fromString(GwSettings::getStaticGw());
      dns.fromString(GwSettings::getStaticDns());
      WiFi.config(ip, gw, mask, dns);
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

    if (wifiResult == WL_CONNECTED)
    {
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      WiFi.setAutoReconnect(true);
      connected = true;
      return true;
    }

    Serial.printf("Failed to connect to configured WiFi [%s]\n", GwSettings::getSsid());
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

// Non-blocking WiFi watchdog: in STA mode, periodically check the link and
// trigger a reconnect with back-off if it dropped. Never blocks the loop.
void maintainWifi()
{
  static uint32_t lastCheck = 0;
  static uint32_t lastReconnect = 0;
  uint32_t now = millis();
  if (now - lastCheck < 10000)
  {
    return;
  }
  lastCheck = now;
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }
  if (lastReconnect != 0 && now - lastReconnect < 15000)
  {
    return; // back-off between attempts
  }
  lastReconnect = now;
  Serial.println("WiFi disconnected, attempting reconnect");
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
}