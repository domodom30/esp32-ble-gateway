#include "web.h"
#include <Update.h>
#include "noble_api.h"
#include "ble_api.h"

HTTPServer *WebManager::server = nullptr;
uint8_t *WebManager::certData = nullptr;
uint8_t *WebManager::pkData = nullptr;
SSLCert *WebManager::cert = nullptr;
HTTPSServer *WebManager::serverSecure = nullptr;
bool WebManager::rebootRequired = false;
bool WebManager::rebootNextLoop = false;
uint8_t *WebManager::buffer = new uint8_t[ESP_GW_WEBSERVER_BUFFER_SIZE];

// true if `s` is exactly `expectedLen` hexadecimal characters
static bool isHexString(const char *s, size_t expectedLen)
{
  if (strlen(s) != expectedLen)
  {
    return false;
  }
  for (size_t i = 0; i < expectedLen; i++)
  {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
    {
      return false;
    }
  }
  return true;
}

// true if `s` is a syntactically valid dotted-quad IPv4 address
static bool isValidIPv4(const char *s)
{
  if (s == nullptr)
  {
    return false;
  }
  for (int part = 0; part < 4; part++)
  {
    if (*s < '0' || *s > '9')
    {
      return false; // each octet must start with a digit
    }
    int value = 0;
    int digits = 0;
    while (*s >= '0' && *s <= '9')
    {
      value = value * 10 + (*s - '0');
      if (++digits > 3 || value > 255)
      {
        return false;
      }
      s++;
    }
    if (part < 3)
    {
      if (*s != '.')
      {
        return false;
      }
      s++;
    }
  }
  return *s == '\0';
}

bool WebManager::init()
{
  if (!initCertificate())
  {
    Serial.println("Could not init HTTPS certificate");
    return false;
  }

  serverSecure = new HTTPSServer(cert, ESP_GW_WEBSERVER_SECURE_PORT, 1);
  serverSecure->addMiddleware(middlewareAuthentication);
  serverSecure->registerNode(new ResourceNode("/", "GET", handleHome));
  serverSecure->registerNode(new ResourceNode("/config", "GET", handleConfigGet));
  serverSecure->registerNode(new ResourceNode("/config", "POST", handleConfigSet));
  serverSecure->registerNode(new ResourceNode("/factoryReset", "GET", handleFactoryReset));
  serverSecure->registerNode(new ResourceNode("/restart", "GET", handleRestart));
  serverSecure->registerNode(new ResourceNode("/update", "POST", handleOtaUpdate));
  serverSecure->registerNode(new ResourceNode("/radar", "GET", handleRadarGet));
  serverSecure->setDefaultNode(new ResourceNode("", "", handleNotFound));
  serverSecure->start();

  Serial.println("HTTPS started");
  meminfo();

  server = new HTTPServer(ESP_GW_WEBSERVER_PORT, 1);
  server->setDefaultNode(new ResourceNode("", "", handleRedirect));
  server->start();
  Serial.println("HTTP started");
  meminfo();

  return true;
}

void WebManager::loop()
{
  if (rebootRequired)
  {
    // delay the reboot one more loop
    if (rebootNextLoop)
    {
      ESP.restart();
    }
    rebootNextLoop = true;
  }
  server->loop();
  serverSecure->loop();
}

bool WebManager::initCertificate()
{
  if (GwSettings::hasCert())
  {
    Serial.println("Loading stored HTTPS certificate");

    // if name does not match the certificate name, don't load but regenerate instead
    if (strcmp(GwSettings::getCertName(), GwSettings::getName()) == 0)
    {
      Serial.printf("Loaded cert from nvs [%s.local][cert=%d][pk=%d]\n",
                    GwSettings::getCertName(),
                    GwSettings::getCertLen(),
                    GwSettings::getPkLen());

      cert = new SSLCert(GwSettings::getCert(), GwSettings::getCertLen(), GwSettings::getPk(), GwSettings::getPkLen());
    }
  }

  if (cert == nullptr)
  {
    // requires larger stack #define CONFIG_ARDUINO_LOOP_STACK_SIZE 10240
    // should run this in a separate task as sdk can't be configured via platformio.ini
    Serial.println("Generating new HTTPS certificate");

    cert = new SSLCert();
    std::string dn = "CN=";
    dn += GwSettings::getName();
    dn += ".local,O=FancyCompany,C=RO";
    int createCertResult = createSelfSignedCert(
        *cert,
        KEYSIZE_1024,
        dn,
        "20190101000000",
        "20300101000000");

    if (createCertResult != 0)
    {
      Serial.printf("Cerating certificate failed. Error Code = 0x%02X, check SSLCert.hpp for details", createCertResult);
      return false;
    }
    Serial.printf("Creating the certificate was successful [%s.local][cert=%d][pk=%d]\n", GwSettings::getName(), cert->getCertLength(), cert->getPKLength());
    GwSettings::setCertName(GwSettings::getName(), GwSettings::getNameLen());
    GwSettings::setCert(cert->getCertData(), cert->getCertLength());
    GwSettings::setPk(cert->getPKData(), cert->getPKLength());
  }

  return true;
}

