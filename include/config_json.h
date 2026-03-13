#ifndef CONFIG_JSON_H
#define CONFIG_JSON_H

#include "config.h"

// Tamaño máximo del documento JSON
#define MAX_JSON_SIZE 1024

bool loadConfigFromFile(const char *filename, Config &config);
bool saveConfigToFile(const char *filename, const Config &config);

#endif