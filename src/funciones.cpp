#include "funciones.h"
#include "config.h"
#include "config_json.h"
#include "wifihttp.h"
#include "wificfg.h"
#include <Arduino.h>
#include <ctype.h>
#include <SPIFFS.h>
#include <time.h>

/**
 * Inicializa la aplicación
 * Configura el LED, inicia Serial y muestra mensaje de bienvenida
 */
void iniciaApp(Config &config, const char *configFilename)
{
    // Configurar LED
    pinMode(LED_PIN, OUTPUT);
    setStatusLed(false);

    Serial.begin(115200);
    delay(5000); // Espera para que windows detecte el puerto serie virtual.
    Serial.println("Sistema iniciado");
    randomSeed((uint32_t)micros());

    initDefaultConfig(config);

    bool loadedFromFile = loadConfigFromFile(configFilename, config);
    if (!loadedFromFile)
    {
        Serial.println("No existe config.json o no es valido. Se crea con valores por defecto.");
    }

    wifiGetMACAddresses(config);
    calcularMqttMacs(config);
    calcularMqttDatos(config);

    if (!saveConfigToFile(configFilename, config))
    {
        Serial.println("Error: No se pudo guardar la configuracion en SPIFFS");
    }

    mostrarConfiguracionSerial(config);
    conectarWiFiAPySTA(config);

    if (!syncEsp32DateTimeFromServer(config))
    {
        Serial.println("Aviso: no se pudo sincronizar la fecha con el servidor HTTP");
    }
}

// Hace parpadear el LED un número determinado de veces
void LedParpadea(int veces, int periodo_ms_on, int periodo_ms_off)
{
    for (int i = 0; i < veces; i++)
    {
        setStatusLed(true);
        delay(periodo_ms_on);
        setStatusLed(false);
        delay(periodo_ms_off);
    }

    // Al finalizar el parpadeo, restaurar estado segun conectividad STA.
    setStatusLed(isWiFiConnected());
}

// Reinicia el ESP32 con parpadeo previo de advertencia
void ResetEsp32()
{
    LedParpadea(10, 100, 100);
    ESP.restart();
}

bool borrarConfigJson(const char *configFilename, bool &fileExisted, bool &mountOk)
{
    mountOk = SPIFFS.begin(true);
    if (!mountOk)
    {
        fileExisted = false;
        return false;
    }

    fileExisted = SPIFFS.exists(configFilename);
    if (!fileExisted)
    {
        return false;
    }

    return SPIFFS.remove(configFilename);
}