void WebManager::middlewareAuthentication(HTTPRequest *req, HTTPResponse *res, std::function<void()> next)
{
  Serial.println("Auth middleware started");
  std::string reqLogin = req->getBasicAuthUser();
  std::string reqPassword = req->getBasicAuthPassword();

  if (reqPassword.length() == 0 || strcmp(GwSettings::getLogin(), reqLogin.c_str()) != 0 || strcmp(GwSettings::getPassword(), reqPassword.c_str()) != 0)
  {
    res->setStatusCode(401);
    res->setStatusText("Unauthorized");
    res->setHeader("Content-Type", "text/plain");
    res->setHeader("WWW-Authenticate", "Basic realm=\"Gateway admin area\"");
    res->println("401. Unauthorized");
    Serial.println("Auth failed");
  }
  else
  {
    Serial.println("Auth success");
    next();
  }
}

void WebManager::handleHome(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Content-Encoding", "gzip");

  SPIFFS.begin();
  File file = SPIFFS.open("/index.html.gz", "r");
  size_t length = 0;
  do
  {
    length = file.read(buffer, ESP_GW_WEBSERVER_BUFFER_SIZE);
    res->write(buffer, length);
  } while (length > 0);
  file.close();
  SPIFFS.end();

  meminfo();
}

void WebManager::handleConfigGet(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "application/json");
  res->setHeader("Connection", "close");

  JsonDocument config;
  config["name"] = GwSettings::getName();
  config["login"] = GwSettings::getLogin();

  if (GwSettings::getSsidLen() > 0)
  {
    config["wifi_ssid"] = GwSettings::getSsid();
  }

  if (GwSettings::getPassLen() > 0)
  {
    config["wifi_pass"] = GwSettings::getPass();
  }

  config["aes_key"] = GwSettings::getAes();
  config["ble_token"] = GwSettings::getBleToken();

  // Static IP (empty strings if not configured)
  config["static_ip"] = GwSettings::getStaticIp() ? GwSettings::getStaticIp() : "";
  config["static_mask"] = GwSettings::getStaticMask() ? GwSettings::getStaticMask() : "";
  config["static_gw"] = GwSettings::getStaticGw() ? GwSettings::getStaticGw() : "";
  config["static_dns"] = GwSettings::getStaticDns() ? GwSettings::getStaticDns() : "";

  size_t configLength = serializeJson(config, buffer, ESP_GW_WEBSERVER_BUFFER_SIZE);
  config.clear();

  res->write((uint8_t *)buffer, configLength);

  meminfo();
}

