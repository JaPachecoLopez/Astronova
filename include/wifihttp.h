#ifndef WIFIHTTP_H
#define WIFIHTTP_H

#include <Arduino.h>

#include "config.h"

struct HttpResult
{
    int statusCode;
    String message;
    size_t bytesDownloaded = 0;
    int expectedSize = -1;
    bool complete = false;
};

// Callback para reportar progreso de descarga: recibe porcentaje (0-100)
typedef void (*HttpProgressCallback)(int progressPercent);

// --- Cliente HTTP (saliente) ---
HttpResult httpGet(const String &host, int port, const String &path, uint32_t timeoutMs = 5000);
HttpResult httpPost(const String &host, int port, const String &path, const String &payload, const String &contentType = "application/json", uint32_t timeoutMs = 5000);
HttpResult httpDownload(const String &host, int port, const String &path, HttpProgressCallback progressCallback = nullptr, uint32_t timeoutMs = 5000);

// Sincroniza fecha/hora del ESP32 consultando http://httpHost:GetPort/GetFechaUnix.php
// El servidor debe devolver la fecha en formato yyyyMMddhhmmss.
bool syncEsp32DateTimeFromServer(const Config &config, uint32_t timeoutMs = 5000);

#endif
