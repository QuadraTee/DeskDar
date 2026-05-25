#include <Preferences.h>
#include "config_manager.h"

Preferences preferences;

bool hasWiFiConfig(const DeskDarConfig& config) {
    return (
        config.wifiSsid.length() > 0 &&
        config.wifiPassword.length() > 0
    );
}

bool hasAppConfig(const DeskDarConfig& config) {
    return (
        config.postcode.length() > 0 &&
        config.openSkyClientId.length() > 0 &&
        config.openSkyClientSecret.length() > 0
    );
}

bool loadConfig(DeskDarConfig& config) {
    preferences.begin("deskdar", true);

    config.wifiSsid = preferences.getString("wifi_ssid", "");
    config.wifiPassword = preferences.getString("wifi_pass", "");
    config.postcode = preferences.getString("postcode", "");
    config.openSkyClientId = preferences.getString("os_id", "");
    config.openSkyClientSecret = preferences.getString("os_secret", "");
    config.radarOrientationDegrees = preferences.getFloat("orientation", 0.0);

    preferences.end();

    return hasWiFiConfig(config);
}

void saveConfig(const DeskDarConfig& config) {
    preferences.begin("deskdar", false);

    preferences.putString("wifi_ssid", config.wifiSsid);
    preferences.putString("wifi_pass", config.wifiPassword);
    preferences.putString("postcode", config.postcode);
    preferences.putString("os_id", config.openSkyClientId);
    preferences.putString("os_secret", config.openSkyClientSecret);
    preferences.putFloat("orientation", config.radarOrientationDegrees);

    preferences.end();
}

void saveWiFiConfig(const String& wifiSsid, const String& wifiPassword) {
    DeskDarConfig config;
    loadConfig(config);

    config.wifiSsid = wifiSsid;
    config.wifiPassword = wifiPassword;

    saveConfig(config);
}

void saveAppConfig(
    const String& postcode,
    const String& openSkyClientId,
    const String& openSkyClientSecret
) {
    DeskDarConfig config;
    loadConfig(config);

    config.postcode = postcode;
    config.openSkyClientId = openSkyClientId;
    config.openSkyClientSecret = openSkyClientSecret;

    saveConfig(config);
}

void clearConfig() {
    preferences.begin("deskdar", false);
    preferences.clear();
    preferences.end();
}
