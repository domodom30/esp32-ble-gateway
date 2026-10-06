#include "noble_api.h"
#include <WiFi.h>
#include <map>

bool NobleApi::ready = false;
Security *NobleApi::sec = nullptr;
WebSocketsServer *NobleApi::ws = nullptr;
Challenge NobleApi::challenges[WEBSOCKETS_SERVER_CLIENT_MAX];
uint32_t NobleApi::authDeadline[WEBSOCKETS_SERVER_CLIENT_MAX];
#define AUTH_TIMEOUT_MS 10000
PeripheralClient NobleApi::peripheralConnections[MAX_CLIENT_CONNECTIONS];
uint8_t NobleApi::activeConnections = 0;
QueueHandle_t NobleApi::txQueue = nullptr;
QueueHandle_t NobleApi::connReqQueue = nullptr;
QueueHandle_t NobleApi::connResQueue = nullptr;
TaskHandle_t NobleApi::connWorkerTask = nullptr;

// Sentinel client id meaning "send to every authenticated client".
#define WS_TX_BROADCAST 0xFE
#define WS_TX_QUEUE_LEN 16
#define CONN_QUEUE_LEN MAX_CLIENT_CONNECTIONS

struct WsTxItem
{
  uint8_t client;
  char *payload; // heap-allocated, freed by drainTx after sending
};
struct ConnReq
{
  uint8_t client;
  BLEPeripheralID id;
};
struct ConnRes
{
  uint8_t client;
  BLEPeripheralID id;
  bool ok;
};

bool isEmptyChallenge(Challenge challenge)
{
  for (auto i = 0; i < BLOCK_SIZE; i++)
  {
    if (challenge[i] != 0x00)
    {
      return false;
    }
  }
  return true;
}

void clearChallenge(Challenge challenge)
{
  for (auto i = 0; i < BLOCK_SIZE; i++)
  {
    challenge[i] = 0x00;
  }
}
/**
 * Initialize API
 */
bool NobleApi::init()
{
  // initialize clients and challenges
  for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
  {
    peripheralConnections[i].client = INVALID_CLIENT;
  }
  for (auto i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++)
  {
    clearChallenge(challenges[i]);
    authDeadline[i] = 0;
  }

  // instantiate security module
  sec = new Security(GwSettings::getAes());

  // initilalize BLE
  BLEApi::init();
  BLEApi::onDeviceFound(onBLEDeviceFound);
  BLEApi::onDeviceDisconnected(onBLEDeviceDisconnected);
  BLEApi::onCharacteristicNotification(onCharacteristicNotification);

  // marshalling primitives: WS tx queue (all sends flushed from the loop task)
  // and the connect worker (keeps the blocking connect off the loop task)
  txQueue = xQueueCreate(WS_TX_QUEUE_LEN, sizeof(WsTxItem));
  connReqQueue = xQueueCreate(CONN_QUEUE_LEN, sizeof(ConnReq));
  connResQueue = xQueueCreate(CONN_QUEUE_LEN, sizeof(ConnRes));
  xTaskCreatePinnedToCore(connWorker, "bleConnWorker", 8192, nullptr, 1, &connWorkerTask, 1);

  // initialize websocket
  ws = new WebSocketsServer(ESP_GW_WEBSOCKET_PORT);
  ws->enableHeartbeat(30000, 5000, 3);
  ws->begin();
  ws->onEvent(onWsEvent);
  ready = true;

  return ready;
}

/**
 * Loop to process websocket
 */
void NobleApi::loop()
{
  if (ready)
  {
    // Process websocket events
    ws->loop();

    // disconnect clients that did not authenticate within AUTH_TIMEOUT_MS
    uint32_t now = millis();
    for (uint8_t client = 0; client < WEBSOCKETS_SERVER_CLIENT_MAX; client++)
    {
      if (authDeadline[client] != 0 && now > authDeadline[client])
      {
        if (ws->clientIsConnected(client) && !isEmptyChallenge(challenges[client]))
        {
          Serial.printf("[%u] Auth timeout, disconnecting\n", client);
          ws->disconnect(client);
        }
        authDeadline[client] = 0;
      }
    }

    // hand back connect() results, then flush all queued WS messages. drainTx
    // is the ONLY place ws->sendTXT is called, so producers on other tasks
    // (NimBLE callbacks, connect worker) never touch the socket directly.
    drainConnResults();
    drainTx();
  }
}

