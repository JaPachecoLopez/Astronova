#include "mqttclient.h"

#include "funciones.h"
#include "wifihttp.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static Config *mqttConfig = nullptr;

// Intervalo minimo entre reintentos de conexion (ms)
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;
static unsigned long lastRetryMs = 0;
static int lastHttpDownloadProgressPercent = -10;

static void onDownloadProgress(int progressPercent)
{
    if (progressPercent < 0)
    {
        progressPercent = 0;
    }
    if (progressPercent > 100)
    {
        progressPercent = 100;
    }

    int progressStep = (progressPercent / 10) * 10;
    if (progressPercent == 100)
    {
        progressStep = 100;
    }

    if (progressStep == lastHttpDownloadProgressPercent)
    {
        return;
    }

    lastHttpDownloadProgressPercent = progressStep;
    Serial.printf("[File Download] %d%%\n", progressStep);
}

static bool parseHttpUrl(const char *url, String &hostOut, int &portOut, String &pathOut)
{
    if (!url)
    {
        return false;
    }

    String u(url);
    if (!u.startsWith("http://"))
    {
        return false;
    }

    String remainder = u.substring(7); // sin "http://"
    int slashIdx = remainder.indexOf('/');
    String hostPort = (slashIdx >= 0) ? remainder.substring(0, slashIdx) : remainder;
    pathOut = (slashIdx >= 0) ? remainder.substring(slashIdx) : "/";

    if (hostPort.length() == 0)
    {
        return false;
    }

    int colonIdx = hostPort.indexOf(':');
    if (colonIdx >= 0)
    {
        hostOut = hostPort.substring(0, colonIdx);
        String portStr = hostPort.substring(colonIdx + 1);
        if (hostOut.length() == 0 || portStr.length() == 0)
        {
            return false;
        }
        portOut = portStr.toInt();
        if (portOut <= 0)
        {
            return false;
        }
    }
    else
    {
        hostOut = hostPort;
        portOut = 80;
    }

    return true;
}

static bool extractModelAndVersionFromUrl(const char *url, char *modelOut, size_t modelOutLen, char *versionOut, size_t versionOutLen)
{
    if (!url || !modelOut || !versionOut || modelOutLen == 0 || versionOutLen == 0)
    {
        return false;
    }

    modelOut[0] = '\0';
    versionOut[0] = '\0';

    const char *fileName = strrchr(url, '/');
    fileName = (fileName != nullptr) ? (fileName + 1) : url;

    const char *prefix = "ANC_COG_";
    const size_t prefixLen = strlen(prefix);
    if (strncmp(fileName, prefix, prefixLen) != 0)
    {
        return false;
    }

    const char *modelStart = fileName + prefixLen;
    const char *versionMarker = strstr(modelStart, "_v");
    if (!versionMarker)
    {
        return false;
    }

    size_t modelLen = static_cast<size_t>(versionMarker - modelStart);
    if (modelLen == 0 || modelLen >= modelOutLen)
    {
        return false;
    }

    const char *versionStart = versionMarker + 2; // salta "_v"
    if (*versionStart == '.')
    {
        versionStart++; // tolera formato "_v.4.2.2"
    }

    const char *binExt = strstr(versionStart, ".bin");
    if (!binExt || strcmp(binExt, ".bin") != 0)
    {
        return false;
    }

    size_t versionLen = static_cast<size_t>(binExt - versionStart);
    if (versionLen == 0 || versionLen >= versionOutLen)
    {
        return false;
    }

    memcpy(modelOut, modelStart, modelLen);
    modelOut[modelLen] = '\0';

    memcpy(versionOut, versionStart, versionLen);
    versionOut[versionLen] = '\0';

    return true;
}

