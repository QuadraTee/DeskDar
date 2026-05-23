#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "aircraft_metadata.h"

const int MAX_METADATA_QUEUE = 10;

String metadataQueue[MAX_METADATA_QUEUE];
int metadataQueueCount = 0;

bool isInQueue(const String& icao24) {
    for (int i = 0; i < metadataQueueCount; i++) {
        if (metadataQueue[i] == icao24) {
            return true;
        }
    }

    return false;
}

void queueMetadataLookup(const String& icao24) {
    if (icao24.length() == 0) {
        return;
    }

    if (isInQueue(icao24)) {
        return;
    }

    if (metadataQueueCount >= MAX_METADATA_QUEUE) {
        return;
    }

    metadataQueue[metadataQueueCount] = icao24;
    metadataQueueCount++;

    Serial.print("Queued metadata lookup: ");
    Serial.println(icao24);
}

void enrichAircraftMetadata(Aircraft& aircraft) {
    aircraft.registration = "Unknown";
    aircraft.aircraftModel = "Unknown";
    aircraft.aircraftType = "Unknown";

    if (aircraft.icao24 == "ab8fcb") {
        aircraft.registration = "N844MC";
        aircraft.aircraftModel = "Boeing 787-9 Dreamliner";
        aircraft.aircraftType = "Wide-body Airliner";
        return;
    }

    queueMetadataLookup(aircraft.icao24);
}

void processMetadataQueue() {
    if (metadataQueueCount == 0) {
        return;
    }

    String icao24 = metadataQueue[0];

    Serial.print("Looking up metadata online for: ");
    Serial.println(icao24);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(10000);
    https.useHTTP10(true);

    String url = "https://api.adsbdb.com/v0/aircraft/";
    url += icao24;

    if (!https.begin(client, url)) {
        Serial.println("Metadata HTTPS begin failed");
        return;
    }

    int httpCode = https.GET();

    Serial.print("Metadata HTTP status: ");
    Serial.println(httpCode);

    if (httpCode == 200) {
        String payload = https.getString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            JsonObject data = doc["response"]["aircraft"];

            String registration = data["registration"] | "Unknown";
            String manufacturer = data["manufacturer"] | "";
            String type = data["type"] | "";
            String icaoType = data["icao_type"] | "Unknown";

            String model = "Unknown";

            if (manufacturer.length() > 0 && type.length() > 0) {
                model = manufacturer + " " + type;
            } else if (type.length() > 0) {
                model = type;
            }

            Serial.println("Metadata found:");
            Serial.print("Registration: ");
            Serial.println(registration);
            Serial.print("Model: ");
            Serial.println(model);
            Serial.print("Type: ");
            Serial.println(icaoType);
        } else {
            Serial.print("Metadata JSON parse failed: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.println("Metadata lookup failed");
    }

    https.end();

    for (int i = 1; i < metadataQueueCount; i++) {
        metadataQueue[i - 1] = metadataQueue[i];
    }

    metadataQueueCount--;
}