void WebManager::handleConfigSet(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Connection", "close");

  size_t idx = 0;
  // while "not everything read" or "buffer is full"
  while (!req->requestComplete() && idx < ESP_GW_WEBSERVER_BUFFER_SIZE)
  {
    idx += req->readChars((char *)buffer + idx, ESP_GW_WEBSERVER_BUFFER_SIZE - idx);
  }

  if (!req->requestComplete())
  {
    Serial.println("Request entity too large");
    Serial.println((char *)buffer);
    res->setStatusCode(413);
    res->setStatusText("Request entity too large");
    res->println("413 Request entity too large");
    return;
  }

  buffer[idx + 1] = '\0';

  JsonDocument config;
  DeserializationError error = deserializeJson(config, buffer, idx + 1);

  if (error != DeserializationError::Ok)
  { // Check for errors in parsing
    Serial.println("Invalid JSON format");
    Serial.println((char *)buffer);
    res->setStatusCode(400);
    res->setStatusText("Invalid JSON format");
    res->println("400 Invalid JSON format");
    return;
  }

  // Reject an invalid AES key before applying any change (atomic)
  const char *aesKeyCheck = config["aes_key"];
  if (aesKeyCheck && strlen(aesKeyCheck) > 0 && !isHexString(aesKeyCheck, BLOCK_SIZE * 2))
  {
    Serial.println("Invalid AES key");
    res->setStatusCode(400);
    res->setStatusText("Invalid AES key");
    res->setHeader("Content-Type", "text/plain");
    res->println("400 Invalid AES key (expected 32 hex chars)");
    return;
  }

  // Reject an incomplete/invalid static IP set before applying anything.
  // Partial/invalid static config bricks the device (unreachable, no AP
  // fallback once associated), so enforce all-or-nothing here (atomic).
  {
    const char *sipChk = config["static_ip"];
    const char *smskChk = config["static_mask"];
    const char *sgwChk = config["static_gw"];
    const char *sdnsChk = config["static_dns"];
    bool ipSet = sipChk && strlen(sipChk) > 0;
    bool mskSet = smskChk && strlen(smskChk) > 0;
    bool gwSet = sgwChk && strlen(sgwChk) > 0;
    bool dnsSet = sdnsChk && strlen(sdnsChk) > 0;
    if (ipSet || mskSet || gwSet)
    {
      if (!(ipSet && mskSet && gwSet) ||
          !isValidIPv4(sipChk) || !isValidIPv4(smskChk) || !isValidIPv4(sgwChk) ||
          (dnsSet && !isValidIPv4(sdnsChk)))
      {
        Serial.println("Invalid static IP configuration");
        res->setStatusCode(400);
        res->setStatusText("Invalid static IP");
        res->setHeader("Content-Type", "text/plain");
        res->println("400 Invalid static IP (need valid ip+mask+gw, or leave all empty for DHCP)");
        return;
      }
    }
  }

  Serial.println("Checking for config changes");

  const char *name = config["name"];
  if (name && strlen(name) > 0)
  {
    Serial.printf("Setting new name [%s]\n", name);
    GwSettings::setName(name, strlen(name) + 1);
    rebootRequired = true;
  }

  const char *newPassword = config["password"];
  if (newPassword && strlen(newPassword) > 0)
  {
    Serial.printf("Setting new admin password\n");
    GwSettings::setPassword(newPassword, strlen(newPassword) + 1);
    rebootRequired = true;
  }

  const char *newLogin = config["login"];
  if (newLogin && strlen(newLogin) > 0)
  {
    Serial.printf("Setting new admin login [%s]\n", newLogin);
    GwSettings::setLogin(newLogin, strlen(newLogin) + 1);
    rebootRequired = true;
  }

  const char *ssid = config["wifi_ssid"];
  if (ssid && strlen(ssid) > 0)
  {
    Serial.printf("Setting new WiFi SSID [%s]\n", ssid);
    GwSettings::setSsid(ssid, strlen(ssid) + 1);
    rebootRequired = true;
  }

  const char *pass = config["wifi_pass"];
  if (pass && strlen(pass) > 0)
  {
    Serial.printf("Setting new WiFi Password\n");
    GwSettings::setPass(pass, strlen(pass) + 1);
    rebootRequired = true;
  }

  const char *sip = config["static_ip"];
  const char *smsk = config["static_mask"];
  const char *sgw = config["static_gw"];
  const char *sdns = config["static_dns"];
  // empty string = disable static IP
  if (sip != nullptr)
  {
    GwSettings::setStaticIp(sip, strlen(sip) + 1);
    rebootRequired = true;
  }
  if (smsk != nullptr)
  {
    GwSettings::setStaticMask(smsk, strlen(smsk) + 1);
    rebootRequired = true;
  }
  if (sgw != nullptr)
  {
    GwSettings::setStaticGw(sgw, strlen(sgw) + 1);
    rebootRequired = true;
  }
  if (sdns != nullptr)
  {
    GwSettings::setStaticDns(sdns, strlen(sdns) + 1);
    rebootRequired = true;
  }

  const char *bleToken = config["ble_token"];
  if (bleToken && strlen(bleToken) > 0)
  {
    Serial.println("Setting new BLE token");
    GwSettings::setBleToken(bleToken, strlen(bleToken) + 1);
    rebootRequired = true;
  }

  const char *aesKey = config["aes_key"];
  if (aesKey && strlen(aesKey) > 0)
  {
    Serial.println("Setting new AES key");
    GwSettings::setAes(aesKey, strlen(aesKey) + 1);
    rebootRequired = true;
  }

  res->setHeader("Content-Type", "text/plain");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->print("OK");

  if (rebootRequired)
  {
    Serial.println("Rebooting");
  }
  meminfo();
}

