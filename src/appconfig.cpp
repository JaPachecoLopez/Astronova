#include "appconfig.h"
#include <Arduino.h>

const char *const APP_CONFIG_FILENAME = "/config.json";

void initDefaultConfig(Config &config)
{
    strlcpy(config.AppNombre, "Astronova", sizeof(config.AppNombre));
    strlcpy(config.AppBluid, __DATE__ " " __TIME__, sizeof(config.AppBluid));
    config.AppVersion = 2;
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
