#include "gw_settings.h"
#include <cstring>

bool GwSettings::ready = false;
Preferences GwSettings::prefs;
char *GwSettings::name = nullptr;
size_t GwSettings::nameLen = 0;
char *GwSettings::password = nullptr;
size_t GwSettings::passwordLen = 0;
char *GwSettings::login = nullptr;
size_t GwSettings::loginLen = 0;
char *GwSettings::ssid = nullptr;
size_t GwSettings::ssidLen = 0;
char *GwSettings::pass = nullptr;
size_t GwSettings::passLen = 0;
char *GwSettings::aes = nullptr;
char *GwSettings::bleToken = nullptr;
size_t GwSettings::bleTokenLen = 0;
char *GwSettings::staticIp = nullptr;
char *GwSettings::staticMask = nullptr;
char *GwSettings::staticGw = nullptr;
char *GwSettings::staticDns = nullptr;
char *GwSettings::certName = nullptr;
size_t GwSettings::certNameLen = 0;
uint8_t *GwSettings::cert = nullptr;
size_t GwSettings::certLen = 0;
uint8_t *GwSettings::pk = nullptr;
size_t GwSettings::pkLen = 0;

bool GwSettings::init()
{
  prefs.begin("ESP32GW");

  if (!prefs.isKey("name"))
  { // default name (heap-allocated so setters can delete[] it safely)
    const char *def = "esp32gw";
    nameLen = strlen(def) + 1;
    name = new char[nameLen];
    memcpy(name, def, nameLen);
    prefs.putBytes("name", name, nameLen - 1);
  }
  else
  {
    nameLen = prefs.getBytesLength("name") + 1;
    name = new char[nameLen];
    prefs.getBytes("name", name, nameLen - 1);
    name[nameLen - 1] = '\0';
  }

  if (!prefs.isKey("password"))
  { // default admin password (heap-allocated, see name above)
    const char *def = "admin";
    passwordLen = strlen(def) + 1;
    password = new char[passwordLen];
    memcpy(password, def, passwordLen);
    prefs.putBytes("password", password, passwordLen - 1);
  }
  else
  {
    passwordLen = prefs.getBytesLength("password") + 1;
    password = new char[passwordLen];
    prefs.getBytes("password", password, passwordLen - 1);
    password[passwordLen - 1] = '\0';
  }

  if (!prefs.isKey("login"))
  { // default login (heap-allocated, see name above)
    const char *def = "admin";
    loginLen = strlen(def) + 1;
    login = new char[loginLen];
    memcpy(login, def, loginLen);
    prefs.putBytes("login", login, loginLen - 1);
  }
  else
  {
    loginLen = prefs.getBytesLength("login") + 1;
    login = new char[loginLen];
    prefs.getBytes("login", login, loginLen - 1);
    login[loginLen - 1] = '\0';
  }

  if (prefs.isKey("ssid"))
  {
    ssidLen = prefs.getBytesLength("ssid") + 1;
    ssid = new char[ssidLen];
    prefs.getBytes("ssid", ssid, ssidLen - 1);
    ssid[ssidLen - 1] = '\0';
  }

  if (prefs.isKey("pass"))
  {
    passLen = prefs.getBytesLength("pass") + 1;
    pass = new char[passLen];
    prefs.getBytes("pass", pass, passLen - 1);
    pass[passLen - 1] = '\0';
  }

  aes = new char[BLOCK_SIZE * 2 + 1];
  if (!prefs.isKey("aes"))
  { // new AES key
    Security::generateKey(aes);
    prefs.putBytes("aes", (uint8_t *)aes, BLOCK_SIZE * 2);
  }
  else
  {
    prefs.getBytes("aes", aes, BLOCK_SIZE * 2);
    aes[BLOCK_SIZE * 2] = '\0';
  }

  // BLE WebSocket auth token (default "admin:admin" for backward compat)
  if (!prefs.isKey("ble_token"))
  {
    const char *def = "admin:admin";
    bleTokenLen = strlen(def) + 1;
    bleToken = new char[bleTokenLen];
    memcpy(bleToken, def, bleTokenLen);
    prefs.putBytes("ble_token", bleToken, bleTokenLen - 1);
  }
  else
  {
    bleTokenLen = prefs.getBytesLength("ble_token") + 1;
    bleToken = new char[bleTokenLen];
    prefs.getBytes("ble_token", bleToken, bleTokenLen - 1);
    bleToken[bleTokenLen - 1] = '\0';
  }

  // Static IP settings (optional)
  auto loadStr = [&](const char *key, char *&ptr)
  {
    if (prefs.isKey(key))
    {
      size_t len = prefs.getBytesLength(key) + 1;
      ptr = new char[len];
      prefs.getBytes(key, ptr, len - 1);
      ptr[len - 1] = '\0';
    }
  };
  loadStr("static_ip", staticIp);
  loadStr("static_mask", staticMask);
  loadStr("static_gw", staticGw);
  loadStr("static_dns", staticDns);

  if (prefs.isKey("cert_name"))
  {
    certNameLen = prefs.getBytesLength("cert_name") + 1;
    certName = new char[certNameLen];
    prefs.getBytes("cert_name", certName, certNameLen - 1);
    certName[certNameLen - 1] = '\0';
  }

  if (prefs.isKey("cert"))
  {
    certLen = prefs.getBytesLength("cert");
    cert = new uint8_t[certLen];
    prefs.getBytes("cert", cert, certLen);
  }

  if (prefs.isKey("pk"))
  {
    pkLen = prefs.getBytesLength("pk");
    pk = new uint8_t[pkLen];
    prefs.getBytes("pk", pk, pkLen);
  }

  ready = true;
  return true;
}

