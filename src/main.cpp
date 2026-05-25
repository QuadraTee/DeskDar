#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

#include "aircraft.h"
#include "opensky_client.h"
#include "postcode_client.h"
#include "display.h"
#include "radar_renderer.h"
#include "aircraft_metadata.h"
#include "opensky_auth.h"
#include "config_manager.h"
#include "setup_portal.h"

DeskDarConfig config;
WebServer server(80);

String debugLog = "";
const int MAX_DEBUG_LOG_LENGTH = 8000;

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SECONDS = 0;
const int DAYLIGHT_OFFSET_SECONDS = 3600;

const unsigned long AIRCRAFT_REFRESH_MS = 30000;
const unsigned long METADATA_REFRESH_MS = 5000;
const unsigned long MIN_GAP_BETWEEN_NETWORK_TASKS_MS = 8000;

unsigned long lastAircraftRefresh = 0;
unsigned long lastMetadataRefresh = 0;

float userLatitude = 0.0;
float userLongitude = 0.0;
bool locationReady = false;
bool appConfigReady = false;

String openSkyToken;

bool metadataTaskRunning = false;

const unsigned long FRAME_INTERVAL_MS = 33;
const unsigned long AIRCRAFT_FADE_MS = 60000;
unsigned long lastFrameTime = 0;

Aircraft currentAircraftList[MAX_AIRCRAFT];
int currentAircraftCount = 0;

bool connectWiFi();
bool initialiseDeskDarServices();

String htmlEscape(const String& value) {
    String escaped = value;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

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
        line += " deg | Fade ";
        line += String(getAircraftFadePercent(aircraftList[i], AIRCRAFT_FADE_MS));
        line += "%";

        addDebugLog(line);
    }
}

void updatePredictedAircraftPositions() {
    for (int i = 0; i < currentAircraftCount; i++) {
        float predictedDistanceKm = 0.0;
        float predictedBearingDegrees = 0.0;

        predictAircraftRadarPosition(
            currentAircraftList[i],
            userLatitude,
            userLongitude,
            getSearchRadiusKm(),
            120,
            120,
            110,
            8,
            currentAircraftList[i].predictedRadarX,
            currentAircraftList[i].predictedRadarY,
            currentAircraftList[i].predictedHeadingX,
            currentAircraftList[i].predictedHeadingY,
            predictedDistanceKm,
            predictedBearingDegrees
        );
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

String buildPageHeader(const String& title, const String& activeTab) {
    String html = "";

    html += "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>";
    html += htmlEscape(title);
    html += "</title>";
    html += R"rawliteral(
<style>
body {
    margin: 0;
    font-family: Arial, sans-serif;
    background: #081008;
    color: #e8ffe8;
}
header {
    background: #111b11;
    border-bottom: 1px solid #245c24;
    padding: 16px 20px;
}
h1 {
    margin: 0 0 12px 0;
    color: #80ff80;
}
nav a {
    color: #c8ffc8;
    text-decoration: none;
    margin-right: 8px;
    padding: 8px 12px;
    border-radius: 6px;
    display: inline-block;
}
nav a.active {
    background: #1d7a1d;
    color: white;
}
main {
    padding: 20px;
}
.card {
    background: #111b11;
    border: 1px solid #245c24;
    border-radius: 12px;
    padding: 16px;
    margin-bottom: 16px;
}
.grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 12px;
}
label {
    display: block;
    margin-top: 12px;
    font-weight: bold;
}
input {
    width: 100%;
    padding: 10px;
    margin-top: 6px;
    box-sizing: border-box;
    background: #061006;
    color: #e8ffe8;
    border: 1px solid #397a39;
    border-radius: 6px;
}
button {
    margin-top: 16px;
    padding: 12px 18px;
    background: #18a118;
    color: white;
    border: 0;
    border-radius: 6px;
    font-weight: bold;
}
button.danger {
    background: #9b2222;
}
.notice {
    background: #2b2206;
    border: 1px solid #d6a300;
    color: #ffe7a3;
    border-radius: 8px;
    padding: 12px;
    margin-bottom: 16px;
}
.logbox {
    background: #111;
    color: #0f0;
    padding: 10px;
    height: 420px;
    overflow-y: scroll;
    font-family: monospace;
    white-space: pre-wrap;
    border: 1px solid #444;
    border-radius: 8px;
}
.small {
    color: #a9cfa9;
    font-size: 0.95em;
}
</style>
)rawliteral";
    html += "</head><body>";
    html += "<header>";
    html += "<h1>DeskDar</h1>";
    html += "<nav>";

    html += "<a href='/'";
    if (activeTab == "dashboard") html += " class='active'";
    html += ">Dashboard</a>";

    html += "<a href='/logs-page'";
    if (activeTab == "logs") html += " class='active'";
    html += ">Logs</a>";

    html += "<a href='/settings'";
    if (activeTab == "settings") html += " class='active'";
    html += ">Settings</a>";

    html += "<a href='/aircraft'";
    if (activeTab == "aircraft") html += " class='active'";
    html += ">Aircraft</a>";

    html += "<a href='/system'";
    if (activeTab == "system") html += " class='active'";
    html += ">System</a>";

    html += "</nav>";
    html += "</header><main>";

    return html;
}

