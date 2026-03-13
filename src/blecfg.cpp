#include "blecfg.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLEServer *bleServer = nullptr;
static const Config *bleConfig = nullptr;
static bool bleStarted = false;
static char currentBleName[32] = "";

static void buildBleDeviceName(const Config &config, char *nameOut, size_t nameOutLen)
{
    if (!nameOut || nameOutLen == 0)
    {
        return;
    }

    if (config.mqttMac10[0] != '\0')
    {
        strlcpy(nameOut, config.mqttMac10, nameOutLen);
        return;
    }

    if (config.AppNombre[0] != '\0')
    {
        strlcpy(nameOut, config.AppNombre, nameOutLen);
        return;
    }

    strlcpy(nameOut, "Astronova", nameOutLen);
}

static void applyBleDeviceName(const char *deviceName)
{
    if (!deviceName || deviceName[0] == '\0')
    {
        return;
    }

    NimBLEDevice::setDeviceName(deviceName);

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData advertisementData;
    NimBLEAdvertisementData scanResponseData;

    advertisementData.setName(deviceName);
    advertisementData.setCompleteServices(NimBLEUUID("180A"));
    scanResponseData.setName(deviceName);

    advertising->stop();
    advertising->setAdvertisementData(advertisementData);
    advertising->setScanResponseData(scanResponseData);
    advertising->start();

    strlcpy(currentBleName, deviceName, sizeof(currentBleName));

    Serial.print("[BLE] Advertising activo como ");
    Serial.println(currentBleName);
}

void bleBegin(const Config &config)
{
    if (bleStarted)
    {
        return;
    }

    bleConfig = &config;

    char deviceName[32];
    buildBleDeviceName(config, deviceName, sizeof(deviceName));

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    bleServer = NimBLEDevice::createServer();

    bleStarted = true;
    applyBleDeviceName(deviceName);
}

void bleHandle()
{
    if (!bleStarted || !bleServer || !bleConfig)
    {
        return;
    }

    char deviceName[32];
    buildBleDeviceName(*bleConfig, deviceName, sizeof(deviceName));
    if (strcmp(deviceName, currentBleName) == 0)
    {
        return;
    }

    applyBleDeviceName(deviceName);
}
