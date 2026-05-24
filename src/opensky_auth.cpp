#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "opensky_auth.h"


bool fetchOpenSkyToken(
    const String& clientId,
    const String& clientSecret,
    String& accessToken
    ) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(15000);
    https.useHTTP10(true);

    String url = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";

    if (!https.begin(client, url)) {
        Serial.println("OpenSky auth HTTPS begin failed");
        return false;
    }

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=client_credentials";
    body += "&client_id=";
    body += clientId;
    body += "&client_secret=";
    body += clientSecret;

    Serial.println();
    Serial.println("Requesting OpenSky access token...");

    int httpCode = https.POST(body);

    Serial.print("OpenSky auth HTTP status: ");
    Serial.println(httpCode);

    if (httpCode != 200) {
        Serial.println("OpenSky auth request failed");
        Serial.println(https.getString());
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("OpenSky auth JSON parse failed: ");
        Serial.println(error.c_str());
        return false;
    }

    const char* token = doc["access_token"];

    if (!token) {
        Serial.println("No access_token in OpenSky auth response");
        return false;
    }

    accessToken = token;

    Serial.println("OpenSky access token received");

    return true;
}