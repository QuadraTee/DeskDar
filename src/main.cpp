#include <Arduino.h>
#include <WiFi.h>

#include "aircraft.h"
#include "opensky_client.h"
#include "postcode_client.h"
#include "display.h"

const char* WIFI_SSID = "Vodafone3390-2.4G";
const char* WIFI_PASSWORD = "xZcdzCd6fRtcFTaK";
const char* POSTCODE = "WV95BL";

const unsigned long REFRESH_INTERVAL_MS = 15000;

float userLatitude = 0.0;
float userLongitude = 0.0;
bool locationReady = false;

unsigned long lastRefreshTime = 0;

void connectWiFi();

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("DeskDar starting...");

    connectWiFi();

    locationReady = fetchPostcode(
        POSTCODE,
        userLatitude,
        userLongitude
    );

    if (!locationReady) {
        Serial.println("Could not fetch postcode location.");
    }
}

void loop() {
    if (!locationReady) {
        delay(5000);
        return;
    }

    unsigned long now = millis();

    if (lastRefreshTime == 0 || now - lastRefreshTime >= REFRESH_INTERVAL_MS) {
        lastRefreshTime = now;

        Aircraft aircraftList[MAX_AIRCRAFT];

        int aircraftCount = fetchNearbyAircraft(
            userLatitude,
            userLongitude,
            aircraftList,
            MAX_AIRCRAFT
        );

        renderAircraftList(aircraftList, aircraftCount);
    }
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