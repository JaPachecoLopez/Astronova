#ifndef FUNCIONES_H
#define FUNCIONES_H

#include "config.h"
#include "wificfg.h"

void LedParpadea(int veces, int periodo_ms_on = 500, int periodo_ms_off = 500);
void ResetEsp32();
void iniciaApp(Config &config, const char *configFilename);
void initDefaultConfig(Config &config);
void calcularMqttDatos(Config &config);
void calcularMqttMacs(Config &config);
void mostrarConfiguracionSerial(const Config &config);
bool borrarConfigJson(const char *configFilename, bool &fileExisted, bool &mountOk);
bool VerificaBoleano(String value);
void obtenerFechaHoraEsp32Iso8601(char *buf, size_t bufLen);
void getIso8601DateTime(char *buf, size_t bufLen);
void MayToMin(const char *src, char *dst, size_t dstLen);

#endif