/**
 * Cleanup after a client disconnects:
 * - disconnect connected devices
 * - remove client connection mappings
 * - remove challenges
 */
void NobleApi::clientDisconnectCleanup(uint8_t client)
{
  // disconnect all assigned peripheralUuid
  for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
  {
    if (peripheralConnections[i].client == client)
    {
      BLEApi::disconnect(peripheralConnections[i].id);
      delClient(peripheralConnections[i].id);
    }
  }

  clearChallenge(challenges[client]);
  authDeadline[client] = 0;
}

/**
 * Can the client connect to device ?
 */
bool NobleApi::clientCanConnect(uint8_t client, BLEPeripheralID id)
{
  uint8_t connectedClient = getClient(id);
  if (connectedClient != INVALID_CLIENT && connectedClient != client)
  {
    return false;
  }
  return true;
}

/**
 * Is client connected to the device ?
 */
bool NobleApi::clientConnected(uint8_t client, BLEPeripheralID id)
{
  uint8_t connectedClient = getClient(id);
  if (connectedClient == client)
  {
    return true;
  }
  return false;
}

/**
 * Process websocket events
 */
void NobleApi::onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_DISCONNECTED)
  {
    clientDisconnectCleanup(client);
    Serial.printf("[%u] Disconnected!\n", client);
    if (ws->connectedClients() == 0)
    {
      BLEApi::stopScan();
    }
    meminfo();
  }
  else if (type == WStype_CONNECTED)
  {
    meminfo();
    IPAddress ip = ws->remoteIP(client);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", client, ip[0], ip[1], ip[2], ip[3], payload);
    // Generate IV and send auth request
    initClient(client);
    sendAuthMessage(client);
  }
  else if (type == WStype_TEXT)
  {
    // Serial.printf("[%u] get  Text: %s\n", client, payload);

    JsonDocument command;
    DeserializationError error = deserializeJson(command, payload, length);

    if (error != DeserializationError::Ok)
    { // Check for errors in parsing
      Serial.printf("Message parsing failed: ");
      Serial.println(error.f_str());
      return;
    }
    else
    {
      const char *action = command["action"];
      if (action && strlen(action) > 0)
      {
        bool authenticated = false;
        if (isEmptyChallenge(challenges[client]))
        {
          authenticated = true;
        }
        if (!authenticated)
        {
          // client is not authenticated, only respond to auth
          if (strcmp(action, "auth") == 0)
          {
            const char *response = command["response"];
            // response is null when the key is absent; strlen(null) would crash,
            // letting an unauthenticated client reboot the gateway.
            if (response != nullptr && strlen(response) > 0)
            {
              checkAuth(client, response);
            }
          }
        }
        else
        {
          // client is authenticated, allow other commands

          // scan sfuff
          if (strcmp(action, "startScanning") == 0)
          {
            // TODO: if the scan is already running, send the list of discovered devices
            // TODO: setup (per connection ?) services filter

            // more or less a hack to allow for passive scanning as noble API does not have such parameter
            const bool active = command["allowDuplicates"];
            if (active)
            {
              BLEApi::startScan(0, false);
            }
            else
            {
              BLEApi::startScan(0, true);
            }
          }
          else if (strcmp(action, "stopScanning") == 0)
          {
            BLEApi::stopScan();
          }
          else
          {
            // stuff that requires periperalUuid
            const char *tempUuid = command["peripheralUuid"];
            if (tempUuid != nullptr)
            {
              BLEPeripheralID peripheralUuid = BLEApi::idFromString(tempUuid);

              // connection
              if (strcmp(action, "connect") == 0)
              {
                // Reserve the peripheral for this client, then offload the
                // blocking connect to connWorker. The connect/disconnect reply
                // is sent later from drainConnResults (loop task) so this
                // handler — and the WebSocket — never blocks for ~seconds.
                if (clientCanConnect(client, peripheralUuid))
                {
                  addClient(peripheralUuid, client); // reserve the slot
                  ConnReq req = {client, peripheralUuid};
                  if (connReqQueue == nullptr || xQueueSend(connReqQueue, &req, 0) != pdTRUE)
                  {
                    // worker queue full: release the reservation and report busy
                    delClient(peripheralUuid);
                    sendDisconnected(client, peripheralUuid, "busy");
                  }
                }
                else
                {
                  sendDisconnected(client, peripheralUuid, "denied");
                }
              }
              // actions that require a connection
              else if (clientConnected(client, peripheralUuid))
              {
                if (strcmp(action, "disconnect") == 0)
                {
                  // Client-initiated disconnect (noble disconnect / cancelConnect).
                  // With a live link, the reply is sent by onBLEDeviceDisconnected
                  // once NimBLE reports the link down. Without one (connect still
                  // in flight, link already gone) reply now so the client never
                  // waits; drainConnResults releases a connect that completes later.
                  if (!BLEApi::disconnect(peripheralUuid))
                  {
                    delClient(peripheralUuid);
                    sendDisconnected(client, peripheralUuid, "not connected");
                  }
                }
                else if (strcmp(action, "discoverServices") == 0)
                {
                  const std::vector<NimBLERemoteService *> *services = BLEApi::discoverServices(peripheralUuid);
                  if (services != nullptr)
                  {
                    sendServices(client, peripheralUuid, services);
                  }
                }
                else if (strcmp(action, "discoverCharacteristics") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  const std::vector<NimBLERemoteCharacteristic *> *characteristics = BLEApi::discoverCharacteristics(peripheralUuid, serviceUuid);
                  if (characteristics != nullptr)
                  {
                    sendCharacteristics(client, peripheralUuid, serviceUuid, characteristics);
                  }
                  else
                  {
                    delClient(peripheralUuid);
                    sendDisconnected(client, peripheralUuid, "aborted");
                  }
                }
                else if (strcmp(action, "read") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::string characteristicUuid = command["characteristicUuid"];
                  std::string value = BLEApi::readCharacteristic(peripheralUuid, serviceUuid, characteristicUuid);
                  sendCharacteristicValue(client, peripheralUuid, serviceUuid, characteristicUuid, value);
                }
                else if (strcmp(action, "write") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::string characteristicUuid = command["characteristicUuid"];
                  std::string dataHex = command["data"];
                  size_t length = dataHex.length() / 2;
                  // BLE attribute values are at most 512 bytes; reject oversized
                  // payloads so the stack VLA below stays bounded
                  if (length == 0 || length > 512)
                  {
                    Serial.printf("Rejected write: invalid data length %u\n", (unsigned)length);
                    sendCharacteristicWrite(client, peripheralUuid, serviceUuid, characteristicUuid);
                  }
                  else
                  {
                    uint8_t data[length];
                    sec->fromHex(dataHex.c_str(), length * 2, data);
                    bool withoutResponse = command["withoutResponse"];
                    // success and failure currently send the same ack
                    BLEApi::writeCharacteristic(peripheralUuid, serviceUuid, characteristicUuid, data, length, withoutResponse);
                    sendCharacteristicWrite(client, peripheralUuid, serviceUuid, characteristicUuid);
                  }
                }
                else if (strcmp(action, "notify") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::string characteristicUuid = command["characteristicUuid"];
                  bool notify = command["notify"];
                  // subscribe or unsubscribe
                  BLEApi::notifyCharacteristic(peripheralUuid, serviceUuid, characteristicUuid, notify);
                  sendCharacteristicNotification(client, peripheralUuid, serviceUuid, characteristicUuid, notify);
                }
              }
              else
              {
                sendDisconnected(client, peripheralUuid, "not connected");
              }
            }
            else
            {
              // failed to read peripheralUuid
            }
          }
        }
      }
      command.clear();
    }
  }
  else if (type == WStype_PONG)
  {
    // do nothing
  }
  else
  {
    Serial.printf("Type not implemented [%u]\n", type);
  }
}

