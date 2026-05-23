#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "aircraft.h"
#include "display.h"
#include "opensky_client.h"
#include "postcode_client.h"

const char* WIFI_SSID = "Vodafone3390-2.4G";
const char* WIFI_PASSWORD = "xZcdzCd6fRtcFTaK";

const char* POSTCODE = "WV95BL";

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

    if (fetchPostcode(POSTCODE, latitude, longitude)) {
        Aircraft aircraftList[MAX_AIRCRAFT];

        int aircraftCount = fetchNearbyAircraft(
            latitude,
            longitude,
            aircraftList,
            MAX_AIRCRAFT
        );

        renderAircraftList(aircraftList, aircraftCount);
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

