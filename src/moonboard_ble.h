#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class BLECharacteristic;
class BLEServer;
class MoonboardBleCharacteristicCallbacks;
class MoonboardBleServerCallbacks;

class MoonboardBleServer
{
public:
    MoonboardBleServer();

    bool begin(const char *localName);
    bool connected() const;
    int available() const;
    int read();
    bool consumeOverflow();

private:
    static constexpr size_t RECEIVE_CAPACITY = 4096;

    void appendReceived(const std::string &value);

    BLEServer *server_;
    BLECharacteristic *txCharacteristic_;
    SemaphoreHandle_t receiveMutex_;
    char receiveBuffer_[RECEIVE_CAPACITY];
    size_t receiveHead_;
    size_t receiveSize_;
    bool receiveOverflow_;

    friend class MoonboardBleCharacteristicCallbacks;
    friend class MoonboardBleServerCallbacks;
};