// An authenticated client has an empty challenge (cleared on successful auth).
// Same predicate drainTx uses to fan out broadcasts.
bool NobleApi::hasAuthenticatedClient()
{
  for (uint8_t c = 0; c < WEBSOCKETS_SERVER_CLIENT_MAX; c++)
  {
    if (ws->clientIsConnected(c) && isEmptyChallenge(challenges[c]))
    {
      return true;
    }
  }
  return false;
}

bool NobleApi::isHaConnected()
{
  return WiFi.status() == WL_CONNECTED && hasAuthenticatedClient();
}

void NobleApi::onBLEDeviceFound(NimBLEAdvertisedDevice *advertisedDevice, BLEPeripheralID id)
{
  // No authenticated noble client means nobody consumes discover events, so we
  // skip the JSON build + malloc that drainTx would otherwise just discard.
  if (!hasAuthenticatedClient())
  {
    return;
  }

  JsonDocument command;
  command["type"] = "discover";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["address"] = advertisedDevice->getAddress().toString();
  if (advertisedDevice->getAddressType() == BLE_ADDR_PUBLIC || advertisedDevice->getAddressType() == BLE_ADDR_PUBLIC_ID)
  {
    command["addressType"] = "public";
  }
  else if (advertisedDevice->getAddressType() == BLE_ADDR_RANDOM || advertisedDevice->getAddressType() == BLE_ADDR_RANDOM_ID)
  {
    command["addressType"] = "random";
  }
  else
  {
    command["addressType"] = "unknown";
  }
  command["connectable"] = "true";
  command["rssi"] = advertisedDevice->getRSSI();
  command["advertisement"]["localName"] = advertisedDevice->getName();
  if (advertisedDevice->haveTXPower())
  {
    command["advertisement"]["txPowerLevel"] = advertisedDevice->getTXPower();
  }
  if (advertisedDevice->haveServiceUUID())
  {
    JsonArray serviceUuids = command["advertisement"]["serviceUuids"].to<JsonArray>();
    serviceUuids.add(advertisedDevice->getServiceUUID().toString());
  }
  if (advertisedDevice->haveManufacturerData())
  {
    char manufacturerData[advertisedDevice->getManufacturerData().length() * 2 + 1];
    // toHex already writes the trailing '\0' at index length*2
    sec->toHex((uint8_t *)advertisedDevice->getManufacturerData().data(), advertisedDevice->getManufacturerData().length(), manufacturerData);
    command["advertisement"]["manufacturerData"] = manufacturerData;
  }

  sendJsonMessage(command);
  command.clear();
}

