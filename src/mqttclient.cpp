#include "mqttclient.h"

#include "funciones.h"
#include "wifihttp.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

bool SendTopic(const char *topic, const char *payload, unsigned int topicLen, unsigned int payloadLen);

static Config *mqttConfig = nullptr;

// Intervalo minimo entre reintentos de conexion (ms)
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;
static unsigned long lastRetryMs = 0;
static int lastHttpDownloadProgressPercent = -10;
static unsigned long idTransNum = 1000 + (esp_random() % 2001); // Valor inicial aleatorio idTrans dispositivos.
static String lastCalendarDataJson;

enum class repPet : uint8_t
{
    CortexUpdate12 = 0,
    CortexUpdateStart = 1,
    CortexUpdateComplete = 2,
    Sincro12 = 3,
    SincroOk = 4,
    Fecha12 = 5,
    FechaOk = 6
};

static unsigned long nextIdTrans()
{
    return ++idTransNum;
}

static void waitMillisecondsNonBlocking(unsigned long waitMs)
{
    const unsigned long startMs = millis();
    while (millis() - startMs < waitMs)
    {
        if (mqttClient.connected())
        {
            mqttClient.loop();
        }
        yield();
    }
}

static bool sendResponseByType(repPet responseType, int32_t idTrans = 0, const String &dataJson = "")
{
    const char *responseTopic = nullptr;
    char fullTopic[160];
    char responseTimestamp[21];
    char responsePayload[160];
    int responsePayloadLen = -1;
    int fullTopicLen = -1;

    if (!mqttConfig)
    {
        Serial.println("[MQTT] sendResponseByType error: mqttConfig no disponible");
        return false;
    }

    obtenerFechaHoraEsp32Iso8601(responseTimestamp, sizeof(responseTimestamp));
    Serial.printf("[MQTT] Enviando: %d\n", static_cast<int>(responseType));
    switch (responseType)
    {
    case repPet::CortexUpdate12: // cortex_fw_update 12
        responseTopic = "response/start/cortex_fw_update";
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"idTrans\":%lu,\"header\":{\"timestamp\":\"%s\",\"status\":12,\"uid\":77}}", nextIdTrans(), responseTimestamp);
        break;
    case repPet::CortexUpdateStart: // notification/started/cortex_fw_update
        // {"header":{"timestamp":"1970-01-01T00:00:14Z","uid":77},"data":{"event":"started","message":"Download started"}}
        responseTopic = "notification/started/cortex_fw_update";
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"header\":{\"timestamp\":\"%s\",\"uid\":77},\"data\":{\"event\":\"started\",\"message\":\"Download started\"}}",
                                      responseTimestamp);
        break;
    case repPet::CortexUpdateComplete: // notification/started/cortex_fw_update
        // {"header":{"timestamp":"1970-01-01T00:00:14Z","uid":77},"data":{"event":"started","message":"Download completed"}}
        responseTopic = "notification/update_done/cortex_fw_update";
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"header\":{\"timestamp\":\"%s\",\"uid\":77},\"data\":{\"event\":\"completed\",\"message\":\"Download completed successfully\"}}",
                                      responseTimestamp);
        break;

    case repPet::Sincro12:
        //{"idTrans":9232764,"header":{"timestamp":"2026-03-16T08:26:04Z","status":12,"uid":70}}
        responseTopic = "response/request_sync/anc_device";
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"idTrans\":%lu,\"header\":{\"timestamp\":\"%s\",\"status\":12,\"uid\":70}}", idTrans, responseTimestamp);
        break;
    case repPet::SincroOk:
        //{"header":{"timestamp":"2026-03-16T08:26:07Z","uid":70},"data":{"language":0,"country":0,"city":0,"contrast":4,"permanent":0,"pwdEnDis":0,"zoneEnabled":0,"mode1224":0}}
        responseTopic = "notification/cfg/device";
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"header\":{\"timestamp\":\"%s\",\"uid\":70},\"data\":{\"language\":0,\"country\":0,\"city\":0,\"contrast\":4}}",
                                      responseTimestamp);
        break;

    case repPet::Fecha12:                        // Fecha
        responseTopic = "response/cfg/calendar"; //  {"idTrans":9232765,"header":{"timestamp":"2026-03-16T08:26:17Z","status":12,"uid":75}}
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"idTrans\":%lu,\"header\":{\"timestamp\":\"%s\",\"status\":12,\"uid\":75}}", idTrans, responseTimestamp);
        break;
    case repPet::FechaOk:                               // Estado de calendario
        responseTopic = "notification/status/calendar"; // {"header":{"timestamp":"2026-03-16T08:26:17Z","uid":75},"data":{"calendar":[9,25,56,64,16,3,26]}}
        responsePayloadLen = snprintf(responsePayload, sizeof(responsePayload),
                                      "{\"header\":{\"timestamp\":\"%s\",\"uid\":75},\"data\":%s}", responseTimestamp, dataJson.c_str());
        break;
    default:
        Serial.printf("[MQTT] sendResponseByType tipo no soportado: %d\n", static_cast<int>(responseType));
        return false;
    }
    if (responsePayloadLen < 0 || responsePayloadLen >= static_cast<int>(sizeof(responsePayload)))
    {
        Serial.println("[MQTT] sendResponseByType error: payload truncado");
        return false;
    }

    fullTopicLen = snprintf(fullTopic, sizeof(fullTopic), "%s%s", mqttConfig->mqttTopic, responseTopic);
    if (fullTopicLen < 0 || fullTopicLen >= static_cast<int>(sizeof(fullTopic)))
    {
        Serial.println("[MQTT] sendResponseByType error: topic truncado");
        return false;
    }

    return SendTopic(fullTopic, responsePayload, fullTopicLen, responsePayloadLen);
}

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

