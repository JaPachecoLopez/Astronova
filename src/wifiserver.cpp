#include "wifiserver.h"

#include "config_json.h"
#include "funciones.h"
#include "wifihttp.h"

#include <Arduino.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

static WebServer webServer(80);
static Config *webServerConfig = nullptr;

static String escapeJsonString(const String &value)
{
    String escaped;
    escaped.reserve(value.length() + 8);

    for (size_t i = 0; i < value.length(); i++)
    {
        char ch = value[i];
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }

    return escaped;
}

static String buildQueryArgsJson()
{
    String json = "{";
    for (int i = 0; i < webServer.args(); i++)
    {
        if (i > 0)
        {
            json += ",";
        }

        json += "\"" + escapeJsonString(webServer.argName(i)) + "\":\"" +
                escapeJsonString(webServer.arg(i)) + "\"";
    }
    json += "}";
    return json;
}

static void appendCommandMsg(String &target, const String &message)
{
    if (target.length() > 0)
    {
        target += " | ";
    }
    target += message;
}

static String getCurrentDateTimeStr()
{
    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_r(&now, &tmNow);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmNow);
    return String(buf);
}

static String buildClientsHtml()
{
    int n = WiFi.softAPgetStationNum();
    String html = "<h2>Equipos conectados al AP (" + String(n) + ")</h2>";
    if (n == 0)
    {
        html += "<p>Ningun equipo conectado.</p>";
    }
    else
    {
        html += "<table border='1' cellpadding='4'><tr><th>#</th><th>MAC</th></tr>";
        wifi_sta_list_t stationList;
        esp_wifi_ap_get_sta_list(&stationList);
        for (int i = 0; i < stationList.num; i++)
        {
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     stationList.sta[i].mac[0], stationList.sta[i].mac[1],
                     stationList.sta[i].mac[2], stationList.sta[i].mac[3],
                     stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
            html += "<tr><td>" + String(i + 1) + "</td><td>" + mac + "</td></tr>";
        }
        html += "</table>";
    }
    return html;
}

static void handleRoot()
{
    LedParpadea(1, 40, 40);
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
                  "<title>" +
                  String(webServerConfig->AppNombre) + "</title></head><body>"
                                                       "<h1>" +
                  String(webServerConfig->AppNombre) + "</h1>"
                                                       "<p>Producto: " +
                  String(webServerConfig->AppProducto) + " &nbsp; "
                                                         "Modelo: " +
                  String(webServerConfig->AppModelo) + " &nbsp; "
                                                       "ID: " +
                  String(webServerConfig->mqttMac10) + "</p>"
                                                       "<p>Fecha/Hora: " +
                  getCurrentDateTimeStr() + "</p>"
                                            "<p><a href='/config'>Ver configuracion</a></p>"
                                            "<p><a href='/comandos'>Probar comandos</a></p>"
                                            "<p><a href='/comandos?save=true'>Guardar config.json</a></p>"
                                            "<p><a href='/comandos?reset=true'>Reset</a></p>"
                                            "<p><a href='/comandos?clear=true&reset=true'>Borrado y reset</a></p>" +
                  buildClientsHtml() +
                  "</body></html>";
    webServer.send(200, "text/html", html);
}

