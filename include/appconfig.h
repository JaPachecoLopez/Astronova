#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <WiFi.h>

// Definiciones de pines
#define LED_PIN 8
#define LED_ACTIVE_LOW 1
struct Config
{
    // VARIABLES QUE SE ALMACENAN EN config.json
    char AppNombre[64];
    char AppBluid[24]; // "Mmm dd yyyy hh:mm:ss"
    int AppVersion;
    char AppProducto[10]; // Nombre del producto, "ANCPLUS" ,"ASTROLUM"
    char AppModelo[3];    // 2bytes+fin   1x ,5x ,9x ,Dx
    char wifiHost[64];
    char wifiPassword[32];
    char wifiApPassword[32];
    char httpHost[64];
    int httPort;
    char mqttHost[64];
    char mqttUsuario[32];
    char mqttPassword[32];
    // VARIABLES QUE NO SE ALMACENAN EN config.json SON CALCULADAS EN FUNCION DEL ESP32
    char wifiIp[16];       // xxx.xxx.xxx.xxx (obtenida al conectar al Router)
    char macSTA[18];       // AA:BB:CC:DD:EE:FF
    char macAP[18];        // AA:BB:CC:DD:EE:FF
    char macBluetooth[18]; // AA:BB:CC:DD:EE:FF
    char mqttMac[13];      // Mac limpia sin ":" y mayusculas A2D5C85010 ,
    char mqttMac6[7];      // 0 + ULTIMOS 5 CARACTERES DE LA MAC  IHD/ANC_PLUS/  "085010"   /A2D5C85010/response/cfg/nimble

    // VARIABLES QUE NO SE ALMACENAN EN config.json POR QUE SON CALCULADAS EN FUNCION DE MAC. MODELO Y PRODUCTO
    char mqttModelo[3]; // 1x->A1 ./ANC_PLUS/A1xxxxxxxx  '5x->A2 ./ANC_PLUS/A2xx '9x->D1 ./LOG_PLUS/D1xxxxxxxx 'Dx->D2 './LOG_PLUS/D2xxxxxxxx
    char mqttMac10[11]; // mqttModelo +  ULTIMOS 8 CARACTERES DE LA MAC  MIHD/ANC_PLUS/085010/   "A2D5C85010"   /response/cfg/nimble
    char mqttTopic[80]; // IHD/ ANC_PLUS ó LOG_PLUS ó AST_LUM + / mqttMac10 + /
    char mqttVer0Cortex[20];
    char mqttVer1Cortex[20];
    char mqttVer0Esp32[20];
    char mqttVer1Esp32[20];
};

extern const char *const APP_CONFIG_FILENAME;

void initDefaultConfig(Config &config);

#endif