static void cortexUpdate(const String &payloadJson)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadJson);
    if (error)
    {
        Serial.print("[MQTT] cortexUpdate JSON invalido: ");
        Serial.println(error.c_str());
        return;
    }

    const char *url = doc["data"]["url"] | "";
    if (url[0] == '\0')
    {
        Serial.println("[MQTT] cortexUpdate sin data.url");
        return;
    }

    char modelo[16];
    char version[24];
    if (!extractModelAndVersionFromUrl(url, modelo, sizeof(modelo), version, sizeof(version)))
    {
        Serial.print("[MQTT] cortexUpdate URL no valida: ");
        Serial.println(url);
        return;
    }

    Serial.print("[MQTT] cortexUpdate modelo=");
    Serial.print(modelo);
    Serial.print(" version=");
    Serial.println(version);

    String host;
    int port = 80;
    String path;
    if (!parseHttpUrl(url, host, port, path))
    {
        Serial.print("[MQTT] cortexUpdate URL no soportada para download: ");
        Serial.println(url);
        return;
    }

    lastHttpDownloadProgressPercent = -10;
    HttpResult downloadResult = httpDownload(host, port, path, onDownloadProgress);
    Serial.print("[MQTT] cortexUpdate download status=");
    Serial.println(downloadResult.statusCode);
    if (downloadResult.statusCode == 200)
    {
        Serial.print("[MQTT] cortexUpdate download completado, bytes=");
        Serial.println(downloadResult.bytesDownloaded);

        if (downloadResult.expectedSize >= 0)
        {
            Serial.print("[MQTT] cortexUpdate tamaño esperado=");
            Serial.println(downloadResult.expectedSize);

            if (!downloadResult.complete)
            {
                Serial.print("[MQTT] cortexUpdate descarga incompleta: esperados ");
                Serial.print(downloadResult.expectedSize);
                Serial.print(", recibidos ");
                Serial.println(downloadResult.bytesDownloaded);
            }
        }
    }
    else
    {
        Serial.print("[MQTT] cortexUpdate download error: ");
        Serial.println(downloadResult.message);
    }
}

static void routeTopicRemainder(const char *topicRemainder, const byte *payload, unsigned int length)
{
    if (!topicRemainder)
    {
        return;
    }

    // "Switch" de string con comparaciones; aqui se iran anadiendo acciones reales.
    if (strcmp(topicRemainder, "set/start/cortex_fw_update") == 0)
    {
        Serial.println("[MQTT] Ruta: set/start/cortex_fw_update");

        String payloadJson;
        payloadJson.reserve(length + 1);
        for (unsigned int i = 0; i < length; i++)
        {
            payloadJson += static_cast<char>(payload[i]);
        }

        cortexUpdate(payloadJson);
    }
    else if (strcmp(topicRemainder, "notification/status/calendar") == 0)
    {
        Serial.println("[MQTT] Ruta: notification/status/calendar");
    }
    else if (strcmp(topicRemainder, "notification/status/onOff") == 0)
    {
        Serial.println("[MQTT] Ruta: notification/status/onOff");
    }
    else
    {
        Serial.print("[MQTT] Ruta no implementada: ");
        Serial.println(topicRemainder);
    }
}

bool SendOnLine()
{
    if (!mqttConfig)
    {
        return false;
    }

    char topic[96];
    char payload[256];
    char timestamp[21];

    const char *mqttMac10 = mqttConfig->mqttMac10;
    const char *mqttVer0Esp32 = mqttConfig->mqttVer0Esp32;
    const char *mqttVer0Cortex = mqttConfig->mqttVer0Cortex;
    char macSTA[18];
    MayToMin(mqttConfig->macSTA, macSTA, sizeof(macSTA));
    obtenerFechaHoraEsp32Iso8601(timestamp, sizeof(timestamp));

    snprintf(topic, sizeof(topic), "%snotification/online/system", mqttConfig->mqttTopic);
    snprintf(payload, sizeof(payload),
             "{\"header\":{\"timestamp\":\"%s\",\"uid\":69},\"data\":{\"session_id\":\"%s\",\"cortex\":{\"fw_version\":\"%s\"},\"fw_version\":\"%s\",\"device_mac\":\"%s\"}}",
             timestamp,
             mqttMac10,
             mqttVer0Cortex,
             mqttVer0Esp32,
             macSTA);
    Serial.printf("[MQTT] SendOnLine topic(%d): %s\n", strlen(topic), topic);
    Serial.printf("[MQTT] SendOnLine payload(%d): %s\n", strlen(payload), payload);

    bool published = mqttPublish(topic, payload);
    if (!published)
    {
        Serial.println("[MQTT] SendOnLine fallido");
    }

    return published;
}