static void handleConfig()
{
    LedParpadea(1, 40, 40);
    const Config &c = *webServerConfig;
    String json = "{";
    json += "\"AppNombre\":\"" + String(c.AppNombre) + "\",";
    json += "\"AppBluid\":\"" + String(c.AppBluid) + "\",";
    json += "\"AppVersion\":" + String(c.AppVersion) + ",";
    json += "\"AppProducto\":\"" + String(c.AppProducto) + "\",";
    json += "\"AppModelo\":\"" + String(c.AppModelo) + "\",";
    json += "\"httpHost\":\"" + String(c.httpHost) + "\",";
    json += "\"httPort\":" + String(c.httPort) + ",";
    json += "\"wifiHost\":\"" + String(c.wifiHost) + "\",";
    json += "\"wifiPassword\":\"" + String(c.wifiPassword) + "\",";
    json += "\"mqttHost\":\"" + String(c.mqttHost) + "\",";
    json += "\"mqttUsuario\":\"" + String(c.mqttUsuario) + "\",";
    json += "\"mqttPassword\":\"" + String(c.mqttPassword) + "\",";

    json += "\"macAP\":\"" + String(c.macAP) + "\",";
    json += "\"macBT\":\"" + String(c.macBluetooth) + "\",";
    json += "\"macSTA\":\"" + String(c.macSTA) + "\",";
    json += "\"mqttMac\":\"" + String(c.mqttMac) + "\",";
    json += "\"mqttModelo\":\"" + String(c.mqttModelo) + "\",";

    json += "\"mqttMac6\":\"" + String(c.mqttMac6) + "\",";
    json += "\"mqttMac10\":\"" + String(c.mqttMac10) + "\",";
    json += "\"mqttTopic\":\"" + String(c.mqttTopic) + "\",";
    json += "\"wifiIp\":\"" + String(c.wifiIp) + "\",";
    json += "\"fechaHora\":\"" + getCurrentDateTimeStr() + "\"";

    json += "}";
    webServer.send(200, "application/json", json);
}