static bool extractDataAsString(const String &payloadJson, String &dataOut)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadJson);
    if (error)
    {
        Serial.print("[MQTT] extractDataAsString JSON invalido: ");
        Serial.println(error.c_str());
        return false;
    }

    if (doc["data"].isNull())
    {
        Serial.println("[MQTT] extractDataAsString sin campo data");
        return false;
    }

    dataOut = "";
    serializeJson(doc["data"], dataOut);
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
static void fechaSet(const String &payloadJson)
{
    // formato trama esperado:
    // {"idTrans": 9232765,"data":{"calendar":[9,25,56,64,16,3,26]}}
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadJson);
    if (error)
    {
        Serial.print("[MQTT] fechaSet JSON invalido: ");
        Serial.println(error.c_str());
        return;
    }
    const int32_t idTrans = doc["idTrans"] | 0;
    if (idTrans == 0)
    {
        Serial.println("[MQTT] fechaSet sin idTrans");
        return;
    }
    Serial.print("[MQTT] fechaSet idTrans=");
    Serial.println(idTrans);

    if (!extractDataAsString(payloadJson, lastCalendarDataJson))
    {
        return;
    }
    Serial.print("[MQTT] fechaSet data=");
    Serial.println(lastCalendarDataJson);

    if (!sendResponseByType(repPet::Fecha12, idTrans, lastCalendarDataJson)) // response/cfg/calendar
    {
        return;
    }
    waitMillisecondsNonBlocking(2000);
    if (!sendResponseByType(repPet::FechaOk, idTrans, lastCalendarDataJson)) // notification/status/calendar
    {
        return;
    }
}

