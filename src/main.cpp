#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "secrets.h"

const char* TEST_ICAO24 = "ADD8BF";


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("DeskDar metadata-only test");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(15000);
    https.useHTTP10(true);

    String url = "https://api.adsbdb.com/v0/aircraft/";
    url += TEST_ICAO24;

    Serial.print("Requesting metadata: ");
    Serial.println(url);

    if (!https.begin(client, url)) {
        Serial.println("HTTPS begin failed");
        return;
    }

    int httpCode = https.GET();

    Serial.print("HTTP status: ");
    Serial.println(httpCode);

    if (httpCode == 200) {
        String payload = https.getString();

        Serial.println("Raw response:");
        Serial.println(payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("JSON parse failed: ");
            Serial.println(error.c_str());
            https.end();
            return;
        }

        JsonObject aircraft = doc["response"]["aircraft"];

        String registration = aircraft["registration"] | "Unknown";
        String manufacturer = aircraft["manufacturer"] | "";
        String type = aircraft["type"] | "";
        String icaoType = aircraft["icao_type"] | "Unknown";

        String model = "Unknown";

        if (manufacturer.length() > 0 && type.length() > 0) {
            model = manufacturer + " " + type;
        } else if (type.length() > 0) {
            model = type;
        }

        Serial.println();
        Serial.println("Parsed metadata:");
        Serial.print("Registration: ");
        Serial.println(registration);
        Serial.print("Model: ");
        Serial.println(model);
        Serial.print("Type: ");
        Serial.println(icaoType);
    } else {
        Serial.println("Request failed");
    }

    https.end();
}

void loop() {
}