#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

#include "aircraft.h"
#include "opensky_client.h"
#include "postcode_client.h"
#include "display.h"
#include "aircraft_metadata.h"
#include "opensky_auth.h"
#include "secrets.h"

WebServer server(80);

String debugLog = "";
const int MAX_DEBUG_LOG_LENGTH = 8000;

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SECONDS = 0;
const int DAYLIGHT_OFFSET_SECONDS = 3600;

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

String getCurrentTimeString() {
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        return "??:??:??";
    }

    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

    return String(buffer);
}

void addDebugLog(const String& message) {
    String line = "[" + getCurrentTimeString() + "] " + message;

    Serial.println(line);

    debugLog += line;
    debugLog += "\n";

    if (debugLog.length() > MAX_DEBUG_LOG_LENGTH) {
        debugLog = debugLog.substring(
            debugLog.length() - MAX_DEBUG_LOG_LENGTH
        );
    }
}

void setupTime() {
    configTime(
        GMT_OFFSET_SECONDS,
        DAYLIGHT_OFFSET_SECONDS,
        NTP_SERVER
    );

    Serial.print("Syncing time");

    struct tm timeinfo;

    while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    addDebugLog("Time synced");
}

void addAircraftToDebugLog(const Aircraft aircraftList[], int aircraftCount) {
    addDebugLog("Aircraft list:");

    for (int i = 0; i < aircraftCount; i++) {
        String line = "";

        line += "#";
        line += String(i + 1);
        line += " ";
        line += aircraftList[i].callsign;
        line += " | ICAO24: ";
        line += aircraftList[i].icao24;
        line += " | ";
        line += aircraftList[i].registration;
        line += " | ";
        line += aircraftList[i].aircraftModel;
        line += " | ";
        line += String(aircraftList[i].distanceKm, 1);
        line += " km ";
        line += aircraftList[i].compassDirection;
        line += " | ";
        line += String(aircraftList[i].altitudeFeet, 0);
        line += " ft | ";
        line += String(aircraftList[i].speedKnots, 0);
        line += " kt | Heading ";
        line += String(aircraftList[i].headingDegrees, 0);
        line += " deg";

        addDebugLog(line);
    }
}

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

    html += R"rawliteral(
<h2>Logs</h2>

<div
    id="logBox"
    style="
        background:#111;
        color:#0f0;
        padding:10px;
        height:400px;
        overflow-y:scroll;
        font-family:monospace;
        white-space:pre-wrap;
        border:1px solid #444;
    "
>
)rawliteral";

    html += debugLog;

    html += R"rawliteral(
</div>

<script>
const logBox = document.getElementById('logBox');

function isNearBottom() {
    return (
        logBox.scrollHeight -
        logBox.scrollTop -
        logBox.clientHeight
    ) < 50;
}

async function updateLogs() {
    const shouldAutoScroll = isNearBottom();

    try {
        const response = await fetch('/logs');
        const text = await response.text();

        logBox.textContent = text;

        if (shouldAutoScroll) {
            logBox.scrollTop = logBox.scrollHeight;
        }
    } catch (error) {
        console.log('Log update failed', error);
    }
}

setInterval(updateLogs, 3000);
updateLogs();
</script>

)rawliteral";

    html += "</body></html>";

    server.send(200, "text/html", html);
}

void handleLogs() {
    server.send(200, "text/plain", debugLog);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();

    connectWiFi();
    setupTime();

    addDebugLog("DeskDar starting...");

    server.on("/", handleDebugPage);
    server.on("/debug", handleDebugPage);
    server.on("/logs", handleLogs);

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
        addAircraftToDebugLog(aircraftList, aircraftCount);

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