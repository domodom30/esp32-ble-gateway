#include "ble_api.h"
// #include <freertos/FreeRTOS.h>

bool BLEApi::_isReady = false;
bool BLEApi::_isScanning = false;
NimBLEScan *BLEApi::bleScan = nullptr;
BLEDeviceFound BLEApi::_cbOnDeviceFound = nullptr;
BLEDeviceEvent BLEApi::_cbOnDeviceConnected = nullptr;
BLEDeviceEvent BLEApi::_cbOnDeviceDisconnected = nullptr;
BLECharacteristicNotification BLEApi::_cbOnCharacteristicNotification = nullptr;
NimBLEScanCallbacks *BLEApi::_advertisedDeviceCallback = nullptr;
NimBLEClientCallbacks *BLEApi::_clientCallback = nullptr;
std::map<BLEPeripheralID, uint8_t> BLEApi::addressTypes;
BLERadarEntry BLEApi::_radar[ESP_GW_RADAR_MAX];
uint8_t BLEApi::_radarCount = 0;
BLEConnection BLEApi::connections[MAX_CLIENT_CONNECTIONS];
uint8_t BLEApi::activeConnections = 0;
bool BLEApi::_scanRequested = false;
bool BLEApi::_scanActiveMode = true;
uint32_t BLEApi::_scanDuration = 0;
volatile bool BLEApi::_bleBusy = false;

class myAdvertisedDeviceCallbacks : public NimBLEScanCallbacks
{
  // onResult is the direct replacement for the old onResult (after full scan response)
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice)
  {
    BLEApi::_onDeviceFoundProxy(advertisedDevice);
  }
  // Called when the scan period ends
  void onScanEnd(const NimBLEScanResults &results, int reason)
  {
    BLEApi::_onScanFinished(results);
  }
};

class myClientCallbacks : public NimBLEClientCallbacks
{
public:
  void onConnect(NimBLEClient *pClient, NimBLEConnInfo &connInfo)
  {
    BLEPeripheralID id = BLEApi::idFromAddress(pClient->getPeerAddress());
    Serial.printf("***** Connected to ==%s==\n", pClient->getPeerAddress().toString().c_str());
    BLEApi::_onDeviceInteractionProxy(id, true);
  }
  void onDisconnect(NimBLEClient *pClient, int reason)
  {
    BLEPeripheralID id = BLEApi::idFromAddress(pClient->getPeerAddress());
    // reason : code NimBLE 2.x (ex. BLE_HS_ETIMEOUT = timeout de supervision /
    // coexistence radio, terminaison distante, erreur contrôleur) — loggé pour
    // diagnostiquer les coupures en plein handshake TTLock.
    Serial.printf("***** Disconnected from ==%s== reason=%d\n", pClient->getPeerAddress().toString().c_str(), reason);
    BLEApi::_onDeviceInteractionProxy(id, false);
  }

private:
  std::string peripheralUuid;
};

/**
 * Initialize BLE API
 */
void BLEApi::init()
{
  if (!_isReady)
  {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    NimBLEDevice::init("ESP32BLEGW");
    bleScan = NimBLEDevice::getScan();
    _advertisedDeviceCallback = new myAdvertisedDeviceCallbacks();
    bleScan->setScanCallbacks(_advertisedDeviceCallback, true);
    // NimBLE 2.x : setInterval/setWindow sont en millisecondes (et non plus en
    // unités 0,625 ms). Scan quasi-continu (duty 90 %) pour une découverte
    // rapide des advertisements « newEvents ». Sûr car connect() stoppe le
    // scan avant toute connexion.
    bleScan->setInterval(ESP_GW_SCAN_INTERVAL); // ms
    bleScan->setWindow(ESP_GW_SCAN_WINDOW);     // ms
    _clientCallback = new myClientCallbacks();
    // TODO: maybe do some pre-descovery to get address types of devices around us
    // in case ESP was rebooted and clients try to connect before doing a scan
    _isReady = true;
  }
}

/**
 * If the API is ready
 */
bool BLEApi::isReady()
{
  return _isReady;
}

/**
 * Start scanning for BLE devices
 * @param duration of the scan in seconds. If 0, it will keep scanning until stopped
 * @param active if we are performing an active (send scan requests) or passive (only listen to advertisements) scan
 */
bool BLEApi::startScan(uint32_t duration, bool active)
{
  if (!_isReady)
  {
    return false;
  }
  // record the desired scan state so it can be restored after a connect()
  _scanRequested = true;
  _scanActiveMode = active;
  _scanDuration = duration;
  if (_isScanning)
  {
    return true; // already scanning as desired
  }
  _isScanning = true;
  Serial.println("BLE Scan started");
  bleScan->setActiveScan(active);
  // duration is in seconds (0 = indefinite); NimBLE 2.x expects milliseconds
  bleScan->start(duration * 1000, true);
  return true;
}