void NobleApi::onBLEDeviceDisconnected(BLEPeripheralID id)
{
  uint8_t client = getClient(id);
  if (client != INVALID_CLIENT)
  {
    sendDisconnected(client, id);
    delClient(id);
  }
}

void NobleApi::onCharacteristicNotification(BLEPeripheralID id, const std::string &service, const std::string &characteristic, const std::string &data, bool isNotify)
{
  uint8_t client = getClient(id);
  if (client != INVALID_CLIENT)
  {
    sendCharacteristicValue(
        client,
        id,
        service,
        characteristic,
        data,
        isNotify);
  }
}

void NobleApi::initClient(uint8_t client)
{
  sec->generateIV((uint8_t *)challenges[client]);
  authDeadline[client] = millis() + AUTH_TIMEOUT_MS;
}

void NobleApi::checkAuth(uint8_t client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    if (!isEmptyChallenge(challenges[client]))
    {
      uint8_t encryptedResponse[responseLength / 2];
      size_t encryptedResponseLength = sec->fromHex(response, responseLength, encryptedResponse);

      uint8_t decryptedResponse[encryptedResponseLength + 1];
      size_t decryptedResponseLength = sec->decrypt((uint8_t *)challenges[client], encryptedResponse, encryptedResponseLength, decryptedResponse);
      decryptedResponse[encryptedResponseLength] = '\0';

      if (strcmp((char *)decryptedResponse, GwSettings::getBleToken()) == 0)
      {
        clearChallenge(challenges[client]);
        authDeadline[client] = 0;
        sendState(client);
      }
      else
      {
        Serial.println("Authentication failed");
        ESP_LOG_BUFFER_HEXDUMP("Decrypted", decryptedResponse, decryptedResponseLength + 1, esp_log_level_t::ESP_LOG_INFO);
        sendAuthMessage(client);
      }
    }
  }
}

