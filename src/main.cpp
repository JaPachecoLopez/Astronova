#include <Arduino.h>
#include "funciones.h"
#include "config.h"
#include "wificfg.h"
#include "wifihttp.h"
#include "wifiserver.h"
#include "mqttclient.h"
#include "blecfg.h"

static Config config;
static const char *CONFIG_FILENAME = "/config.json";

void setup()
{
  iniciaApp(config, CONFIG_FILENAME);
  bleBegin(config);
  webServerBegin(config);
  mqttBegin(config);
}

void loop()
{
  verificarYReconectarWiFi(config);
  bleHandle();
  webServerHandle();
  mqttHandle();
}