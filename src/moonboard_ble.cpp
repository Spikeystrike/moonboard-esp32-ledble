#include "moonboard_ble.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace
{
constexpr char SERVICE_UUID[] =
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char RX_CHARACTERISTIC_UUID[] =
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char TX_CHARACTERISTIC_UUID[] =
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

// The current MoonBoard app can send a complete route in one GATT write.
// A default ATT MTU of 23 only carries 20 payload bytes and can leave the
// first route without its closing '#'. 517 is the largest ESP32 BLE MTU and
// still interoperates with clients that negotiate a smaller value.
constexpr uint16_t PREFERRED_MTU = 517;
} // namespace

class MoonboardBleServerCallbacks : public BLEServerCallbacks
{
public:
    void onConnect(BLEServer *server) override
    {
        // Keep advertising so a second MoonBoard client can connect too.
        server->startAdvertising();
    }

    void onDisconnect(BLEServer *server) override
    {
        server->startAdvertising();
    }
};

class MoonboardBleCharacteristicCallbacks
    : public BLECharacteristicCallbacks
{
public:
    explicit MoonboardBleCharacteristicCallbacks(MoonboardBleServer *owner)
        : owner_(owner)
    {
    }

    void onWrite(BLECharacteristic *characteristic) override
    {
        owner_->appendReceived(characteristic->getValue());
    }

private:
    MoonboardBleServer *owner_;
};

MoonboardBleServer::MoonboardBleServer()
    : server_(nullptr),
      txCharacteristic_(nullptr),
      receiveMutex_(nullptr),
      receiveBuffer_{},
      receiveHead_(0),
      receiveSize_(0),
      receiveOverflow_(false),
      receiveStats_{}
{
}

bool MoonboardBleServer::begin(const char *localName)
{
    if (server_ != nullptr)
        return true;
    if (localName == nullptr || localName[0] == '\0')
        return false;

    receiveMutex_ = xSemaphoreCreateMutex();
    if (receiveMutex_ == nullptr)
        return false;

    BLEDevice::init(localName);
    if (BLEDevice::setMTU(PREFERRED_MTU) != ESP_OK)
        return false;
    server_ = BLEDevice::createServer();
    if (server_ == nullptr)
        return false;
    server_->setCallbacks(new MoonboardBleServerCallbacks());

    BLEService *service = server_->createService(SERVICE_UUID);
    if (service == nullptr)
        return false;

    txCharacteristic_ = service->createCharacteristic(
        TX_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY);
    if (txCharacteristic_ == nullptr)
        return false;
    txCharacteristic_->addDescriptor(new BLE2902());

    BLECharacteristic *rxCharacteristic = service->createCharacteristic(
        RX_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR);
    if (rxCharacteristic == nullptr)
        return false;
    rxCharacteristic->setCallbacks(
        new MoonboardBleCharacteristicCallbacks(this));

    service->start();
    BLEAdvertising *advertising = server_->getAdvertising();
    advertising->addServiceUUID(service->getUUID());
    advertising->start();
    return true;
}

bool MoonboardBleServer::connected() const
{
    return server_ != nullptr && server_->getConnectedCount() > 0;
}

int MoonboardBleServer::available() const
{
    if (receiveMutex_ == nullptr)
        return 0;
    if (xSemaphoreTake(receiveMutex_, portMAX_DELAY) != pdTRUE)
        return 0;
    const size_t availableBytes = receiveSize_;
    xSemaphoreGive(receiveMutex_);
    return static_cast<int>(availableBytes);
}

int MoonboardBleServer::read()
{
    if (receiveMutex_ == nullptr)
        return -1;
    if (xSemaphoreTake(receiveMutex_, portMAX_DELAY) != pdTRUE)
        return -1;
    if (receiveSize_ == 0)
    {
        xSemaphoreGive(receiveMutex_);
        return -1;
    }

    const uint8_t value = static_cast<uint8_t>(receiveBuffer_[receiveHead_]);
    receiveHead_ = (receiveHead_ + 1) % RECEIVE_CAPACITY;
    --receiveSize_;
    xSemaphoreGive(receiveMutex_);
    return value;
}

bool MoonboardBleServer::consumeOverflow()
{
    if (receiveMutex_ == nullptr)
        return false;
    if (xSemaphoreTake(receiveMutex_, portMAX_DELAY) != pdTRUE)
        return false;
    const bool overflowed = receiveOverflow_;
    receiveOverflow_ = false;
    xSemaphoreGive(receiveMutex_);
    return overflowed;
}

bool MoonboardBleServer::takeReceiveStats(
    MoonboardBleReceiveStats &stats)
{
    if (receiveMutex_ == nullptr)
        return false;
    if (xSemaphoreTake(receiveMutex_, portMAX_DELAY) != pdTRUE)
        return false;
    if (receiveStats_.writeCount == 0)
    {
        xSemaphoreGive(receiveMutex_);
        return false;
    }

    stats = receiveStats_;
    receiveStats_ = {};
    xSemaphoreGive(receiveMutex_);
    return true;
}

void MoonboardBleServer::appendReceived(const std::string &value)
{
    if (value.empty() || receiveMutex_ == nullptr)
        return;
    if (xSemaphoreTake(receiveMutex_, portMAX_DELAY) != pdTRUE)
        return;

    ++receiveStats_.writeCount;
    receiveStats_.byteCount += static_cast<uint32_t>(value.size());
    receiveStats_.lastWriteLength = value.size();
    if (value.size() > receiveStats_.maximumWriteLength)
        receiveStats_.maximumWriteLength = value.size();

    if (value.size() > RECEIVE_CAPACITY - receiveSize_)
    {
        // Discard an incomplete stream after overflow. A later `l` route
        // marker lets the protocol parser recover at the next app write.
        receiveHead_ = 0;
        receiveSize_ = 0;
        receiveOverflow_ = true;
    }

    const size_t start =
        value.size() > RECEIVE_CAPACITY
            ? value.size() - RECEIVE_CAPACITY
            : 0;
    for (size_t index = start; index < value.size(); ++index)
    {
        const size_t tail =
            (receiveHead_ + receiveSize_) % RECEIVE_CAPACITY;
        receiveBuffer_[tail] = value[index];
        ++receiveSize_;
    }
    xSemaphoreGive(receiveMutex_);
}