void NobleApi::sendJsonMessage(JsonDocument &command, const uint8_t client)
{
  size_t messageLength = measureJson(command) + 1;
  // Heap-allocate (not a stack VLA): this runs on the NimBLE host task too,
  // whose stack is far smaller than the loop task's, and the buffer is owned by
  // the queue until drainTx frees it.
  char *payload = (char *)malloc(messageLength);
  if (payload != nullptr)
  {
    serializeJson(command, payload, messageLength);
    enqueueTx(client, payload); // ownership transferred to the queue
  }
  command.clear();
}

void NobleApi::sendJsonMessage(JsonDocument &command)
{
  size_t messageLength = measureJson(command) + 1;
  char *payload = (char *)malloc(messageLength);
  if (payload != nullptr)
  {
    serializeJson(command, payload, messageLength);
    // fanned out to every authenticated client in drainTx (loop task)
    enqueueTx(WS_TX_BROADCAST, payload);
  }
  command.clear();
}

// Queue an outgoing message; the queue takes ownership of `payload`. Never
// blocks the producer: on a full/unready queue the message is dropped+freed.
void NobleApi::enqueueTx(uint8_t client, char *payload)
{
  WsTxItem item = {client, payload};
  if (txQueue == nullptr || xQueueSend(txQueue, &item, 0) != pdTRUE)
  {
    free(payload);
  }
}

// Flush every queued message to the socket. ONLY called from the loop task, so
// ws->sendTXT is never invoked concurrently from another task.
void NobleApi::drainTx()
{
  if (txQueue == nullptr)
  {
    return;
  }
  WsTxItem item;
  while (xQueueReceive(txQueue, &item, 0) == pdTRUE)
  {
    if (item.client == WS_TX_BROADCAST)
    {
      for (uint8_t c = 0; c < WEBSOCKETS_SERVER_CLIENT_MAX; c++)
      {
        if (ws->clientIsConnected(c) && isEmptyChallenge(challenges[c]))
        {
          ws->sendTXT(c, item.payload);
        }
      }
    }
    else if (ws->clientIsConnected(item.client))
    {
      ws->sendTXT(item.client, item.payload);
    }
    free(item.payload);
  }
}

// Handle connect() results produced by connWorker (loop task context).
void NobleApi::drainConnResults()
{
  if (connResQueue == nullptr)
  {
    return;
  }
  ConnRes res;
  while (xQueueReceive(connResQueue, &res, 0) == pdTRUE)
  {
    if (res.ok)
    {
      // The client may have dropped or released the reservation while the
      // connect was in flight; only confirm if it still owns the peripheral.
      if (ws->clientIsConnected(res.client) && clientConnected(res.client, res.id))
      {
        sendConnected(res.client, res.id);
      }
      else
      {
        BLEApi::disconnect(res.id);
        delClient(res.id);
      }
    }
    else
    {
      delClient(res.id);
      sendDisconnected(res.client, res.id, "failed");
    }
  }
}

// Dedicated task: runs the blocking BLEApi::connect off the loop task.
void NobleApi::connWorker(void *arg)
{
  if (connReqQueue == nullptr || connResQueue == nullptr)
  {
    vTaskDelete(nullptr);
    return;
  }
  ConnReq req;
  for (;;)
  {
    if (xQueueReceive(connReqQueue, &req, portMAX_DELAY) == pdTRUE)
    {
      bool ok = BLEApi::connect(req.id);
      ConnRes res = {req.client, req.id, ok};
      xQueueSend(connResQueue, &res, portMAX_DELAY);
    }
  }
}

void NobleApi::sendAuthMessage(const uint8_t client)
{
  if (!isEmptyChallenge(challenges[client]))
  {
    JsonDocument command;
    command["type"] = "auth";
    char challenge[BLOCK_SIZE * 2 + 1];
    sec->toHex((uint8_t *)challenges[client], BLOCK_SIZE, challenge);
    command["challenge"] = challenge;
    sendJsonMessage(command, client);
    command.clear();
  }
}

void NobleApi::sendState(const uint8_t client)
{
  JsonDocument command;
  command["type"] = "stateChange";
  if (BLEApi::isReady())
  {
    command["state"] = "poweredOn";
  }
  else
  {
    command["state"] = "poweredOff";
  }
  sendJsonMessage(command, client);
  command.clear();
}