/**
 * Raw scan stop: stops the radio scan but leaves the desired-state flag
 * untouched (used internally around connect()).
 */
bool BLEApi::stopScanInternal()
{
  if (_isReady && _isScanning)
  {
    bleScan->stop(); // this does not call the callback onScanFinished
    bleScan->clearResults();
    _isScanning = false;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    Serial.println("BLE Scan stopped");
    return true;
  }
  return false;
}

/**
 * Stop scanning for BLE devices (caller-initiated: clears the desired state
 * so it is NOT auto-resumed after the next connect()).
 */
bool BLEApi::stopScan()
{
  _scanRequested = false;
  return stopScanInternal();
}

void BLEApi::pauseScanForConnect()
{
  // stop the radio scan but keep _scanRequested so it resumes afterwards
  stopScanInternal();
}

void BLEApi::resumeScanIfRequested()
{
  // Arbitrage radio : ne JAMAIS relancer le scan tant qu'une connexion GATT
  // est active. Sur la radio unique de l'ESP32 (+ coexistence WiFi 2,4 GHz),
  // un scan ~90 % de duty cycle affame les connection events et fait tomber le
  // lien en plein handshake TTLock (No response to checkAdmin). Le scan ne
  // reprend qu'à la dernière déconnexion (activeConnections == 0).
  if (_scanRequested && !_isScanning && _isReady && activeConnections == 0)
  {
    _isScanning = true;
    bleScan->setActiveScan(_scanActiveMode);
    bleScan->start(_scanDuration * 1000, true);
    Serial.println("BLE Scan resumed");
  }
}

/**
 * Set a callback for when a device is found
 */
void BLEApi::onDeviceFound(BLEDeviceFound cb)
{
  _cbOnDeviceFound = cb;
}

void BLEApi::onDeviceConnected(BLEDeviceEvent cb)
{
  _cbOnDeviceConnected = cb;
}

void BLEApi::onDeviceDisconnected(BLEDeviceEvent cb)
{
  _cbOnDeviceDisconnected = cb;
}

void BLEApi::onCharacteristicNotification(BLECharacteristicNotification cb)
{
  _cbOnCharacteristicNotification = cb;
}

/**
 * Connect to a device
 * @param address device address
 */
bool BLEApi::connect(BLEPeripheralID id)
{
  NimBLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    return true;
  }
  if (_bleBusy)
  {
    // a connect attempt is already running (re-entrancy from a callback)
    log_w("BLE busy, connect rejected");
    return false;
  }
  _bleBusy = true;
  // pause scanning only for the duration of this connect; other clients'
  // discovery resumes automatically afterwards (does not clear _scanRequested)
  pauseScanForConnect();

  // get MAC address from id
  NimBLEAddress address = addressFromId(id);

  bool connected = false;
  int8_t retry = 5;
  log_i("Connect attempt start");
  peripheral = NimBLEDevice::createClient();
  peripheral->setConnectTimeout(5000); // 5 s (NimBLE 2.x: ms) — feedback d'échec plus rapide
  // Intervalle de connexion court : le SDK TTLock fragmente les commandes en
  // trames de 20 octets (une trame par intervalle de connexion), donc un
  // intervalle court accélère nettement les longues séquences (lecture du
  // journal). Unités : 1,25 ms pour les intervalles, 10 ms pour la
  // supervision → 12 = 15 ms, latency 0, timeout 600 = 6000 ms.
  // Supervision portée de 2 s à 6 s : sous contention radio, 2 s sans paquet
  // coupaient le lien en plein handshake admin TTLock (No response to
  // checkAdmin). Si l'instabilité persiste, relâcher le max : (12, 24, 0, 600).
  peripheral->setConnectionParams(12, 12, 0, 600);
  do
  {
    // TODO: sometimes the connect fails and remains hanging in the semaphore, patch BLE lib ?
    // ----------------------------
    // Connect attempt start
    // lld_pdu_get_tx_flush_nb HCI packet count mismatch (0, 1)
    // [E][BLEClient.cpp:214] gattClientEventHandler(): Failed to connect, status=Unknown ESP_ERR error
    // Retry connection in 1s
    // ----------------------------
    connected = peripheral->connect(address);
    // Serial.println("Connect attempt ended");
    retry--;
    if (connected)
    {
      peripheral->setClientCallbacks(_clientCallback);
    }
    else
    {
      // log_d("Removing peripheral");
      // delete peripheral;
      if (retry > 0)
      {
        log_i("Retry connection in 500ms");
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
    }
  } while (!connected && retry > 0);
  if (connected)
  {
    log_i("Connected to [%s][%d]\n", address.toString().c_str(), retry);
    addConnection(id, peripheral);
  }
  else
  {
    log_d("Removing peripheral");
    NimBLEDevice::deleteClient(peripheral);
    log_e("Could not connect to [%s][%d]\n", address.toString().c_str(), retry);
  }
  // restore discovery for other clients if it was requested
  resumeScanIfRequested();
  _bleBusy = false;
  return connected;
}