String buildPageFooter() {
    return "</main></body></html>";
}

void appendSetupNotice(String& html) {
    if (!appConfigReady) {
        html += "<div class='notice'>";
        html += "<strong>Setup incomplete.</strong> Enter your postcode and OpenSky credentials in Settings to start live aircraft tracking.";
        html += "</div>";
    }
}

void appendRadarRangeControl(String& html) {
    html += "<div class='card'>";
    html += "<h2>Radar Range</h2>";
    html += "<input type='range' min='5' max='100' value='";
    html += String(getSearchRadiusKm(), 0);
    html += "' id='radiusSlider' oninput='updateRadius(this.value)'> ";

    html += "<p><strong><span id='radiusValue'>";
    html += String(getSearchRadiusKm(), 0);
    html += "</span> km</strong></p>";

    html += R"rawliteral(
<script>
function updateRadius(value) {
    document.getElementById('radiusValue').innerText = value;

    fetch('/set-radius?value=' + value)
        .catch(error => console.log('Radius update failed', error));
}
</script>
)rawliteral";
    html += "</div>";
}

void appendLogsBox(String& html) {
    html += R"rawliteral(
<div class='card'>
<h2>Live Logs</h2>
<div id="logBox" class="logbox">
)rawliteral";

    html += debugLog;

    html += R"rawliteral(
</div>
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
}

void handleDashboardPage() {
    String html = buildPageHeader("DeskDar Dashboard", "dashboard");

    appendSetupNotice(html);

    html += "<div class='grid'>";

    html += "<div class='card'><h2>Network</h2>";
    html += "<p><strong>WiFi:</strong> Connected</p>";
    html += "<p><strong>IP:</strong> ";
    html += WiFi.localIP().toString();
    html += "</p></div>";

    html += "<div class='card'><h2>Radar</h2>";
    html += "<p><strong>Range:</strong> ";
    html += String(getSearchRadiusKm(), 0);
    html += " km</p>";
    html += "<p><strong>Current aircraft:</strong> ";
    html += String(currentAircraftCount);
    html += "</p>";
    html += "<p><strong>Sweep:</strong> ";
    html += String(getRadarSweepAngleDegrees(), 0);
    html += " deg</p></div>";

    html += "<div class='card'><h2>Location</h2>";
    html += "<p><strong>Ready:</strong> ";
    html += locationReady ? "Yes" : "No";
    html += "</p>";
    html += "<p><strong>Postcode:</strong> ";
    html += htmlEscape(config.postcode);
    html += "</p>";
    html += "<p><strong>Latitude:</strong> ";
    html += String(userLatitude, 6);
    html += "</p>";
    html += "<p><strong>Longitude:</strong> ";
    html += String(userLongitude, 6);
    html += "</p></div>";

    html += "<div class='card'><h2>System</h2>";
    html += "<p><strong>Free heap:</strong> ";
    html += String(ESP.getFreeHeap());
    html += " bytes</p>";
    html += "<p><strong>App config:</strong> ";
    html += appConfigReady ? "Complete" : "Incomplete";
    html += "</p></div>";

    html += "</div>";

    appendRadarRangeControl(html);
    appendLogsBox(html);

    html += buildPageFooter();

    server.send(200, "text/html", html);
}

void handleLogsPage() {
    String html = buildPageHeader("DeskDar Logs", "logs");
    appendSetupNotice(html);
    appendLogsBox(html);
    html += buildPageFooter();

    server.send(200, "text/html", html);
}

