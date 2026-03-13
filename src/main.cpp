#include <Arduino.h>
#include "funciones.h"
#include "appconfig.h"
#include "wificfg.h"
#include "wifihttp.h"
#include "wifiserver.h"
#include "mqttclient.h"
#include "blecfg.h"
// Para subir repositorio desde terminal: (git add .)    (git commit -m "mensaje")   (git push)

static Config config;

void setup()
{
  iniciaApp(config, APP_CONFIG_FILENAME);
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