bool VerificaBoleano(String value)
{
    value.trim();
    value.toLowerCase();
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

void calcularMqttDatos(Config &config)
{
    const char *productoPath = nullptr;
    char modelPrefix = static_cast<char>(toupper(static_cast<unsigned char>(config.AppModelo[0])));
    // EL MODELO ANCPLUS TIENE SUBMODELOS QUE SON LOG_PLUS
    if (strcmp(config.AppProducto, "ANCPLUS") == 0)
    {
        switch (modelPrefix) //
        {
        case '1':
            productoPath = "ANC_PLUS";
            strlcpy(config.mqttModelo, "A1", sizeof(config.mqttModelo));
            break;
        case '5':
            productoPath = "ANC_PLUS";
            strlcpy(config.mqttModelo, "A2", sizeof(config.mqttModelo));
            break;
        case '9':
            productoPath = "LOG_PLUS";
            strlcpy(config.mqttModelo, "D1", sizeof(config.mqttModelo));
            break;
        case 'D':
            productoPath = "LOG_PLUS";
            strlcpy(config.mqttModelo, "D2", sizeof(config.mqttModelo));
            break;
        default:
            productoPath = "ERR_ERR";
            strlcpy(config.mqttModelo, config.AppModelo, sizeof(config.mqttModelo));
            break;
        }
    }
    else if (strcmp(config.AppProducto, "ASTROLUM") == 0)
    {
        productoPath = "AST_LUM";
        strlcpy(config.mqttModelo, "M1", sizeof(config.mqttModelo));
    }
    else
    {
        productoPath = "ERR_ERR";
        strlcpy(config.mqttModelo, config.AppModelo, sizeof(config.mqttModelo));
    }

    // Calcular mqttMac10 = mqttModelo + últimos 8 dígitos de mqttMac
    const char *last8 = config.mqttMac;
    size_t mqttMacLen = strlen(config.mqttMac);
    if (mqttMacLen > 8)
    {
        last8 = &config.mqttMac[mqttMacLen - 8];
    }
    snprintf(config.mqttMac10, sizeof(config.mqttMac10), "%s%s", config.mqttModelo, last8);
    snprintf(config.mqttTopic, sizeof(config.mqttTopic), "IHD/%s/%s/%s/", productoPath, config.mqttMac6, config.mqttMac10);
}

void calcularMqttMacs(Config &config)
{
    char mqttMacLocal[13];
    size_t out = 0;
    const size_t maxOut = sizeof(mqttMacLocal) - 1;

    for (size_t i = 0; config.macSTA[i] != '\0' && out < maxOut; i++)
    {
        char ch = config.macSTA[i];
        if (ch == ':')
        {
            continue;
        }

        if (isxdigit(static_cast<unsigned char>(ch)))
        {
            mqttMacLocal[out++] = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
        }
    }
    mqttMacLocal[out] = '\0';

    strlcpy(config.mqttMac, mqttMacLocal, sizeof(config.mqttMac));

    const char *last5 = mqttMacLocal;
    if (out > 5)
    {
        last5 = &mqttMacLocal[out - 5];
    }
    snprintf(config.mqttMac6, sizeof(config.mqttMac6), "0%s", last5);
}

// Inicializa la configuración con valores por defecto
void initDefaultConfig(Config &config)
{
    strlcpy(config.AppNombre, "Astronova", sizeof(config.AppNombre));
    strlcpy(config.AppBluid, __DATE__ " " __TIME__, sizeof(config.AppBluid));
    config.AppVersion = 1;
    strlcpy(config.AppProducto, "ANCPLUS", sizeof(config.AppProducto));
    strlcpy(config.AppModelo, "51", sizeof(config.AppModelo));
    strlcpy(config.mqttMac, "", sizeof(config.mqttMac));
    strlcpy(config.mqttMac6, "", sizeof(config.mqttMac6));
    strlcpy(config.mqttMac10, "", sizeof(config.mqttMac10));
    strlcpy(config.mqttModelo, "", sizeof(config.mqttModelo));
    strlcpy(config.mqttHost, "192.168.1.2", sizeof(config.mqttHost));
    strlcpy(config.mqttUsuario, "", sizeof(config.mqttUsuario));
    strlcpy(config.mqttPassword, "", sizeof(config.mqttPassword));
    strlcpy(config.mqttTopic, "", sizeof(config.mqttTopic));
    strlcpy(config.wifiHost, "production_orbis", sizeof(config.wifiHost));
    strlcpy(config.wifiPassword, "1ikj23D8KASuv!cw_dfl82u3F", sizeof(config.wifiPassword));
    strlcpy(config.wifiApPassword, "12345678", sizeof(config.wifiApPassword));
    strlcpy(config.wifiIp, "0.0.0.0", sizeof(config.wifiIp));
    strlcpy(config.httpHost, "192.168.1.2", sizeof(config.httpHost));
    config.httPort = 80;
    strlcpy(config.macSTA, "00:00:00:00:00:00", sizeof(config.macSTA));
    strlcpy(config.macAP, "00:00:00:00:00:00", sizeof(config.macAP));
    strlcpy(config.macBluetooth, "00:00:00:00:00:00", sizeof(config.macBluetooth));
    strlcpy(config.mqttVer0Cortex, "4.2.4.51", sizeof(config.mqttVer0Cortex));
    strlcpy(config.mqttVer1Cortex, "4.2.4.51", sizeof(config.mqttVer1Cortex));
    strlcpy(config.mqttVer0Esp32, "0.9.3_prod", sizeof(config.mqttVer0Esp32));
    strlcpy(config.mqttVer1Esp32, "0.9.3_prod", sizeof(config.mqttVer1Esp32));

    Serial.println("Configuracion por defecto inicializada");
}

void mostrarConfiguracionSerial(const Config &config)
{
    Serial.println("=== Configuracion ===");
    Serial.printf("AppNombre: %s\n", config.AppNombre);
    Serial.printf("AppBluid: %s\n", config.AppBluid);
    Serial.printf("AppVersion: %d\n", config.AppVersion);
    Serial.printf("AppProducto: %s\n", config.AppProducto);
    Serial.printf("AppModelo: %s\n", config.AppModelo);
    Serial.printf("mqttModelo: %s\n", config.mqttModelo);
    Serial.printf("mqttMac: %s\n", config.mqttMac);
    Serial.printf("mqttMac6: %s\n", config.mqttMac6);
    Serial.printf("mqttMac10: %s\n", config.mqttMac10);
    Serial.printf("mqttHost: %s\n", config.mqttHost);
    Serial.printf("mqttUsuario: %s\n", config.mqttUsuario);
    Serial.printf("mqttPassword: %s\n", config.mqttPassword);
    Serial.printf("mqttTopic: %s\n", config.mqttTopic);
    Serial.printf("wifiHost: %s\n", config.wifiHost);
    Serial.printf("wifiIp: %s\n", config.wifiIp);
    Serial.printf("wifiPassword: %s\n", config.wifiPassword);
    Serial.printf("wifiApPassword: %s\n", config.wifiApPassword);
    Serial.printf("httpHost: %s\n", config.httpHost);
    Serial.printf("httPort: %d\n", config.httPort);
    Serial.printf("macSTA: %s\n", config.macSTA);
    Serial.printf("macAP: %s\n", config.macAP);
    Serial.printf("macBluetooth: %s\n", config.macBluetooth);
    Serial.printf("mqttVer0Cortex: %s\n", config.mqttVer0Cortex);
    Serial.printf("mqttVer1Cortex: %s\n", config.mqttVer1Cortex);
    Serial.printf("mqttVer0Esp32: %s\n", config.mqttVer0Esp32);
    Serial.printf("mqttVer1Esp32: %s\n", config.mqttVer1Esp32);

    char fechaIso[21];
    obtenerFechaHoraEsp32Iso8601(fechaIso, sizeof(fechaIso));
    Serial.printf("Fecha/Hora ESP32: %s\n", fechaIso);
    Serial.println("=====================");
}

// Retorna la fecha/hora actual del ESP32 en formato ISO 8601 UTC: "1970-01-01T00:00:14Z"
void obtenerFechaHoraEsp32Iso8601(char *buf, size_t bufLen)
{
    if (buf == nullptr || bufLen == 0)
    {
        return;
    }

    time_t now = time(nullptr);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
    strftime(buf, bufLen, "%Y-%m-%dT%H:%M:%SZ", &tmNow);
}

void getIso8601DateTime(char *buf, size_t bufLen)
{
    obtenerFechaHoraEsp32Iso8601(buf, bufLen);
}

// Copia src en dst convirtiendo todos los caracteres a minusculas
void MayToMin(const char *src, char *dst, size_t dstLen)
{
    if (!src || !dst || dstLen == 0)
    {
        return;
    }
    size_t i = 0;
    for (; i < dstLen - 1 && src[i] != '\0'; i++)
    {
        dst[i] = static_cast<char>(tolower(static_cast<unsigned char>(src[i])));
    }
    dst[i] = '\0';
}
