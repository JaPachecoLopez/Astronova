#include "wifihttp.h"

#include "funciones.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

static const char *HTTP_USER_AGENT = "AstronovaESP32/1.0";

static HttpResult makeErrorResult(int code, const String &prefix)
{
    HttpResult result;
    result.statusCode = code;
    result.message = prefix + HTTPClient::errorToString(code).c_str();
    return result;
}

static void configureHttpClient(HTTPClient &http, uint32_t timeoutMs)
{
    http.setTimeout(timeoutMs);
    http.setUserAgent(HTTP_USER_AGENT);
}

static HttpResult downloadHttpBody(HTTPClient &http, HttpProgressCallback progressCallback)
{
    HttpResult result;
    result.statusCode = http.GET();
    if (result.statusCode <= 0)
    {
        result = makeErrorResult(result.statusCode, "DOWNLOAD error: ");
        return result;
    }

    result.expectedSize = http.getSize();

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[512];
    int lastProgress = -1;

    if (progressCallback)
    {
        progressCallback(0);
    }

    while (http.connected() && (result.expectedSize < 0 || result.bytesDownloaded < static_cast<size_t>(result.expectedSize)))
    {
        size_t availableBytes = stream->available();
        if (availableBytes == 0)
        {
            delay(1);
            continue;
        }

        size_t toRead = availableBytes;
        if (toRead > sizeof(buffer))
        {
            toRead = sizeof(buffer);
        }

        int len = stream->readBytes(reinterpret_cast<char *>(buffer), toRead);
        if (len <= 0)
        {
            delay(1);
            continue;
        }

        result.bytesDownloaded += static_cast<size_t>(len);

        if (progressCallback && result.expectedSize > 0)
        {
            int progress = static_cast<int>((result.bytesDownloaded * 100U) / static_cast<size_t>(result.expectedSize));
            if (progress > 100)
            {
                progress = 100;
            }

            if (progress != lastProgress)
            {
                progressCallback(progress);
                lastProgress = progress;
            }
        }
    }

    if (result.expectedSize >= 0)
    {
        result.complete = (result.bytesDownloaded == static_cast<size_t>(result.expectedSize));
    }
    else
    {
        result.complete = true;
    }

    if (progressCallback && result.complete)
    {
        progressCallback(100);
    }

    return result;
}

static bool parseServerDateTime(const String &rawValue, struct tm &tmOut)
{
    String value = rawValue;
    value.trim();

    // Busca una secuencia de exactamente 12 digitos (yyMMddhhmmss)
    int startIndex = -1;
    for (int i = 0; i <= static_cast<int>(value.length()) - 12; i++)
    {
        bool allDigits = true;
        for (int j = 0; j < 12; j++)
        {
            if (!isDigit(value[i + j]))
            {
                allDigits = false;
                break;
            }
        }
        if (allDigits)
        {
            startIndex = i;
            break;
        }
    }

    if (startIndex < 0)
    {
        return false;
    }

    String s = value.substring(startIndex, startIndex + 12);

    int year = 2000 + s.substring(0, 2).toInt();
    int month = s.substring(2, 4).toInt();
    int day = s.substring(4, 6).toInt();
    int hour = s.substring(6, 8).toInt();
    int minute = s.substring(8, 10).toInt();
    int second = s.substring(10, 12).toInt();

    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
    {
        return false;
    }

    memset(&tmOut, 0, sizeof(tmOut));
    tmOut.tm_year = year - 1900;
    tmOut.tm_mon = month - 1;
    tmOut.tm_mday = day;
    tmOut.tm_hour = hour;
    tmOut.tm_min = minute;
    tmOut.tm_sec = second;

    return true;
}

HttpResult httpGet(const String &host, int port, const String &path, uint32_t timeoutMs)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        HttpResult result;
        result.statusCode = -1;
        result.message = "WiFi no conectado";
        return result;
    }

    LedParpadea(1, 80, 80);

    String url = (port == 80)
                     ? "http://" + host + path
                     : "http://" + host + ":" + String(port) + path;
    Serial.print("[httpGet] URL: ");
    Serial.println(url);

    HTTPClient http;
    if (!http.begin(url))
    {
        HttpResult result;
        result.statusCode = -2;
        result.message = "URL no valida o begin fallo";
        return result;
    }

    configureHttpClient(http, timeoutMs);

    int httpCode = http.GET();
    if (httpCode > 0)
    {
        HttpResult result;
        result.statusCode = httpCode;
        result.message = http.getString();
        http.end();
        return result;
    }

    HttpResult result = makeErrorResult(httpCode, "GET error: ");
    http.end();
    return result;
}

HttpResult httpPost(const String &host, int port, const String &path, const String &payload, const String &contentType, uint32_t timeoutMs)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        HttpResult result;
        result.statusCode = -1;
        result.message = "WiFi no conectado";
        return result;
    }

    LedParpadea(1, 80, 80);

    String url = (port == 80)
                     ? "http://" + host + path
                     : "http://" + host + ":" + String(port) + path;
    Serial.print("[httpPost] URL: ");
    Serial.println(url);

    HTTPClient http;
    if (!http.begin(url))
    {
        HttpResult result;
        result.statusCode = -2;
        result.message = "URL no valida o begin fallo";
        return result;
    }

    configureHttpClient(http, timeoutMs);
    http.addHeader("Content-Type", contentType);

    int httpCode = http.POST(payload);
    if (httpCode > 0)
    {
        HttpResult result;
        result.statusCode = httpCode;
        result.message = http.getString();
        http.end();
        return result;
    }

    HttpResult result = makeErrorResult(httpCode, "POST error: ");
    http.end();
    return result;
}

HttpResult httpDownload(const String &host, int port, const String &path, HttpProgressCallback progressCallback, uint32_t timeoutMs)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        HttpResult result;
        result.statusCode = -1;
        result.message = "WiFi no conectado";
        return result;
    }

    LedParpadea(1, 80, 80);

    String url = (port == 80)
                     ? "http://" + host + path
                     : "http://" + host + ":" + String(port) + path;
    Serial.print("[httpDownload] URL: ");
    Serial.println(url);

    HTTPClient http;
    if (!http.begin(url))
    {
        HttpResult result;
        result.statusCode = -2;
        result.message = "URL no valida o begin fallo";
        return result;
    }

    configureHttpClient(http, timeoutMs);

    HttpResult result = downloadHttpBody(http, progressCallback);
    http.end();
    return result;
}

bool syncEsp32DateTimeFromServer(const Config &config, uint32_t timeoutMs)
{
    HttpResult result = httpGet(String(config.httpHost), config.httPort, "/GetFechaUnix.php", timeoutMs);
    if (result.statusCode != 200)
    {
        Serial.print("Error sincronizando fecha (HTTP): ");
        Serial.println(result.message);
        return false;
    }

    struct tm tmServer;
    if (!parseServerDateTime(result.message, tmServer))
    {
        Serial.print("Error sincronizando fecha: formato invalido: ");
        Serial.println(result.message);
        return false;
    }

    time_t epoch = mktime(&tmServer);
    if (epoch <= 0)
    {
        Serial.println("Error sincronizando fecha: no se pudo convertir a epoch");
        return false;
    }

    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;

    if (settimeofday(&tv, nullptr) != 0)
    {
        Serial.println("Error sincronizando fecha: settimeofday fallo");
        return false;
    }

    Serial.print("Fecha sincronizada con servidor: ");
    Serial.println(result.message);
    return true;
}
