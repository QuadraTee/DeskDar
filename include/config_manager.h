#pragma once

#include <Arduino.h>

struct DeskDarConfig {
    String wifiSsid;
    String wifiPassword;
    String postcode;
    String openSkyClientId;
    String openSkyClientSecret;
};

bool loadConfig(DeskDarConfig& config);
void saveConfig(const DeskDarConfig& config);
void clearConfig();