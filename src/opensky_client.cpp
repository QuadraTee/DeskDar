#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "opensky_client.h"

void calculateRadarPosition(
    Aircraft& aircraft,
    int centerX,
    int centerY,
    int radarRadius,
    float maxRangeKm
);

void calculateHeadingVector(
    Aircraft& aircraft,
    int arrowLength
);

float degreesToRadians(float degrees) {
    return degrees * PI / 180.0;
}

float calculateDistanceKm(float lat1, float lon1, float lat2, float lon2) {
    const float earthRadiusKm = 6371.0;

    float dLat = degreesToRadians(lat2 - lat1);
    float dLon = degreesToRadians(lon2 - lon1);

    float a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(degreesToRadians(lat1)) *
        cos(degreesToRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return earthRadiusKm * c;
}

float calculateBearingDegrees(float lat1, float lon1, float lat2, float lon2) {
    float phi1 = degreesToRadians(lat1);
    float phi2 = degreesToRadians(lat2);
    float deltaLon = degreesToRadians(lon2 - lon1);

    float y = sin(deltaLon) * cos(phi2);
    float x =
        cos(phi1) * sin(phi2) -
        sin(phi1) * cos(phi2) * cos(deltaLon);

    float bearing = atan2(y, x) * 180.0 / PI;

    if (bearing < 0) {
        bearing += 360.0;
    }

    return bearing;
}

void calculateRadarPosition(
    Aircraft& aircraft,
    int centerX,
    int centerY,
    int radarRadius,
    float maxRangeKm
) {
    float clampedDistance = aircraft.distanceKm;

    if (clampedDistance > maxRangeKm) {
        clampedDistance = maxRangeKm;
    }

    float distanceRatio = clampedDistance / maxRangeKm;

    float screenRadius = distanceRatio * radarRadius;

    float bearingRad = degreesToRadians(
        aircraft.bearingDegrees
    );

    aircraft.radarX =
        centerX + sin(bearingRad) * screenRadius;

    aircraft.radarY =
        centerY - cos(bearingRad) * screenRadius;
}

void calculateHeadingVector(
    Aircraft& aircraft,
    int arrowLength
) {
    float headingRad =
        degreesToRadians(aircraft.headingDegrees);

    aircraft.headingX =
        aircraft.radarX + sin(headingRad) * arrowLength;

    aircraft.headingY =
        aircraft.radarY - cos(headingRad) * arrowLength;
}

String compassDirectionFromBearing(float bearing) {
    const char* directions[] = {
        "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    };

    int index = round(bearing / 45.0);

    if (index == 8) {
        index = 0;
    }

    return String(directions[index]);
}

int fetchNearbyAircraft(
    float latitude,
    float longitude,
    Aircraft aircraftList[],
    int maxAircraft
) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(20000);
    https.useHTTP10(true);

    float boxSize = 0.25;

    float lamin = latitude - boxSize;
    float lamax = latitude + boxSize;
    float lomin = longitude - boxSize;
    float lomax = longitude + boxSize;

    String url = "https://opensky-network.org/api/states/all?";
    url += "lamin=" + String(lamin, 6);
    url += "&lamax=" + String(lamax, 6);
    url += "&lomin=" + String(lomin, 6);
    url += "&lomax=" + String(lomax, 6);

    Serial.println();
    Serial.println("Requesting aircraft:");
    Serial.println(url);

    if (!https.begin(client, url)) {
        Serial.println("OpenSky HTTPS begin failed");
        return 0;
    }

    int httpCode = https.GET();

    Serial.print("OpenSky HTTP status: ");
    Serial.println(httpCode);

    if (httpCode != 200) {
        Serial.println("OpenSky request failed");
        https.end();
        return 0;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("OpenSky JSON parse failed: ");
        Serial.println(error.c_str());
        return 0;
    }

    JsonArray states = doc["states"];

    Serial.println();
    Serial.print("Aircraft found: ");
    Serial.println(states.size());

    int aircraftCount = 0;

    for (JsonArray state : states) {
        if (aircraftCount >= maxAircraft) {
            break;
        }

        Aircraft aircraft;

        aircraft.icao24 = state[0].as<String>();
        aircraft.callsign = state[1].as<String>();
        aircraft.originCountry = state[2].as<String>();

        aircraft.longitude = state[5] | 0.0;
        aircraft.latitude = state[6] | 0.0;

        aircraft.altitudeMeters = state[7] | 0.0;
        aircraft.altitudeFeet = aircraft.altitudeMeters * 3.28084;

        aircraft.onGround = state[8] | false;

        aircraft.speedMetersPerSecond = state[9] | 0.0;
        aircraft.speedKnots = aircraft.speedMetersPerSecond * 1.94384;

        aircraft.headingDegrees = state[10] | 0.0;
        aircraft.verticalRate = state[11] | 0.0;

        aircraft.distanceKm = calculateDistanceKm(
            latitude,
            longitude,
            aircraft.latitude,
            aircraft.longitude
        );

        aircraft.bearingDegrees = calculateBearingDegrees(
            latitude,
            longitude,
            aircraft.latitude,
            aircraft.longitude
        );

        aircraft.compassDirection = compassDirectionFromBearing(
            aircraft.bearingDegrees
        );

        calculateRadarPosition(
        aircraft,
        120,
        120,
        110,
        50.0
        );

        calculateHeadingVector(
        aircraft,
        8
        );

        aircraftList[aircraftCount] = aircraft;
        aircraftCount++;
    }

    for (int i = 0; i < aircraftCount - 1; i++) {
        for (int j = i + 1; j < aircraftCount; j++) {
            if (aircraftList[j].distanceKm < aircraftList[i].distanceKm) {
                Aircraft temp = aircraftList[i];
                aircraftList[i] = aircraftList[j];
                aircraftList[j] = temp;
            }
        }
    }

    return aircraftCount;
}