#ifndef WIFICFG_H
#define WIFICFG_H

#include "config.h"

// Controla el LED de estado (on/off logico, respeta polaridad activa alta/baja).
void setStatusLed(bool on);

// Devuelve true si la interfaz STA esta conectada.
bool isWiFiConnected();

// Utilidades de WiFi y direccionamiento MAC.
void addToMac(uint8_t *mac, uint64_t value);
void wifiGetMACAddresses(Config &config);

// Gestion de conectividad AP+STA y reintentos.
void conectarWiFiAPySTA(Config &config);
void verificarYReconectarWiFi(Config &config);

#endif