bool GwSettings::isConfigured()
{
  return (ssidLen > 0 && passLen > 0);
}

void GwSettings::clear()
{
  prefs.clear();
}

char *GwSettings::getName()
{
  return name;
}

size_t GwSettings::getNameLen()
{
  return nameLen;
}

void GwSettings::setName(const char *val, size_t len)
{
  prefs.putBytes("name", val, len - 1);
  delete[] name;
  nameLen = len;
  name = new char[nameLen];
  memcpy(name, val, nameLen);
}

char *GwSettings::getPassword()
{
  return password;
}

size_t GwSettings::getPasswordLen()
{
  return passwordLen;
}

void GwSettings::setPassword(const char *val, size_t len)
{
  prefs.putBytes("password", val, len - 1);
  delete[] password;
  passwordLen = len;
  password = new char[passwordLen];
  memcpy(password, val, passwordLen);
}

char *GwSettings::getSsid()
{
  return ssid;
}

size_t GwSettings::getSsidLen()
{
  return ssidLen;
}

void GwSettings::setSsid(const char *val, size_t len)
{
  prefs.putBytes("ssid", val, len - 1);
  delete[] ssid;
  ssidLen = len;
  ssid = new char[ssidLen];
  memcpy(ssid, val, ssidLen);
}

char *GwSettings::getPass()
{
  return pass;
}

size_t GwSettings::getPassLen()
{
  return passLen;
}

void GwSettings::setPass(const char *val, size_t len)
{
  prefs.putBytes("pass", val, len - 1);
  delete[] pass;
  passLen = len;
  pass = new char[passLen];
  memcpy(pass, val, passLen);
}

char *GwSettings::getAes()
{
  return aes;
}

// val must be a BLOCK_SIZE*2 hex string (validated by the caller).
// `aes` is always allocated as new char[BLOCK_SIZE*2+1] in init().
void GwSettings::setAes(const char *val, size_t len)
{
  (void)len;
  prefs.putBytes("aes", (uint8_t *)val, BLOCK_SIZE * 2);
  memcpy(aes, val, BLOCK_SIZE * 2);
  aes[BLOCK_SIZE * 2] = '\0';
}

char *GwSettings::getBleToken() { return bleToken; }
size_t GwSettings::getBleTokenLen() { return bleTokenLen; }
void GwSettings::setBleToken(const char *val, size_t len)
{
  prefs.putBytes("ble_token", val, len - 1);
  delete[] bleToken;
  bleTokenLen = len;
  bleToken = new char[bleTokenLen];
  memcpy(bleToken, val, bleTokenLen);
}

char *GwSettings::getLogin() { return login; }
size_t GwSettings::getLoginLen() { return loginLen; }
void GwSettings::setLogin(const char *val, size_t len)
{
  prefs.putBytes("login", val, len - 1);
  delete[] login;
  loginLen = len;
  login = new char[loginLen];
  memcpy(login, val, loginLen);
}

static void _saveStr(Preferences &prefs, const char *key, char *&ptr, const char *val, size_t len)
{
  prefs.putBytes(key, val, len - 1);
  delete[] ptr;
  ptr = new char[len];
  memcpy(ptr, val, len);
}

char *GwSettings::getStaticIp() { return staticIp; }
char *GwSettings::getStaticMask() { return staticMask; }
char *GwSettings::getStaticGw() { return staticGw; }
char *GwSettings::getStaticDns() { return staticDns; }
bool GwSettings::hasStaticIp() { return staticIp != nullptr && strlen(staticIp) > 0; }

void GwSettings::setStaticIp(const char *v, size_t l) { _saveStr(prefs, "static_ip", staticIp, v, l); }
void GwSettings::setStaticMask(const char *v, size_t l) { _saveStr(prefs, "static_mask", staticMask, v, l); }
void GwSettings::setStaticGw(const char *v, size_t l) { _saveStr(prefs, "static_gw", staticGw, v, l); }
void GwSettings::setStaticDns(const char *v, size_t l) { _saveStr(prefs, "static_dns", staticDns, v, l); }

bool GwSettings::hasCert()
{
  return (certNameLen > 0 && certLen > 0 && pkLen > 0);
}

char *GwSettings::getCertName()
{
  return certName;
}

size_t GwSettings::getCertNameLen()
{
  return certNameLen;
}

void GwSettings::setCertName(const char *val, size_t len)
{
  prefs.putBytes("cert_name", val, len - 1);
  delete[] certName;
  certNameLen = len;
  certName = new char[certNameLen];
  memcpy(certName, val, certNameLen);
}

uint8_t *GwSettings::getCert()
{
  return cert;
}

size_t GwSettings::getCertLen()
{
  return certLen;
}

void GwSettings::setCert(const uint8_t *val, size_t len)
{
  prefs.putBytes("cert", val, len);
  delete[] cert;
  certLen = len;
  cert = new uint8_t[certLen];
  memcpy(cert, val, certLen);
}

uint8_t *GwSettings::getPk()
{
  return pk;
}

size_t GwSettings::getPkLen()
{
  return pkLen;
}

void GwSettings::setPk(const uint8_t *val, size_t len)
{
  prefs.putBytes("pk", val, len);
  delete[] pk;
  pkLen = len;
  pk = new uint8_t[pkLen];
  memcpy(pk, val, pkLen);
}