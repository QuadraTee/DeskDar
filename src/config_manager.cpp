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

    config.showLabelRegistration = preferences.getBool("lbl_reg", false);
    config.showLabelModel = preferences.getBool("lbl_model", false);
    config.showLabelType = preferences.getBool("lbl_type", false);
    config.showLabelDistance = preferences.getBool("lbl_dist", true);
    config.showLabelAltitude = preferences.getBool("lbl_alt", true);
    config.showLabelSpeed = preferences.getBool("lbl_speed", false);
    config.showLabelHeading = preferences.getBool("lbl_heading", false);
    config.showAircraftTrails = preferences.getBool("show_trails", true);
    config.trailFadeSeconds = preferences.getInt("trail_fade_s", 60);

    config.showAirports = preferences.getBool("show_ap", true);
    config.showMajorAirports = preferences.getBool("show_ap_major", true);
    config.showGaAirports = preferences.getBool("show_ap_ga", true);

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

    preferences.putBool("lbl_reg", config.showLabelRegistration);
    preferences.putBool("lbl_model", config.showLabelModel);
    preferences.putBool("lbl_type", config.showLabelType);
    preferences.putBool("lbl_dist", config.showLabelDistance);
    preferences.putBool("lbl_alt", config.showLabelAltitude);
    preferences.putBool("lbl_speed", config.showLabelSpeed);
    preferences.putBool("lbl_heading", config.showLabelHeading);
    preferences.putBool("show_trails", config.showAircraftTrails);
    preferences.putInt("trail_fade_s", config.trailFadeSeconds);

    preferences.putBool("show_ap", config.showAirports);
    preferences.putBool("show_ap_major", config.showMajorAirports);
    preferences.putBool("show_ap_ga", config.showGaAirports);

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
