#include "wificfg.h"

#include "mqttclient.h"
#include "wifihttp.h"

#include <Arduino.h>

void setStatusLed(bool on)
{
#if LED_ACTIVE_LOW
    digitalWrite(LED_PIN, on ? LOW : HIGH);
#else
    digitalWrite(LED_PIN, on ? HIGH : LOW);
#endif
}

bool isWiFiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

void addToMac(uint8_t *mac, uint64_t value)
{
    uint64_t macValue = 0;

    // Convertir array de 6 bytes a numero de 48 bits
    for (int i = 0; i < 6; i++)
    {
        macValue = (macValue << 8) | mac[i];
    }

    // Sumar el valor
    macValue += value;

    // Convertir de vuelta a array de bytes
    for (int i = 5; i >= 0; i--)
    {
        mac[i] = macValue & 0xFF;
        macValue >>= 8;
    }
}

void wifiGetMACAddresses(Config &config)
{
    uint8_t macSTA[6];
    uint8_t macAP[6];
    uint8_t macBT[6];

    // Inicializar WiFi
    WiFi.mode(WIFI_STA);

    // Obtener MAC de WiFi STA
    WiFi.macAddress(macSTA);

    // Calcular MAC de AP (STA + 1)
    memcpy(macAP, macSTA, 6);
    addToMac(macAP, 1);

    // Calcular MAC de Bluetooth (STA + 2)
    memcpy(macBT, macSTA, 6);
    addToMac(macBT, 2);

    // Formatear como strings "XX:XX:XX:XX:XX:XX"
    snprintf(config.macSTA, sizeof(config.macSTA),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             macSTA[0], macSTA[1], macSTA[2], macSTA[3], macSTA[4], macSTA[5]);

    snprintf(config.macAP, sizeof(config.macAP),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             macAP[0], macAP[1], macAP[2], macAP[3], macAP[4], macAP[5]);

    snprintf(config.macBluetooth, sizeof(config.macBluetooth),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             macBT[0], macBT[1], macBT[2], macBT[3], macBT[4], macBT[5]);

    // Mostrar las MACs en Serial
    Serial.println("=== MAC Addresses ===");
    Serial.print("WiFi STA:   ");
    Serial.println(config.macSTA);
    Serial.print("WiFi AP:    ");
    Serial.println(config.macAP);
    Serial.print("Bluetooth:  ");
    Serial.println(config.macBluetooth);
}

void conectarWiFiAPySTA(Config &config)
{
    char apSSID[64];
    const char *apPassword = config.wifiApPassword;
    // Nombre de la Wifi AP del Esp32
    snprintf(apSSID, sizeof(apSSID), "%.3s_%s", config.AppNombre, config.mqttMac); // 3 primeros caracteres del nombre + mac limpia

    WiFi.mode(WIFI_AP_STA);

    bool apStarted = false;
    if (strlen(apPassword) < 8)
    {
        apPassword = "12345678";
        Serial.println("Aviso: wifiApPassword invalida (<8). Se usa password por defecto segura.");
    }

    if (strlen(apPassword) >= 8)
    {
        apStarted = WiFi.softAP(apSSID, apPassword);
    }
    else
    {
        apStarted = WiFi.softAP(apSSID);
    }

    if (apStarted)
    {
        Serial.println("AP iniciado");
        Serial.print("AP SSID: ");
        Serial.println(apSSID);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        Serial.println("Error: No se pudo iniciar el punto de acceso");
    }

    Serial.print("Conectando STA a SSID: ");
    Serial.println(config.wifiHost);
    WiFi.begin(config.wifiHost, config.wifiPassword);

    const unsigned long timeoutMs = 5000;
    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < timeoutMs)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        String ipStr = WiFi.localIP().toString();
        strlcpy(config.wifiIp, ipStr.c_str(), sizeof(config.wifiIp));
        Serial.println("STA conectado");
        Serial.print("STA IP: ");
        Serial.println(config.wifiIp);
        setStatusLed(true);
    }
    else
    {
        strlcpy(config.wifiIp, "0.0.0.0", sizeof(config.wifiIp));
        Serial.println("Error: STA no se pudo conectar (timeout)");
        setStatusLed(false);
    }
}

void verificarYReconectarWiFi(Config &config)
{
    static bool initialized = false;
    static bool wasConnected = false;
    static unsigned long lastCheckMs = 0;
    static unsigned long lastRetryMs = 0;
    static unsigned long retryIntervalMs = 0;

    const unsigned long checkIntervalMs = 5000;
    const unsigned long minRetryIntervalMs = 3000;
    const unsigned long maxRetryIntervalMs = 10000;

    if (millis() - lastCheckMs < checkIntervalMs)
    {
        return;
    }
    lastCheckMs = millis();

    if (!initialized)
    {
        wasConnected = (WiFi.status() == WL_CONNECTED);
        initialized = true;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        String ipStr = WiFi.localIP().toString();
        strlcpy(config.wifiIp, ipStr.c_str(), sizeof(config.wifiIp));
        setStatusLed(true);
        retryIntervalMs = 0;

        if (!wasConnected)
        {
            Serial.println("WiFi STA recuperado. Sincronizando hora y MQTT...");

            if (!syncEsp32DateTimeFromServer(config))
            {
                Serial.println("Aviso: no se pudo sincronizar la fecha tras reconexion WiFi");
            }

            if (!mqttReconnectNow())
            {
                Serial.println("Aviso: no se pudo reconectar MQTT tras reconexion WiFi");
            }
        }

        wasConnected = true;
        return;
    }

    wasConnected = false;
    setStatusLed(false);
    strlcpy(config.wifiIp, "0.0.0.0", sizeof(config.wifiIp));

    if (retryIntervalMs == 0)
    {
        retryIntervalMs = random(minRetryIntervalMs, maxRetryIntervalMs + 1);
    }

    if (millis() - lastRetryMs < retryIntervalMs)
    {
        return;
    }
    lastRetryMs = millis();

    Serial.println("WiFi STA desconectado. Reintentando conexion...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false, false);
    WiFi.begin(config.wifiHost, config.wifiPassword);
    // No esperamos: el siguiente ciclo del loop comprobará el resultado.
    retryIntervalMs = random(minRetryIntervalMs, maxRetryIntervalMs + 1);
}
