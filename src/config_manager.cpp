#include <Preferences.h>
#include "config_manager.h"

Preferences preferences;

bool loadConfig(DeskDarConfig& config) {
    preferences.begin("deskdar", true);

    config.wifiSsid = preferences.getString("wifi_ssid", "");
    config.wifiPassword = preferences.getString("wifi_pass", "");
    config.postcode = preferences.getString("postcode", "");
    config.openSkyClientId = preferences.getString("os_id", "");
    config.openSkyClientSecret = preferences.getString("os_secret", "");

    preferences.end();

    return (
        config.wifiSsid.length() > 0 &&
        config.wifiPassword.length() > 0 &&
        config.postcode.length() > 0 &&
        config.openSkyClientId.length() > 0 &&
        config.openSkyClientSecret.length() > 0
    );
}

void saveConfig(const DeskDarConfig& config) {
    preferences.begin("deskdar", false);

    preferences.putString("wifi_ssid", config.wifiSsid);
    preferences.putString("wifi_pass", config.wifiPassword);
    preferences.putString("postcode", config.postcode);
    preferences.putString("os_id", config.openSkyClientId);
    preferences.putString("os_secret", config.openSkyClientSecret);

    preferences.end();
}

void clearConfig() {
    preferences.begin("deskdar", false);
    preferences.clear();
    preferences.end();
}