void handleSettingsPage() {
    String html = buildPageHeader("DeskDar Settings", "settings");

    appendSetupNotice(html);

    html += "<div class='card'>";
    html += "<h2>Aircraft Data Settings</h2>";
    html += "<p class='small'>These settings are entered after DeskDar is connected to your home WiFi. They are saved on the ESP32.</p>";

    html += "<form method='POST' action='/save-settings'>";

    html += "<label>Postcode</label>";
    html += "<input name='postcode' value='";
    html += htmlEscape(config.postcode);
    html += "' required>";

    html += "<label>OpenSky Client ID</label>";
    html += "<input name='opensky_client_id' value='";
    html += htmlEscape(config.openSkyClientId);
    html += "' required>";

    html += "<label>OpenSky Client Secret</label>";
    html += "<input name='opensky_client_secret' type='password' value='";
    html += htmlEscape(config.openSkyClientSecret);
    html += "' required>";

    html += "<button type='submit'>Save Settings & Restart</button>";
    html += "</form>";
    html += "</div>";

    appendRadarRangeControl(html);

    html += "<div class='card'>";
    html += "<h2>WiFi Setup</h2>";
    html += "<p class='small'>Use this if you need to move DeskDar to another WiFi network. It clears saved settings and restarts into the DeskDar Setup hotspot.</p>";
    html += "<form method='POST' action='/reset-config' onsubmit=\"return confirm('Clear saved settings and restart into setup mode?');\">";
    html += "<button class='danger' type='submit'>Reset Setup</button>";
    html += "</form>";
    html += "</div>";

    html += buildPageFooter();

    server.send(200, "text/html", html);
}

void handleSaveSettings() {
    String postcode = server.arg("postcode");
    String openSkyClientId = server.arg("opensky_client_id");
    String openSkyClientSecret = server.arg("opensky_client_secret");

    saveAppConfig(
        postcode,
        openSkyClientId,
        openSkyClientSecret
    );

    server.send(
        200,
        "text/html",
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head>"
        "<body style='font-family:Arial,sans-serif;background:#081008;color:#e8ffe8;padding:24px;'>"
        "<h1>DeskDar settings saved</h1>"
        "<p>Device will restart and begin radar mode.</p>"
        "</body></html>"
    );

    delay(2000);
    ESP.restart();
}

void handleResetConfig() {
    clearConfig();

    server.send(
        200,
        "text/html",
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head>"
        "<body style='font-family:Arial,sans-serif;background:#081008;color:#e8ffe8;padding:24px;'>"
        "<h1>DeskDar setup reset</h1>"
        "<p>Device will restart into setup mode.</p>"
        "</body></html>"
    );

    delay(2000);
    ESP.restart();
}

void handleAircraftPage() {
    String html = buildPageHeader("DeskDar Aircraft", "aircraft");

    appendSetupNotice(html);

    html += "<div class='card'>";
    html += "<h2>Current Aircraft</h2>";

    if (currentAircraftCount == 0) {
        html += "<p>No aircraft currently loaded.</p>";
    } else {
        html += "<div class='logbox'>";
        for (int i = 0; i < currentAircraftCount; i++) {
            html += "#";
            html += String(i + 1);
            html += " ";
            html += htmlEscape(currentAircraftList[i].callsign);
            html += " | ICAO24: ";
            html += htmlEscape(currentAircraftList[i].icao24);
            html += " | ";
            html += htmlEscape(currentAircraftList[i].registration);
            html += " | ";
            html += htmlEscape(currentAircraftList[i].aircraftModel);
            html += " | ";
            html += String(currentAircraftList[i].distanceKm, 1);
            html += " km ";
            html += htmlEscape(currentAircraftList[i].compassDirection);
            html += " | ";
            html += String(currentAircraftList[i].altitudeFeet, 0);
            html += " ft | ";
            html += String(currentAircraftList[i].speedKnots, 0);
            html += " kt | Fade ";
            html += String(getAircraftFadePercent(currentAircraftList[i], AIRCRAFT_FADE_MS));
            html += "%\n";
        }
        html += "</div>";
    }

    html += "</div>";
    html += buildPageFooter();

    server.send(200, "text/html", html);
}

void handleSystemPage() {
    String html = buildPageHeader("DeskDar System", "system");

    html += "<div class='grid'>";

    html += "<div class='card'><h2>Firmware</h2>";
    html += "<p><strong>Version:</strong> ";
    html += "v0.8-dashboard-settings";
    html += "</p></div>";

    html += "<div class='card'><h2>Memory</h2>";
    html += "<p><strong>Free heap:</strong> ";
    html += String(ESP.getFreeHeap());
    html += " bytes</p></div>";

    html += "<div class='card'><h2>Config</h2>";
    html += "<p><strong>WiFi config:</strong> ";
    html += hasWiFiConfig(config) ? "Saved" : "Missing";
    html += "</p>";
    html += "<p><strong>App config:</strong> ";
    html += appConfigReady ? "Complete" : "Incomplete";
    html += "</p></div>";

    html += "</div>";
    html += buildPageFooter();

    server.send(200, "text/html", html);
}

