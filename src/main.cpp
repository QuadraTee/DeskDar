#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "aircraft.h"
#include "opensky_client.h"
#include "postcode_client.h"
#include "display.h"
#include "aircraft_metadata.h"
#include "opensky_auth.h"
#include "secrets.h"

WebServer server(80);

String debugLog = "";
const int MAX_DEBUG_LOG_LENGTH = 4000;

void addDebugLog(const String& message) {
    Serial.println(message);

    debugLog += message;
    debugLog += "\n";

    if (debugLog.length() > MAX_DEBUG_LOG_LENGTH) {
        debugLog = debugLog.substring(
            debugLog.length() - MAX_DEBUG_LOG_LENGTH
        );
    }
}

const unsigned long AIRCRAFT_REFRESH_MS = 60000;
const unsigned long METADATA_REFRESH_MS = 20000;
const unsigned long MIN_GAP_BETWEEN_NETWORK_TASKS_MS = 15000;

unsigned long lastAircraftRefresh = 0;
unsigned long lastMetadataRefresh = 0;

float userLatitude = 0.0;
float userLongitude = 0.0;
bool locationReady = false;

String openSkyToken;

bool metadataTaskRunning = false;

void connectWiFi();

void metadataTask(void* parameter) {
    metadataTaskRunning = true;

    addDebugLog("Metadata task started");
    processMetadataQueue();
    addDebugLog("Metadata task finished");

    metadataTaskRunning = false;

    vTaskDelete(NULL);
}

void handleDebugPage() {
    String html = "";

    html += "<!DOCTYPE html><html><head>";
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<title>DeskDar Debug</title>";
    html += "</head><body>";
    html += "<h1>DeskDar Debug</h1>";

    html += "<p><strong>WiFi:</strong> Connected</p>";
    html += "<p><strong>IP:</strong> ";
    html += WiFi.localIP().toString();
    html += "</p>";

    html += "<p><strong>Free heap:</strong> ";
    html += String(ESP.getFreeHeap());
    html += " bytes</p>";

    html += "<p><strong>Location ready:</strong> ";
    html += locationReady ? "Yes" : "No";
    html += "</p>";

    html += "<p><strong>Latitude:</strong> ";
    html += String(userLatitude, 6);
    html += "</p>";

    html += "<p><strong>Longitude:</strong> ";
    html += String(userLongitude, 6);
    html += "</p>";

    html += "<h2>Logs</h2>";
    html += "<pre>";
    html += debugLog;
    html += "</pre>";

    html += "</body></html>";

    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    addDebugLog("DeskDar starting...");

    connectWiFi();

    server.on("/", handleDebugPage);
    server.on("/debug", handleDebugPage);

    server.on("/favicon.ico", []() {
        server.send(204);
    });

    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
    });

    server.begin();

    addDebugLog("Debug web server started");
    addDebugLog("Open: http://" + WiFi.localIP().toString());

    if (!fetchOpenSkyToken(openSkyToken)) {
        addDebugLog("Could not authenticate with OpenSky.");
    } else {
        addDebugLog("OpenSky token received.");
    }

    locationReady = fetchPostcode(
        POSTCODE,
        userLatitude,
        userLongitude
    );

    if (!locationReady) {
        addDebugLog("Could not fetch postcode location.");
    } else {
        addDebugLog("Postcode location ready.");
        addDebugLog("Latitude: " + String(userLatitude, 6));
        addDebugLog("Longitude: " + String(userLongitude, 6));
    }

    lastMetadataRefresh = millis();
}

void loop() {
    server.handleClient();

    if (!locationReady) {
        delay(5000);
        return;
    }

    unsigned long now = millis();

    bool aircraftDue =
        lastAircraftRefresh == 0 ||
        now - lastAircraftRefresh >= AIRCRAFT_REFRESH_MS;

    bool enoughGapAfterAircraft =
        now - lastAircraftRefresh >= MIN_GAP_BETWEEN_NETWORK_TASKS_MS;

    bool enoughGapBeforeAircraft =
        AIRCRAFT_REFRESH_MS - (now - lastAircraftRefresh) >= MIN_GAP_BETWEEN_NETWORK_TASKS_MS;

    bool metadataDue =
        now - lastMetadataRefresh >= METADATA_REFRESH_MS;

    if (aircraftDue) {
        lastAircraftRefresh = now;

        addDebugLog("Fetching aircraft from OpenSky...");

        Aircraft aircraftList[MAX_AIRCRAFT];

        int aircraftCount = fetchNearbyAircraft(
            userLatitude,
            userLongitude,
            aircraftList,
            MAX_AIRCRAFT,
            openSkyToken
        );

        if (aircraftCount == -1) {
            addDebugLog("OpenSky token expired. Refreshing token...");

            if (fetchOpenSkyToken(openSkyToken)) {
                addDebugLog("OpenSky token refreshed.");
            } else {
                addDebugLog("OpenSky token refresh failed.");
            }

            return;
        }

        addDebugLog("Aircraft fetched: " + String(aircraftCount));

        renderAircraftList(aircraftList, aircraftCount);
        renderAsciiRadar(aircraftList, aircraftCount);
    }
    else if (
        !metadataTaskRunning &&
        metadataDue &&
        enoughGapAfterAircraft &&
        enoughGapBeforeAircraft
    ) {
        lastMetadataRefresh = now;

        addDebugLog("Starting metadata lookup task");

        xTaskCreatePinnedToCore(
            metadataTask,
            "MetadataTask",
            16384,
            NULL,
            1,
            NULL,
            0
        );
    }
}

void connectWiFi() {
    addDebugLog("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    addDebugLog("WiFi connected!");
    addDebugLog("IP address: " + WiFi.localIP().toString());
}