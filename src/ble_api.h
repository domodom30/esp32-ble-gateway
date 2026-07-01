#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED

#ifndef MAX_CLIENT_CONNECTIONS
#define MAX_CLIENT_CONNECTIONS 5
#endif

// BLE scan duty cycle (ms, NimBLE 2.x). Default ~90% is aggressive and is a
// prime suspect for WiFi 2.4 GHz coexistence issues — tune for diagnostics.
#ifndef ESP_GW_SCAN_INTERVAL
#define ESP_GW_SCAN_INTERVAL 100
#endif
#ifndef ESP_GW_SCAN_WINDOW
#define ESP_GW_SCAN_WINDOW 90
#endif

// Upper bound for the per-peripheral address-type cache. BLE privacy rotates
// random MACs, so this map would otherwise grow without bound and fragment
// the heap. ~128 * ~48 B ≈ 6 KB capped; entries are re-learned on the next
// advertisement so a purge on overflow is safe.
#ifndef ESP_GW_ADDR_TYPE_CACHE_MAX
#define ESP_GW_ADDR_TYPE_CACHE_MAX 128
#endif

#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS MAX_CLIENT_CONNECTIONS

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_bt_defs.h>
#include <functional>
#include "util.h"

class myAdvertisedDeviceCallbacks;
class myClientCallbacks;

typedef std::array<uint8_t, ESP_BD_ADDR_LEN> BLEPeripheralID;
typedef std::function<void(NimBLEAdvertisedDevice *advertisedDevice, BLEPeripheralID id)> BLEDeviceFound;
typedef std::function<void(BLEPeripheralID id)> BLEDeviceEvent;
typedef std::function<void(BLEPeripheralID id, const std::string &service, const std::string &characteristic, const std::string &data, bool isNotify)> BLECharacteristicNotification;
struct BLEConnection
{
  BLEPeripheralID id;
  NimBLEClient *device;
};

// One bounded address-type cache entry (fixed-size, no heap). Replaces a
// std::map whose per-node heap allocations fragmented the heap as BLE privacy
// rotated random MACs.
struct AddrTypeEntry
{
  BLEPeripheralID id;
  uint8_t type;
  uint32_t lastSeen;
};

class BLEApi
{
public:
  static void init();
  static bool isReady();
  static bool startScan(uint32_t duration = 0, bool active = true);
  static bool stopScan();
  static bool isScanning();
  static void onDeviceFound(BLEDeviceFound cb);
  static void onDeviceConnected(BLEDeviceEvent cb);
  static void onDeviceDisconnected(BLEDeviceEvent cb);
  static void onCharacteristicNotification(BLECharacteristicNotification cb);
  static bool connect(BLEPeripheralID);
  static bool disconnect(BLEPeripheralID);
  static const std::vector<NimBLERemoteService *> *discoverServices(BLEPeripheralID id);
  static const std::vector<NimBLERemoteCharacteristic *> *discoverCharacteristics(BLEPeripheralID id, const std::string &service);
  static std::string readCharacteristic(BLEPeripheralID id, const std::string &service, const std::string &characteristic);
  static bool notifyCharacteristic(BLEPeripheralID id, const std::string &service, const std::string &characteristic, bool notify = true);
  static bool writeCharacteristic(BLEPeripheralID id, const std::string &service, const std::string &characteristic, uint8_t *data, size_t length, bool withoutResponse = true);
  static BLEPeripheralID idFromAddress(NimBLEAddress address);
  static NimBLEAddress addressFromId(BLEPeripheralID id);
  static std::string idToString(BLEPeripheralID id);
  static BLEPeripheralID idFromString(const char *idStr);

private:
  friend class myAdvertisedDeviceCallbacks;
  friend class myClientCallbacks;
  static bool _isReady;
  static bool _isScanning;
  // desired scan state (kept across connect() pauses) + last scan params
  static bool _scanRequested;
  static bool _scanActiveMode;
  static uint32_t _scanDuration;
  // re-entrancy guard around a connect attempt
  static volatile bool _bleBusy;
  static bool stopScanInternal();
  static void pauseScanForConnect();
  static void resumeScanIfRequested();
  static NimBLEScanCallbacks *_advertisedDeviceCallback;
  static NimBLEClientCallbacks *_clientCallback;
  static NimBLEScan *bleScan;
  // bounded address-type cache (fixed array, no heap; replaces std::map)
  static AddrTypeEntry _addrTypes[ESP_GW_ADDR_TYPE_CACHE_MAX];
  static uint8_t _addrTypeCount;
  static void _storeAddressType(const BLEPeripheralID &id, uint8_t type);
  static uint8_t _lookupAddressType(const BLEPeripheralID &id);
  static void _onScanFinished(const NimBLEScanResults &results);
  static void _onCharacteristicNotification(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify);
  static void _onDeviceFoundProxy(const NimBLEAdvertisedDevice *advertisedDevice);
  static void _onDeviceInteractionProxy(BLEPeripheralID id, bool connected);
  static BLEDeviceFound _cbOnDeviceFound;
  static BLEDeviceEvent _cbOnDeviceConnected;
  static BLEDeviceEvent _cbOnDeviceDisconnected;
  static BLECharacteristicNotification _cbOnCharacteristicNotification;
  static BLEConnection connections[MAX_CLIENT_CONNECTIONS];
  static uint8_t activeConnections;
  static bool addConnection(BLEPeripheralID id, NimBLEClient *device);
  static NimBLEClient *getConnection(BLEPeripheralID id);
  static void delConnection(BLEPeripheralID id);
};

#endif