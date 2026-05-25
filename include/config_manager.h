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
bool hasWiFiConfig(const DeskDarConfig& config);
bool hasAppConfig(const DeskDarConfig& config);

void saveConfig(const DeskDarConfig& config);
void saveWiFiConfig(const String& wifiSsid, const String& wifiPassword);
void saveAppConfig(
    const String& postcode,
    const String& openSkyClientId,
    const String& openSkyClientSecret
);

void clearConfig();