bool BLEApi::disconnect(BLEPeripheralID id)
{

  NimBLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (peripheral->isConnected())
    {
      meminfo();
      peripheral->disconnect();
      // vTaskDelay(3000 / portTICK_PERIOD_MS);
      // delete peripheral;
      // meminfo();
    }
  }
  return true;
}

const std::vector<NimBLERemoteService *> *BLEApi::discoverServices(BLEPeripheralID id)
{
  NimBLEClient *peripheral = getConnection(id);
  if (peripheral)
  {
    if (!peripheral->isConnected())
    {
      return nullptr;
    }
    return &peripheral->getServices(true);
  }
  return nullptr;
}

const std::vector<NimBLERemoteCharacteristic *> *BLEApi::discoverCharacteristics(BLEPeripheralID id, std::string service)
{
  NimBLEClient *peripheral = getConnection(id);
  if (peripheral)
  {
    if (!peripheral->isConnected())
    {
      return nullptr;
    }
    NimBLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
    if (remoteService != nullptr)
    {
      return &remoteService->getCharacteristics(true);
    }
  }
  return nullptr;
}

std::string BLEApi::readCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic)
{
  NimBLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (!peripheral->isConnected())
    {
      return "";
    }
    NimBLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
    if (remoteService != nullptr)
    {
      NimBLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(characteristic));
      if (remoteCharacteristic->canRead())
      {
        return remoteCharacteristic->readValue();
      }
    }
  }
  return "";
}

bool BLEApi::notifyCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, bool notify)
{
  NimBLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (peripheral->isConnected())
    {
      NimBLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
      if (remoteService != nullptr)
      {
        NimBLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(characteristic));
        if (remoteCharacteristic->canNotify())
        {
          if (notify)
          {
            remoteCharacteristic->subscribe(true, _onCharacteristicNotification);
          }
          else
          {
            remoteCharacteristic->unsubscribe();
          }
          return true;
        }
      }
    }
  }
  return false;
}