static void handleComandos()
{
    LedParpadea(1, 40, 40);
    bool resetRequested = false;
    int totalOrdenesRecibidas = webServer.args();
    int ordenesProcesadasCorrectamente = 0;
    int ordenesMalProcesadas = 0;
    int ordenesNoProcesadasNoExisten = 0;

    String msgOrdenesProcesadasCorrectamente;
    String msgOrdenesMalProcesadas;
    String msgOrdenesNoProcesadasNoExisten;

    for (int i = 0; i < webServer.args(); i++)
    {
        String argName = webServer.argName(i);
        String argValue = webServer.arg(i);
        String ordenOriginal = argName + "=" + argValue;
        String normalizedName = argName;
        normalizedName.toLowerCase();

        if (normalizedName == "reset")
        {
            if (VerificaBoleano(argValue))
            {
                resetRequested = true;
                ordenesProcesadasCorrectamente++;
                appendCommandMsg(msgOrdenesProcesadasCorrectamente, ordenOriginal);
            }
            else
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (valor no booleano)");
            }
        }
        else if (normalizedName == "clear")
        {
            if (VerificaBoleano(argValue))
            {
                bool clearExecuted = false;
                bool clearFileExisted = false;
                bool clearMountOk = true;

                clearExecuted = borrarConfigJson(APP_CONFIG_FILENAME, clearFileExisted, clearMountOk);
                if (!clearMountOk)
                {
                    ordenesMalProcesadas++;
                    appendCommandMsg(msgOrdenesMalProcesadas,
                                     ordenOriginal + " (error montando SPIFFS)");
                }
                else
                {
                    ordenesProcesadasCorrectamente++;
                    if (clearFileExisted && clearExecuted)
                    {
                        appendCommandMsg(msgOrdenesProcesadasCorrectamente,
                                         ordenOriginal + " (config.json borrado)");
                    }
                    else
                    {
                        appendCommandMsg(msgOrdenesProcesadasCorrectamente,
                                         ordenOriginal + " (config.json no existia)");
                    }
                }
            }
            else
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (valor no booleano)");
            }
        }
        else if (normalizedName == "save")
        {
            if (VerificaBoleano(argValue))
            {
                bool saveOk = saveConfigToFile(APP_CONFIG_FILENAME, *webServerConfig);
                if (saveOk)
                {
                    ordenesProcesadasCorrectamente++;
                    appendCommandMsg(msgOrdenesProcesadasCorrectamente, ordenOriginal + " (config.json guardado)");
                }
                else
                {
                    ordenesMalProcesadas++;
                    appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (error guardando config.json)");
                }
            }
            else
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (valor no booleano)");
            }
        }
        else if (normalizedName == "fecha")
        {
            if (VerificaBoleano(argValue))
            {
                if (syncEsp32DateTimeFromServer(*webServerConfig))
                {
                    ordenesProcesadasCorrectamente++;
                    appendCommandMsg(msgOrdenesProcesadasCorrectamente,
                                     ordenOriginal + " (fecha sincronizada: " + getCurrentDateTimeStr() + ")");
                }
                else
                {
                    ordenesMalProcesadas++;
                    appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (error sincronizando fecha)");
                }
            }
            else
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (valor no booleano)");
            }
        }
        else if (normalizedName == "appproducto")
        {
            argValue.trim();
            if (argValue.length() == 0)
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas, ordenOriginal + " (valor vacio)");
            }
            else if (argValue.length() >= sizeof(webServerConfig->AppProducto))
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas,
                                 ordenOriginal + " (longitud maxima " +
                                     String(sizeof(webServerConfig->AppProducto) - 1) + ")");
            }
            else
            {
                strlcpy(webServerConfig->AppProducto, argValue.c_str(), sizeof(webServerConfig->AppProducto));
                calcularMqttDatos(*webServerConfig);
                ordenesProcesadasCorrectamente++;
                appendCommandMsg(msgOrdenesProcesadasCorrectamente,
                                 ordenOriginal + " (AppProducto actualizado)");
            }
        }
        else if (normalizedName == "appmodelo")
        {
            argValue.trim();
            if (argValue.length() != 2)
            {
                ordenesMalProcesadas++;
                appendCommandMsg(msgOrdenesMalProcesadas,
                                 ordenOriginal + " (debe tener exactamente 2 caracteres)");
            }
            else
            {
                strlcpy(webServerConfig->AppModelo, argValue.c_str(), sizeof(webServerConfig->AppModelo));
                calcularMqttDatos(*webServerConfig);
                ordenesProcesadasCorrectamente++;
                appendCommandMsg(msgOrdenesProcesadasCorrectamente,
                                 ordenOriginal + " (AppModelo actualizado)");
            }
        }
        else
        {
            ordenesNoProcesadasNoExisten++;
            appendCommandMsg(msgOrdenesNoProcesadasNoExisten, ordenOriginal);
        }
    }

    bool ok = totalOrdenesRecibidas > 0 &&
              ordenesMalProcesadas == 0 &&
              ordenesNoProcesadasNoExisten == 0;

    String json = "{";
    json += "\"Total\":" + String(totalOrdenesRecibidas) + ",";
    json += "\"Ok\":" + String(ordenesProcesadasCorrectamente) + ",";
    json += "\"msgOk\":\"" +
            escapeJsonString(msgOrdenesProcesadasCorrectamente) + "\"";

    if (ordenesMalProcesadas > 0)
    {
        json += ",\"Mal\":" + String(ordenesMalProcesadas);
        json += ",\"msgMal\":\"" + escapeJsonString(msgOrdenesMalProcesadas) + "\"";
    }

    if (ordenesNoProcesadasNoExisten > 0)
    {
        json += ",\"NoExisten\":" + String(ordenesNoProcesadasNoExisten);
        json += ",\"msgNoExisten\":\"" +
                escapeJsonString(msgOrdenesNoProcesadasNoExisten) + "\"";
    }

    json += "}";

    webServer.send(ok ? 200 : 400, "application/json", json);
    if (resetRequested)
    {
        delay(100);
        ResetEsp32();
    }
}

static void handleNotFound()
{
    webServer.send(404, "text/plain", "Pagina no encontrada");
}

static void handleNoContent()
{
    webServer.send(204, "text/plain", "");
}

void webServerBegin(Config &config)
{
    webServerConfig = &config;
    webServer.on("/", handleRoot);
    webServer.on("/config", handleConfig);
    webServer.on("/comandos", handleComandos);
    webServer.on("/favicon.ico", HTTP_ANY, handleNoContent);
    webServer.on("/generate_204", HTTP_ANY, handleNoContent);
    webServer.on("/hotspot-detect.html", HTTP_ANY, handleNoContent);
    webServer.on("/connecttest.txt", HTTP_ANY, handleNoContent);
    webServer.on("/ncsi.txt", HTTP_ANY, handleNoContent);
    webServer.onNotFound(handleNotFound);
    webServer.begin();
    Serial.println("Servidor web iniciado en puerto 80");
}

void webServerHandle()
{
    webServer.handleClient();
}