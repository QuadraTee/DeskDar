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
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>DeskDar WiFi Setup</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;background:#0b0f0b;color:#e8ffe8;margin:0;padding:24px;}";
    html += ".card{max-width:520px;margin:auto;background:#111b11;border:1px solid #245c24;border-radius:12px;padding:20px;}";
    html += "input{width:100%;padding:10px;margin-top:6px;box-sizing:border-box;background:#061006;color:#e8ffe8;border:1px solid #397a39;border-radius:6px;}";
    html += "button{padding:12px 18px;background:#18a118;color:white;border:0;border-radius:6px;font-weight:bold;}";
    html += "p.help{color:#a9cfa9;line-height:1.4;}";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='card'>";
    html += "<h1>DeskDar WiFi Setup</h1>";
    html += "<p class='help'>Connect DeskDar to your home WiFi. After it reboots, open the DeskDar dashboard on your normal network to enter postcode and OpenSky credentials.</p>";

    html += "<form method='POST' action='/save'>";

    html += "<p>WiFi SSID:<br>";
    html += "<input name='wifi_ssid' required></p>";

    html += "<p>WiFi Password:<br>";
    html += "<input name='wifi_password' type='password' required></p>";

    html += "<button type='submit'>Save WiFi Settings</button>";

    html += "</form>";
    html += "</div>";
    html += "</body></html>";

    setupServer.send(200, "text/html", html);
}

void handleSaveConfig() {
    String wifiSsid = setupServer.arg("wifi_ssid");
    String wifiPassword = setupServer.arg("wifi_password");

    saveWiFiConfig(wifiSsid, wifiPassword);

    setupServer.send(
        200,
        "text/html",
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head>"
        "<body style='font-family:Arial,sans-serif;background:#0b0f0b;color:#e8ffe8;padding:24px;'>"
        "<h1>DeskDar WiFi saved</h1>"
        "<p>Device will restart and connect to your WiFi.</p>"
        "<p>After reboot, open the DeskDar IP address shown in Serial Monitor, or check your router for the device IP.</p>"
        "</body></html>"
    );

    delay(2000);
    ESP.restart();
}

void startSetupPortal() {
    Serial.println("Starting DeskDar WiFi setup portal...");

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