void NobleApi::sendConnected(const uint8_t client, BLEPeripheralID id)
{
  JsonDocument command;
  command["type"] = "connect";
  command["peripheralUuid"] = BLEApi::idToString(id);
  sendJsonMessage(command, client);
}

void NobleApi::sendDisconnected(const uint8_t client, BLEPeripheralID id)
{
  sendDisconnected(client, id, "");
}

void NobleApi::sendDisconnected(const uint8_t client, BLEPeripheralID id, std::string reason)
{
  JsonDocument command;
  command["type"] = "disconnect";
  command["peripheralUuid"] = BLEApi::idToString(id);
  if (reason != "")
  {
    command["reason"] = reason;
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendServices(const uint8_t client, BLEPeripheralID id, const std::vector<NimBLERemoteService *> *services)
{
  JsonDocument command;
  command["type"] = "servicesDiscover";
  command["peripheralUuid"] = BLEApi::idToString(id);
  JsonArray serviceUuids = command["serviceUuids"].to<JsonArray>();
  for (NimBLERemoteService *service : *services)
  {
    NimBLEUUID uuid = service->getUUID();
    serviceUuids.add(uuid.to128().toString());
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristics(const uint8_t client, BLEPeripheralID id, std::string service, const std::vector<NimBLERemoteCharacteristic *> *characteristics)
{
  JsonDocument command;
  command["type"] = "characteristicsDiscover";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  JsonArray characteristicsUuids = command["characteristics"].to<JsonArray>();
  for (NimBLERemoteCharacteristic *characteristic : *characteristics)
  {
    JsonObject charact = characteristicsUuids.add<JsonObject>();
    NimBLEUUID chrUuid = characteristic->getUUID();
    charact["uuid"] = chrUuid.to128().toString();
    JsonArray properties = charact["properties"].to<JsonArray>();
    if (characteristic->canRead())
    {
      properties.add("read");
    }
    if (characteristic->canWrite())
    {
      properties.add("write");
    }
    if (characteristic->canWriteNoResponse())
    {
      properties.add("writeWithoutResponse");
    }
    if (characteristic->canNotify())
    {
      properties.add("notify");
    }
    if (characteristic->canIndicate())
    {
      properties.add("indicate");
    }
    if (characteristic->canBroadcast())
    {
      properties.add("broadcast");
    }
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicValue(
    const uint8_t client,
    BLEPeripheralID id,
    std::string service,
    std::string characteristic,
    std::string value,
    bool isNotification)
{
  JsonDocument command;
  command["type"] = "read";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  if (value.length() > 0)
  {
    char hexValue[value.length() * 2 + 1];
    sec->toHex((uint8_t *)value.c_str(), value.length(), hexValue);
    command["data"] = std::string(hexValue);
  }
  else
  {
    command["data"] = "00";
  }
  command["isNotification"] = isNotification;
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicNotification(
    const uint8_t client,
    BLEPeripheralID id,
    std::string service,
    std::string characteristic,
    bool state)
{
  JsonDocument command;
  command["type"] = "notify";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  command["state"] = state;
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicWrite(const uint8_t client, BLEPeripheralID id, std::string service, std::string characteristic)
{
  JsonDocument command;
  command["type"] = "write";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  sendJsonMessage(command, client);
}

bool NobleApi::addClient(BLEPeripheralID id, uint8_t client)
{
  if (activeConnections < MAX_CLIENT_CONNECTIONS)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (peripheralConnections[i].client == INVALID_CLIENT)
      {
        peripheralConnections[i].client = client;
        peripheralConnections[i].id = id;
        activeConnections++;
        return true;
      }
    }
  }
  return false;
}

uint8_t NobleApi::getClient(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (peripheralConnections[i].id == id)
      {
        return peripheralConnections[i].client;
      }
    }
  }
  return INVALID_CLIENT;
}

void NobleApi::delClient(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (peripheralConnections[i].id == id)
      {
        peripheralConnections[i].client = INVALID_CLIENT;
        activeConnections--;
      }
    }
  }
}