void handleLogs() {
    server.send(200, "text/plain", debugLog);
}

void handleSetRadius() {
    if (!server.hasArg("value")) {
        server.send(400, "text/plain", "Missing value");
        return;
    }

    float radius = server.arg("value").toFloat();

    setSearchRadiusKm(radius);

    addDebugLog("Radar range changed to " + String(getSearchRadiusKm(), 0) + " km");

    server.send(200, "text/plain", String(getSearchRadiusKm(), 0));
}

bool initialiseDeskDarServices() {
    appConfigReady = hasAppConfig(config);

    if (!appConfigReady) {
        addDebugLog("DeskDar app setup incomplete. Open Settings to enter postcode and OpenSky credentials.");
        locationReady = false;
        return false;
    }

    if (!fetchOpenSkyToken(
        config.openSkyClientId,
        config.openSkyClientSecret,
        openSkyToken
    )) {
        addDebugLog("Could not authenticate with OpenSky.");
    } else {
        addDebugLog("OpenSky token received.");
    }

    locationReady = fetchPostcode(
        config.postcode.c_str(),
        userLatitude,
        userLongitude
    );

    if (!locationReady) {
        addDebugLog("Could not fetch postcode location.");
        return false;
    }

    addDebugLog("Postcode location ready.");
    addDebugLog("Latitude: " + String(userLatitude, 6));
    addDebugLog("Longitude: " + String(userLongitude, 6));

    lastMetadataRefresh = millis();

    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();

    if (!loadConfig(config)) {
        Serial.println("No saved WiFi config found.");
        startSetupPortal();
    }

    if (!connectWiFi()) {
        Serial.println("Could not connect to saved WiFi.");
        startSetupPortal();
    }

    setupTime();

    addDebugLog("DeskDar starting...");

    server.on("/", handleDashboardPage);
    server.on("/debug", handleDashboardPage);
    server.on("/logs-page", handleLogsPage);
    server.on("/settings", handleSettingsPage);
    server.on("/save-settings", HTTP_POST, handleSaveSettings);
    server.on("/reset-config", HTTP_POST, handleResetConfig);
    server.on("/aircraft", handleAircraftPage);
    server.on("/system", handleSystemPage);
    server.on("/logs", handleLogs);
    server.on("/set-radius", handleSetRadius);

    server.on("/favicon.ico", []() {
        server.send(204);
    });

    server.onNotFound([]() {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    });

    server.begin();

    addDebugLog("DeskDar dashboard started");
    addDebugLog("Open: http://" + WiFi.localIP().toString());

    initialiseDeskDarServices();
}

void loop() {
    server.handleClient();

    if (!appConfigReady || !locationReady) {
        delay(50);
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

        int aircraftCount = fetchNearbyAircraft(
            userLatitude,
            userLongitude,
            currentAircraftList,
            MAX_AIRCRAFT,
            openSkyToken
        );

        if (aircraftCount == -1) {
            addDebugLog("OpenSky token expired. Refreshing token...");

            if (fetchOpenSkyToken(
                config.openSkyClientId,
                config.openSkyClientSecret,
                openSkyToken
            )) {
                addDebugLog("OpenSky token refreshed.");
            } else {
                addDebugLog("OpenSky token refresh failed.");
            }

            return;
        }

        currentAircraftCount = aircraftCount;

        addDebugLog("Aircraft fetched: " + String(currentAircraftCount));
        addAircraftToDebugLog(currentAircraftList, currentAircraftCount);

        renderAircraftList(currentAircraftList, currentAircraftCount);
        renderAsciiRadar(currentAircraftList, currentAircraftCount);
        renderRadarFrame(currentAircraftList, currentAircraftCount);
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

    if (now - lastFrameTime >= FRAME_INTERVAL_MS) {
        lastFrameTime = now;
        updateRadarSweep(now);
        updatePredictedAircraftPositions();
    }
}

bool connectWiFi() {
    Serial.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(
        config.wifiSsid.c_str(),
        config.wifiPassword.c_str()
    );

    unsigned long startAttempt = millis();
    const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttempt >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println();
            Serial.println("WiFi connection timed out.");
            return false;
        }

        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    return true;
}