static void cortexSincro(const String &payloadJson)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadJson);
    if (error)
    {
        Serial.print("[MQTT] cortexSincro JSON invalido: ");
        Serial.println(error.c_str());
        return;
    }
    const int32_t idTrans = doc["idTrans"] | 0;
    if (idTrans == 0)
    {
        Serial.println("[MQTT] cortexSincro sin idTrans");
        return;
    }
    Serial.print("[MQTT] cortexSincro idTrans=");
    Serial.println(idTrans);
    if (!sendResponseByType(repPet::Sincro12, idTrans)) // response/request_sync/anc_device
    {
        return;
    }
    waitMillisecondsNonBlocking(2050);
    if (!sendResponseByType(repPet::SincroOk, idTrans)) // notification/cfg/device
    {
        return;
    }
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

    if (!sendResponseByType(repPet::CortexUpdate12)) // response/start/cortex_fw_update
    {
        return;
    }
    waitMillisecondsNonBlocking(2050);

    if (!sendResponseByType(repPet::CortexUpdateStart)) // notification/started/cortex_fw_update
    {
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

            if (downloadResult.complete)
            {
                Serial.println("[MQTT] cortexUpdate descarga completa y verificada");
                waitMillisecondsNonBlocking(1000);
                if (!sendResponseByType(repPet::CortexUpdateComplete)) // notification/update_done/cortex_fw_update
                {
                    return;
                }
                Serial.println("[MQTT] Realizando pausa simulada deactualizacion Cortex...");
                waitMillisecondsNonBlocking(1000);

                // Asignar la version del Cortex a la config, para que se refleje en el proximo SendOnLine.
                snprintf(mqttConfig->mqttVer0Cortex, sizeof(mqttConfig->mqttVer0Cortex), "%s.%s", version, modelo);
                Serial.print("[MQTT] mqttVer0Cortex actualizado a: ");
                Serial.println(mqttConfig->mqttVer0Cortex);

                SendOnLine(); // Simula que el dispositivo se reinicia y vuelve a conectarse online tras la actualizacion Cortex.
            }
            else
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
    else if (strcmp(topicRemainder, "set/request_sync/device") == 0)
    {
        Serial.println("[MQTT] Ruta: set/request_sync/device");
        String payloadJson;
        payloadJson.reserve(length + 1);
        for (unsigned int i = 0; i < length; i++)
        {
            payloadJson += static_cast<char>(payload[i]);
        }

        cortexSincro(payloadJson);
    }
    else if (strcmp(topicRemainder, "set/cfg/calendar") == 0)
    {
        Serial.println("[MQTT] Ruta: set/cfg/calendar");
        String payloadJson;
        payloadJson.reserve(length + 1);
        for (unsigned int i = 0; i < length; i++)
        {
            payloadJson += static_cast<char>(payload[i]);
        }

        fechaSet(payloadJson);
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
bool SendTopic(const char *topic, const char *payload, unsigned int topicLen, unsigned int payloadLen)
{
    if (!mqttConfig)
        return false;

    Serial.printf("[MQTT] SendTopic  (%u): %s\n", static_cast<unsigned>(topicLen), topic);
    Serial.printf("[MQTT] SendPayload(%u): %s\n", static_cast<unsigned>(payloadLen), payload);
    bool published = mqttPublish(topic, payload);
    if (!published)
    {
        Serial.println("[MQTT] SendTopic fallido");
    }
    return published;
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

    int topicLen = snprintf(topic, sizeof(topic), "%snotification/online/system", mqttConfig->mqttTopic);
    int payloadLen = snprintf(payload, sizeof(payload),
                              "{\"header\":{\"timestamp\":\"%s\",\"uid\":69},\"data\":{\"session_id\":\"%s\",\"cortex\":{\"fw_version\":\"%s\"},\"fw_version\":\"%s\",\"device_mac\":\"%s\",\"appv\":\"%d\",\"ip\":\"%s\"}}",
                              timestamp,
                              mqttMac10,
                              mqttVer0Cortex,
                              mqttVer0Esp32,
                              macSTA,
                              mqttConfig->AppVersion,
                              mqttConfig->wifiIp);

    if (topicLen < 0 || topicLen >= static_cast<int>(sizeof(topic)))
    {
        Serial.println("[MQTT] SendOnLine error: topic truncado");
        return false;
    }

    if (payloadLen < 0 || payloadLen >= static_cast<int>(sizeof(payload)))
    {
        Serial.println("[MQTT] SendOnLine error: payload truncado");
        return false;
    }

    Serial.printf("[MQTT] SendOnLine topic(%u): %s\n", static_cast<unsigned>(topicLen), topic);
    Serial.printf("[MQTT] SendOnLine payload(%u): %s\n", static_cast<unsigned>(payloadLen), payload);

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