void WebManager::handleFactoryReset(HTTPRequest *req, HTTPResponse *res)
{
  GwSettings::clear();
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Connection", "close");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->print("OK");

  Serial.println("Rebooting");
  rebootRequired = true;
}

void WebManager::handleRestart(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "text/plain");
  res->setHeader("Connection", "close");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->print("OK");

  Serial.println("Restart requested via web");
  rebootRequired = true;
}

// Bluetooth radar: keep a scan alive while the UI polls and stream the
// bounded, currently-nearby device list as JSON (no large buffer).
void WebManager::handleRadarGet(HTTPRequest *req, HTTPResponse *res)
{
  NobleApi::radarKeepAlive();

  res->setHeader("Content-Type", "application/json");
  res->setHeader("Connection", "close");
  res->setStatusCode(200);
  res->setStatusText("OK");

  const BLERadarEntry *list = nullptr;
  uint8_t count = BLEApi::getRadar(list);
  uint32_t now = millis();

  res->print("[");
  bool first = true;
  for (uint8_t i = 0; i < count; i++)
  {
    uint32_t age = now - list[i].lastSeen;
    if (age >= ESP_GW_RADAR_TTL_MS)
    {
      continue; // stale: not currently nearby
    }
    if (!first)
    {
      res->print(",");
    }
    first = false;
    res->print("{\"id\":\"");
    res->print(BLEApi::idToString(list[i].id).c_str());
    res->print("\",\"name\":\"");
    for (const char *p = list[i].name; *p != '\0'; p++)
    {
      if (*p == '"' || *p == '\\')
      {
        res->print('\\');
      }
      res->print(*p);
    }
    res->print("\",\"rssi\":");
    res->print((int)list[i].rssi);
    res->print(",\"age\":");
    res->print((unsigned long)age);
    res->print("}");
  }
  res->print("]");
}

void WebManager::handleOtaUpdate(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "text/plain");
  res->setHeader("Connection", "close");

  size_t contentLength = req->getContentLength();
  size_t freeSpace = ESP.getFreeSketchSpace();

  // refuse empty uploads or images larger than the inactive OTA slot
  if (contentLength == 0 || contentLength > freeSpace)
  {
    Serial.printf("OTA rejected: size=%u free=%u\n", (unsigned)contentLength, (unsigned)freeSpace);
    res->setStatusCode(400);
    res->setStatusText("Bad Request");
    res->print("FAIL invalid firmware size");
    return;
  }

  if (!Update.begin(contentLength))
  {
    Serial.printf("OTA begin failed: %s\n", Update.errorString());
    res->setStatusCode(500);
    res->setStatusText("Internal Server Error");
    res->print("FAIL ");
    res->print(Update.errorString());
    return;
  }

  Serial.printf("OTA started: %u bytes\n", (unsigned)contentLength);

  uint8_t otaBuffer[1024];
  size_t written = 0;
  while (!req->requestComplete())
  {
    size_t n = req->readBytes(otaBuffer, sizeof(otaBuffer));
    if (n == 0)
    {
      break;
    }
    if (Update.write(otaBuffer, n) != n)
    {
      Serial.printf("OTA write failed: %s\n", Update.errorString());
      Update.abort();
      res->setStatusCode(500);
      res->setStatusText("Internal Server Error");
      res->print("FAIL ");
      res->print(Update.errorString());
      return;
    }
    written += n;
  }

  if (written != contentLength || !Update.end(true))
  {
    Serial.printf("OTA end failed (%u/%u): %s\n",
                  (unsigned)written, (unsigned)contentLength, Update.errorString());
    if (!Update.isFinished())
    {
      Update.abort();
    }
    res->setStatusCode(500);
    res->setStatusText("Internal Server Error");
    res->print("FAIL ");
    res->print(Update.errorString());
    return;
  }

  Serial.println("OTA success, rebooting");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->print("OK");
  rebootRequired = true;
}

void WebManager::handleRedirect(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Connection", "close");

  std::string dn;
  dn = "https://";
  dn += GwSettings::getName();
  dn += ".local";
  res->setHeader("Location", dn);
  res->setStatusCode(301);
  res->setStatusText("Moved Permanently");
}

void WebManager::handleNotFound(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Connection", "close");
  res->setStatusCode(404);
  res->setStatusText("NOT FOUND");
  res->println("NOT FOUND");
}