bool BLEApi::writeCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, uint8_t *data, size_t length, bool withoutResponse)
{
  NimBLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (peripheral->isConnected())
    {
      NimBLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
      if (remoteService != nullptr)
      {
        NimBLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(characteristic));
        if (remoteCharacteristic->canWrite() || remoteCharacteristic->canWriteNoResponse())
        {
          remoteCharacteristic->writeValue(data, length, !withoutResponse);
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * DO NOT USE: Proxy method for setting up the ESP32 BLEDevice callback
 */
void BLEApi::_onDeviceFoundProxy(const NimBLEAdvertisedDevice *advertisedDevice)
{
  BLEPeripheralID devId = idFromAddress(advertisedDevice->getAddress());
  // BLE privacy renouvelle les MAC aléatoires : sans borne ce cache croît
  // indéfiniment et fragmente le tas. Les entrées ne servent que
  // transitoirement à connect() et sont réapprises au prochain
  // advertisement, donc une purge au plafond est sûre.
  if (addressTypes.size() >= ESP_GW_ADDR_TYPE_CACHE_MAX &&
      addressTypes.find(devId) == addressTypes.end())
  {
    addressTypes.clear();
  }
  addressTypes[devId] = advertisedDevice->getAddressType();
  _radarUpsert(devId, (int8_t)advertisedDevice->getRSSI(), advertisedDevice->getName().c_str());
  if (_cbOnDeviceFound)
  {
    _cbOnDeviceFound(const_cast<NimBLEAdvertisedDevice *>(advertisedDevice), devId);
  }
}

bool BLEApi::isScanning()
{
  return _isScanning;
}

uint8_t BLEApi::getRadar(const BLERadarEntry *&out)
{
  out = _radar;
  return _radarCount;
}

// Insert/update a bounded radar entry (fixed array, no heap). Updates an
// existing id in place; when full, evicts the oldest-seen entry. The name is
// truncated and sanitized to printable ASCII so the JSON stays well-formed.
void BLEApi::_radarUpsert(const BLEPeripheralID &id, int8_t rssi, const char *name)
{
  uint32_t now = millis();
  uint8_t slot = _radarCount;
  uint8_t oldestIdx = 0;
  uint32_t oldestSeen = 0xFFFFFFFF;
  for (uint8_t i = 0; i < _radarCount; i++)
  {
    if (_radar[i].id == id)
    {
      slot = i;
      break;
    }
    if (_radar[i].lastSeen < oldestSeen)
    {
      oldestSeen = _radar[i].lastSeen;
      oldestIdx = i;
    }
  }
  if (slot == _radarCount)
  {
    if (_radarCount < ESP_GW_RADAR_MAX)
    {
      _radarCount++;
    }
    else
    {
      slot = oldestIdx; // full: replace the least-recently-seen entry
    }
  }
  _radar[slot].id = id;
  _radar[slot].rssi = rssi;
  _radar[slot].lastSeen = now;
  uint8_t j = 0;
  if (name != nullptr)
  {
    for (; name[j] != '\0' && j < ESP_GW_RADAR_NAME_MAX - 1; j++)
    {
      char c = name[j];
      _radar[slot].name[j] = (c >= 0x20 && c < 0x7F) ? c : '?';
    }
  }
  _radar[slot].name[j] = '\0';
}

/**
 * DO NOT USE: Proxy method for setting up the ESP32 BLEDevice connect and disconnect events
 */
void BLEApi::_onDeviceInteractionProxy(BLEPeripheralID id, bool connected)
{
  if (connected)
  {
    Serial.println("***** Connect ACK");
    if (_cbOnDeviceConnected != nullptr)
    {
      _cbOnDeviceConnected(id);
    }
  }
  else
  {
    Serial.println("***** Disconnect ACK");
    NimBLEClient *peripheral = getConnection(id);
    if (peripheral != nullptr)
    {
      peripheral->setClientCallbacks(nullptr);
      delConnection(id);
      if (_cbOnDeviceDisconnected != nullptr)
      {
        _cbOnDeviceDisconnected(id);
      }
      vTaskDelay(200 / portTICK_PERIOD_MS);
      Serial.println("Dealocating memory");
      NimBLEDevice::deleteClient(peripheral);
      meminfo();
      // Connexion fermée : si c'était la dernière (activeConnections == 0) et
      // qu'un scan était demandé, la découverte BLE peut reprendre.
      resumeScanIfRequested();
    }
  }
}

void BLEApi::_onScanFinished(const NimBLEScanResults &results)
{
  _isScanning = false;
  bleScan->clearResults();
  Serial.println("BLE Scan stopped callback");
}

void BLEApi::_onCharacteristicNotification(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify)
{
  if (_cbOnCharacteristicNotification != nullptr)
  {
    // patch required, see https://github.com/espressif/arduino-esp32/issues/3367
    const NimBLERemoteService *service = characteristic->getRemoteService();
    const NimBLEClient *client = service->getClient();
    std::string dataStr = std::string((char *)data, length);
    NimBLEUUID svcUuid = service->getUUID();
    NimBLEUUID chrUuid = characteristic->getUUID();
    _cbOnCharacteristicNotification(
        idFromAddress(client->getPeerAddress()),
        svcUuid.to128().toString(),
        chrUuid.to128().toString(),
        dataStr,
        isNotify);
  }
}

bool BLEApi::addConnection(BLEPeripheralID id, NimBLEClient *device)
{
  if (activeConnections < MAX_CLIENT_CONNECTIONS)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (connections[i].device == nullptr)
      {
        connections[i].device = device;
        connections[i].id = id;
        activeConnections++;
        return true;
      }
    }
  }
  return false;
}

NimBLEClient *BLEApi::getConnection(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (connections[i].id == id)
      {
        return connections[i].device;
      }
    }
  }
  return nullptr;
}

void BLEApi::delConnection(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (connections[i].id == id)
      {
        connections[i].device = nullptr;
        activeConnections--;
      }
    }
  }
}

BLEPeripheralID BLEApi::idFromAddress(NimBLEAddress address)
{
  BLEPeripheralID peripheralUuid;
  memcpy(peripheralUuid.data(), address.getBase()->val, ESP_BD_ADDR_LEN);
  return peripheralUuid;
}

NimBLEAddress BLEApi::addressFromId(BLEPeripheralID id)
{
  // this is stupid
  uint8_t address[6];
  std::reverse_copy(id.data(), id.data() + sizeof address, address);
  return NimBLEAddress(address, addressTypes[id]);
}

std::string BLEApi::idToString(BLEPeripheralID id)
{
  // stack buffer instead of malloc/free: this is a hot path (every advertisement)
  char res[ESP_BD_ADDR_LEN * 2 + 1];
  snprintf(res, sizeof(res), "%02x%02x%02x%02x%02x%02x", id[5], id[4], id[3], id[2], id[1], id[0]);
  return std::string(res);
}

BLEPeripheralID BLEApi::idFromString(const char *idStr)
{
  int data[6];
  if (sscanf(idStr, "%2x%2x%2x%2x%2x%2x", &data[5], &data[4], &data[3], &data[2], &data[1], &data[0]) != 6)
  {
    log_e("Error parsing address");
  }
  BLEPeripheralID id;
  for (size_t i = 0; i < ESP_BD_ADDR_LEN; i++)
  {
    id[i] = data[i];
  }
  return id;
}