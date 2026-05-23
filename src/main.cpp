#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "aircraft.h"
#include "display.h"

const char* WIFI_SSID = "Vodafone3390-2.4G";
const char* WIFI_PASSWORD = "xZcdzCd6fRtcFTaK";

const char* POSTCODE = "WV95BL";
const int MAX_AIRCRAFT = 20;

void connectWiFi();
bool fetchPostcode(float& latitude, float& longitude);
void fetchNearbyAircraft(float latitude, float longitude);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("DeskDar ESP32 live aircraft test");

    connectWiFi();

    float latitude = 0.0;
    float longitude = 0.0;

    if (fetchPostcode(latitude, longitude)) {
        fetchNearbyAircraft(latitude, longitude);
    } else {
        Serial.println("Could not fetch postcode location.");
    }
}

void loop() {
    delay(10000);
}

void connectWiFi() {
    Serial.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

bool fetchPostcode(float& latitude, float& longitude) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(15000);
    https.useHTTP10(true);

    String url = "https://api.postcodes.io/postcodes/";
    url += POSTCODE;

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


void fetchNearbyAircraft(float latitude, float longitude) {

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
        return;
    }

    int httpCode = https.GET();

    Serial.print("OpenSky HTTP status: ");
    Serial.println(httpCode);

    if (httpCode != 200) {
        Serial.println("OpenSky request failed");
        https.end();
        return;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("OpenSky JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }

    JsonArray states = doc["states"];

    Serial.println();
    Serial.print("Aircraft found: ");
    Serial.println(states.size());

    const int MAX_AIRCRAFT = 20;

    Aircraft aircraftList[MAX_AIRCRAFT];
    int aircraftCount = 0;

    for (JsonArray state : states) {

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

        if (aircraftCount < MAX_AIRCRAFT) {
            aircraftList[aircraftCount] = aircraft;
            aircraftCount++;
        }
    }

   // Sort aircraft by distance (closest first)

for (int i = 0; i < aircraftCount - 1; i++) {

    for (int j = i + 1; j < aircraftCount; j++) {

        if (aircraftList[j].distanceKm < aircraftList[i].distanceKm) {

            Aircraft temp = aircraftList[i];
            aircraftList[i] = aircraftList[j];
            aircraftList[j] = temp;
        }
    }
}

renderAircraftList(aircraftList, aircraftCount);
}