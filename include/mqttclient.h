#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include "config.h"

// Inicializa y conecta el cliente MQTT usando los datos de Config.
// Debe llamarse despues de conectar WiFi STA.
void mqttBegin(Config &config);

// Procesa mensajes entrantes y mantiene la conexion activa.
// Llamar desde loop().
void mqttHandle();

// Fuerza un intento inmediato de conexion al broker MQTT.
// Devuelve true si queda conectado tras el intento.
bool mqttReconnectNow();

// Publica el estado online en el topic fijo IHC/ONLINE.
bool SendOnLine();

// Publica un mensaje en un topic.
// Devuelve true si el envio fue aceptado por la libreria.
bool mqttPublish(const char *topic, const char *payload, bool retained = false);

// Devuelve true si el cliente esta actualmente conectado al broker.
bool mqttIsConnected();

#endif
