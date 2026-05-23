#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "postcode_client.h"

bool fetchPostcode(
    const char* postcode,
    float& latitude,
    float& longitude
) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(15000);
    https.useHTTP10(true);

    String url = "https://api.postcodes.io/postcodes/";
    url += postcode;

    Serial.println();
    Serial.println("Requesting postcode:");
    Serial.println(url);

    if (!https.begin(client, url)) {
        Serial.println("Postcode HTTPS begin failed");
        return false;
    }

    int httpCode = https.GET();

    Serial.print("Postcode HTTP status: ");
    Serial.println(httpCode);

    if (httpCode != 200) {
        Serial.println("Postcode request failed");
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("Postcode JSON parse failed: ");
        Serial.println(error.c_str());
        return false;
    }

    const char* formattedPostcode = doc["result"]["postcode"];
    latitude = doc["result"]["latitude"];
    longitude = doc["result"]["longitude"];

    Serial.println("Postcode parsed:");
    Serial.print("Postcode: ");
    Serial.println(formattedPostcode);
    Serial.print("Latitude: ");
    Serial.println(latitude, 6);
    Serial.print("Longitude: ");
    Serial.println(longitude, 6);

    return true;
}