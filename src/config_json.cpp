#include "config_json.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

// Documento JSON usado para leer/escribir configuracion
JsonDocument jsonDoc;

static bool ensureSpiffsMounted()
{
    static bool mounted = false;
    if (mounted)
    {
        return true;
    }

    mounted = SPIFFS.begin(true);
    if (!mounted)
    {
        Serial.println("Error: no se pudo montar SPIFFS");
    }
    return mounted;
}

bool loadConfigFromFile(const char *filename, Config &config)
{
    if (!ensureSpiffsMounted())
    {
        return false;
    }

    File file = SPIFFS.open(filename, FILE_READ);
    if (!file)
    {
        Serial.println("Error: No se pudo abrir el archivo de configuracion");
        return false;
    }

    DeserializationError error = deserializeJson(jsonDoc, file);
    file.close();

    if (error)
    {
        Serial.print("Error al parsear JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    strlcpy(config.AppNombre, jsonDoc["AppNombre"] | config.AppNombre, sizeof(config.AppNombre));
    strlcpy(config.AppBluid, jsonDoc["AppBluid"] | config.AppBluid, sizeof(config.AppBluid));
    config.AppVersion = jsonDoc["AppVersion"] | config.AppVersion;
    strlcpy(config.AppProducto, jsonDoc["AppProducto"] | config.AppProducto, sizeof(config.AppProducto));
    strlcpy(config.AppModelo, jsonDoc["AppModelo"] | config.AppModelo, sizeof(config.AppModelo));
    strlcpy(config.mqttHost, jsonDoc["mqttHost"] | config.mqttHost, sizeof(config.mqttHost));
    strlcpy(config.mqttUsuario, jsonDoc["mqttUsuario"] | config.mqttUsuario, sizeof(config.mqttUsuario));
    strlcpy(config.mqttPassword, jsonDoc["mqttPassword"] | config.mqttPassword, sizeof(config.mqttPassword));
    strlcpy(config.wifiHost, jsonDoc["wifiHost"] | config.wifiHost, sizeof(config.wifiHost));
    strlcpy(config.wifiPassword, jsonDoc["wifiPassword"] | config.wifiPassword, sizeof(config.wifiPassword));
    strlcpy(config.wifiApPassword, jsonDoc["wifiApPassword"] | config.wifiApPassword, sizeof(config.wifiApPassword));
    strlcpy(config.httpHost, jsonDoc["httpHost"] | config.httpHost, sizeof(config.httpHost));
    config.httPort = jsonDoc["httPort"] | config.httPort;
    strlcpy(config.mqttVer0Cortex, jsonDoc["mqttVer0Cortex"] | config.mqttVer0Cortex, sizeof(config.mqttVer0Cortex));
    strlcpy(config.mqttVer1Cortex, jsonDoc["mqttVer1Cortex"] | config.mqttVer1Cortex, sizeof(config.mqttVer1Cortex));
    strlcpy(config.mqttVer0Esp32, jsonDoc["mqttVer0Esp32"] | config.mqttVer0Esp32, sizeof(config.mqttVer0Esp32));
    strlcpy(config.mqttVer1Esp32, jsonDoc["mqttVer1Esp32"] | config.mqttVer1Esp32, sizeof(config.mqttVer1Esp32));

    Serial.println("Configuracion cargada correctamente");
    return true;
}

bool saveConfigToFile(const char *filename, const Config &config)
{
    if (!ensureSpiffsMounted())
    {
        return false;
    }

    jsonDoc.clear();
    jsonDoc["AppNombre"] = config.AppNombre;
    jsonDoc["AppBluid"] = config.AppBluid;
    jsonDoc["AppVersion"] = config.AppVersion;
    jsonDoc["AppProducto"] = config.AppProducto;
    jsonDoc["AppModelo"] = config.AppModelo;
    jsonDoc["mqttHost"] = config.mqttHost;
    jsonDoc["mqttUsuario"] = config.mqttUsuario;
    jsonDoc["mqttPassword"] = config.mqttPassword;
    jsonDoc["wifiHost"] = config.wifiHost;
    jsonDoc["wifiPassword"] = config.wifiPassword;
    jsonDoc["wifiApPassword"] = config.wifiApPassword;
    jsonDoc["httpHost"] = config.httpHost;
    jsonDoc["httPort"] = config.httPort;
    jsonDoc["mqttVer0Cortex"] = config.mqttVer0Cortex;
    jsonDoc["mqttVer1Cortex"] = config.mqttVer1Cortex;
    jsonDoc["mqttVer0Esp32"] = config.mqttVer0Esp32;
    jsonDoc["mqttVer1Esp32"] = config.mqttVer1Esp32;

    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file)
    {
        Serial.println("Error: No se pudo crear el archivo de configuracion");
        return false;
    }

    if (serializeJson(jsonDoc, file) == 0)
    {
        Serial.println("Error al escribir JSON al archivo");
        file.close();
        return false;
    }

    file.close();
    Serial.println("Configuracion guardada correctamente");
    return true;
}