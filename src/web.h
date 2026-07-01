#ifndef ESP_GW_WEB_H
#define ESP_GW_WEB_H

#ifndef ESP_GW_WEBSERVER_PORT
#define ESP_GW_WEBSERVER_PORT 80
#endif

#ifndef ESP_GW_WEBSERVER_SECURE_PORT
#define ESP_GW_WEBSERVER_SECURE_PORT 443
#endif

#ifndef ESP_GW_WEBSERVER_BUFFER_SIZE
#define ESP_GW_WEBSERVER_BUFFER_SIZE 512
#endif

#include "gw_settings.h"
#include "util.h"
#include "security.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

using namespace httpsserver;

class WebManager {
  public:
    static bool init();
    static void loop();
  private:
    static HTTPServer *server;
    static uint8_t *certData;
    static uint8_t *pkData;
    static SSLCert * cert;
    static HTTPSServer *serverSecure;
    static bool rebootRequired;
    static uint32_t rebootAt;
    static uint8_t *buffer;

    static bool initCertificate();
    static void clearCertificate();
    // Quiesce the radios (BLE + WiFi) before the software reset. A bare
    // ESP.restart() with WiFi/BT still running can leave the coexistence
    // hardware in a state the bootloader can't recover from without a power
    // cycle ("obligé de débrancher l'ESP"); this makes the auto-reboot reliable.
    static void restartClean();
    static void middlewareAuthentication(HTTPRequest * req, HTTPResponse * res, std::function<void()> next);
    static void handleHome(HTTPRequest * req, HTTPResponse * res);
    static void handleConfigGet(HTTPRequest * req, HTTPResponse * res);
    static void handleConfigSet(HTTPRequest * req, HTTPResponse * res);
    static void handleFactoryReset(HTTPRequest * req, HTTPResponse * res);
    static void handleRestart(HTTPRequest * req, HTTPResponse * res);
    static void handleOtaPrepare(HTTPRequest * req, HTTPResponse * res);
    static void handleOtaUpdate(HTTPRequest * req, HTTPResponse * res);
    static void handleRedirect(HTTPRequest * req, HTTPResponse * res);
    static void handleNotFound(HTTPRequest *req, HTTPResponse *res);
};

#endif