static void onMessageReceived(char *topic, byte *payload, unsigned int length)
{
    if (!mqttConfig || !topic)
    {
        return;
    }

    const char *prefix = mqttConfig->mqttTopic;
    size_t prefixLen = strlen(prefix);
    if (prefixLen == 0 || strncmp(topic, prefix, prefixLen) != 0)
    {
        Serial.print("[MQTT] Mensaje ignorado, topic fuera de mqttTopic: ");
        Serial.println(topic);
        return;
    }

    const char *topicRemainder = topic + prefixLen;

    Serial.print("[MQTT] Mensaje en topic: ");
    Serial.print(topic);
    Serial.print(" | resto: ");
    Serial.print(topicRemainder);
    Serial.print(" -> ");
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print(static_cast<char>(payload[i]));
    }
    Serial.println();

    routeTopicRemainder(topicRemainder, payload, length);
}

static bool mqttConnect()
{
    if (!mqttConfig)
    {
        return false;
    }

    Serial.print("[MQTT] Conectando a ");
    Serial.print(mqttConfig->mqttHost);
    Serial.print(" como ");
    Serial.print(mqttConfig->mqttMac);
    Serial.print("...");

    bool connected;
    if (strlen(mqttConfig->mqttUsuario) > 0)
    {
        connected = mqttClient.connect(
            mqttConfig->mqttMac,
            mqttConfig->mqttUsuario,
            mqttConfig->mqttPassword);
    }
    else
    {
        connected = mqttClient.connect(mqttConfig->mqttMac);
    }

    if (connected)
    {
        Serial.println(" OK");
        LedParpadea(2, 50, 50);

        char subscribeTopic[24];
        snprintf(subscribeTopic, sizeof(subscribeTopic), "IHD/+/%s/#", mqttConfig->mqttMac6);
        if (mqttClient.subscribe(subscribeTopic))
        {
            Serial.print("[MQTT] Suscrito a ");
            Serial.println(subscribeTopic);
        }
        else
        {
            Serial.print("[MQTT] Error al suscribirse a ");
            Serial.println(subscribeTopic);
        }

        SendOnLine();
    }
    else
    {
        Serial.print(" ERROR estado=");
        Serial.println(mqttClient.state());
    }

    return connected;
}

void mqttBegin(Config &config)
{
    mqttConfig = &config;
    mqttClient.setServer(config.mqttHost, 1883);
    mqttClient.setCallback(onMessageReceived);
    mqttClient.setKeepAlive(60);
    mqttClient.setBufferSize(512);
    mqttConnect();
}

void mqttHandle()
{
    if (!mqttConfig)
    {
        return;
    }

    if (mqttClient.connected())
    {
        mqttClient.loop();
        return;
    }

    // Reintento con intervalo
    unsigned long now = millis();
    if (now - lastRetryMs < MQTT_RETRY_INTERVAL_MS)
    {
        return;
    }
    lastRetryMs = now;

    mqttReconnectNow();
}

bool mqttReconnectNow()
{
    if (!mqttConfig)
    {
        return false;
    }

    if (mqttClient.connected())
    {
        return true;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        lastRetryMs = millis();
        return mqttConnect();
    }

    return false;
}

bool mqttPublish(const char *topic, const char *payload, bool retained)
{
    if (!mqttClient.connected())
    {
        Serial.println("[MQTT] Publish fallido: no conectado");
        return false;
    }
    return mqttClient.publish(topic, payload, retained);
}

bool mqttIsConnected()
{
    return mqttClient.connected();
}
