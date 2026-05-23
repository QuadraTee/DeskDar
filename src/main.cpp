#include <Arduino.h>
#include <WiFi.h>

#include "aircraft.h"
#include "opensky_client.h"
#include "postcode_client.h"
#include "display.h"
#include "aircraft_metadata.h"
#include "opensky_auth.h"
#include "secrets.h"



const unsigned long AIRCRAFT_REFRESH_MS = 60000;
const unsigned long METADATA_REFRESH_MS = 180000;

unsigned long lastAircraftRefresh = 0;
unsigned long lastMetadataRefresh = 0;

float userLatitude = 0.0;
float userLongitude = 0.0;
bool locationReady = false;


void connectWiFi();

String openSkyToken;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("DeskDar starting...");

    connectWiFi();


    if (!fetchOpenSkyToken(openSkyToken)) {
        Serial.println("Could not authenticate with OpenSky.");
    }

    locationReady = fetchPostcode(
        POSTCODE,
        userLatitude,
        userLongitude
    );

    if (!locationReady) {
    Serial.println("Could not fetch postcode location.");
}

lastMetadataRefresh = millis();

}

void loop() {
    if (!locationReady) {
        delay(5000);
        return;
    }

    unsigned long now = millis();
    bool aircraftFetchRan = false;

    if (
        lastAircraftRefresh == 0 ||
        now - lastAircraftRefresh >= AIRCRAFT_REFRESH_MS
    ) {
        lastAircraftRefresh = now;
        aircraftFetchRan = true;

        Aircraft aircraftList[MAX_AIRCRAFT];

        int aircraftCount = fetchNearbyAircraft(
            userLatitude,
            userLongitude,
            aircraftList,
            MAX_AIRCRAFT,
            openSkyToken
        );

        renderAircraftList(aircraftList, aircraftCount);
        renderAsciiRadar(aircraftList, aircraftCount);
    }

    if (
        !aircraftFetchRan &&
        now - lastMetadataRefresh >= METADATA_REFRESH_MS
    ) {
        lastMetadataRefresh = now;

        processMetadataQueue();
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