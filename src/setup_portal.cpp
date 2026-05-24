#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "setup_portal.h"
#include "config_manager.h"


WebServer setupServer(80);
DNSServer dnsServer;

void handleSetupPage() {
    String html = "";

    html += "<!DOCTYPE html><html><head>";
    html += "<title>DeskDar Setup</title>";
    html += "</head><body>";
    html += "<h1>DeskDar Setup</h1>";

    html += "<form method='POST' action='/save'>";

    html += "<p>WiFi SSID:<br>";
    html += "<input name='wifi_ssid'></p>";

    html += "<p>WiFi Password:<br>";
    html += "<input name='wifi_password' type='password'></p>";

    html += "<p>Postcode:<br>";
    html += "<input name='postcode'></p>";

    html += "<p>OpenSky Client ID:<br>";
    html += "<input name='opensky_client_id'></p>";

    html += "<p>OpenSky Client Secret:<br>";
    html += "<input name='opensky_client_secret' type='password'></p>";

    html += "<button type='submit'>Save</button>";

    html += "</form>";
    html += "</body></html>";

    setupServer.send(200, "text/html", html);
}

void handleSaveConfig() {
    DeskDarConfig config;

    config.wifiSsid = setupServer.arg("wifi_ssid");
    config.wifiPassword = setupServer.arg("wifi_password");
    config.postcode = setupServer.arg("postcode");
    config.openSkyClientId = setupServer.arg("opensky_client_id");
    config.openSkyClientSecret = setupServer.arg("opensky_client_secret");

    saveConfig(config);

    setupServer.send(
        200,
        "text/html",
        "<h1>DeskDar config saved</h1><p>Device will restart.</p>"
    );

    delay(2000);
    ESP.restart();
}

void startSetupPortal() {
    Serial.println("Starting DeskDar setup portal...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("DeskDar Setup");

    IPAddress apIP = WiFi.softAPIP();

    dnsServer.start(53, "*", apIP);

    Serial.print("Setup portal IP: ");
    Serial.println(apIP);

    setupServer.on("/", handleSetupPage);
    setupServer.on("/save", HTTP_POST, handleSaveConfig);

    setupServer.onNotFound([]() {
        setupServer.sendHeader("Location", "/", true);
        setupServer.send(302, "text/plain", "");
    });

    setupServer.begin();

    while (true) {
        dnsServer.processNextRequest();
        setupServer.handleClient();
        delay(10);
    }
}