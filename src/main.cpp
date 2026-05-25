#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "aircraft.h"
#include "opensky_client.h"
#include "postcode_client.h"
#include "display.h"
#include "radar_renderer.h"
#include "aircraft_metadata.h"
#include "opensky_auth.h"
#include "config_manager.h"
#include "setup_portal.h"
#include "airport_landmarks.h"

DeskDarConfig config;
WebServer server(80);

const char* FIRMWARE_VERSION = "v0.20-large-radar-tab";
const char* UPDATE_VERSION_URL = "https://quadratee.github.io/DeskDar/latest.txt";
const char* UPDATE_FIRMWARE_URL = "https://quadratee.github.io/DeskDar/firmware.bin";

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

String jsonEscape(const String& value) {
    String escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    return escaped;
}

float normaliseDegrees(float degrees) {
    while (degrees < 0.0) {
        degrees += 360.0;
    }

    while (degrees >= 360.0) {
        degrees -= 360.0;
    }

    return degrees;
}



float dashboardDegreesToRadians(float degrees) {
    return degrees * PI / 180.0;
}

float dashboardDistanceKm(float lat1, float lon1, float lat2, float lon2) {
    const float earthRadiusKm = 6371.0;

    float dLat = dashboardDegreesToRadians(lat2 - lat1);
    float dLon = dashboardDegreesToRadians(lon2 - lon1);

    float a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(dashboardDegreesToRadians(lat1)) *
        cos(dashboardDegreesToRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return earthRadiusKm * c;
}

float dashboardBearingDegrees(float lat1, float lon1, float lat2, float lon2) {
    float phi1 = dashboardDegreesToRadians(lat1);
    float phi2 = dashboardDegreesToRadians(lat2);
    float deltaLon = dashboardDegreesToRadians(lon2 - lon1);

    float y = sin(deltaLon) * cos(phi2);
    float x =
        cos(phi1) * sin(phi2) -
        sin(phi1) * cos(phi2) * cos(deltaLon);

    return normaliseDegrees(atan2(y, x) * 180.0 / PI);
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

.setting-row {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 0;
}

.setting-row span {
    min-width: 180px;
    font-weight: 600;
}

.setting-row input {
    transform: scale(1.15);
}
.setting-row span {
    font-weight: 700;
}
.setting-row input[type="checkbox"] {
    width: auto;
    margin: 0;
    transform: scale(1.2);
}

.radar-page-card {
    max-width: 1100px;
    margin-left: auto;
    margin-right: auto;
}
.radar-large-wrap {
    display: flex;
    justify-content: center;
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

    html += "<a href='/radar'";
    if (activeTab == "radar") html += " class='active'";
    html += ">Radar</a>";

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

void appendBrowserRadar(String& html, bool largeView = false) {
    int canvasSize = largeView ? 900 : 520;
    int maxWidth = largeView ? 900 : 520;

    html += "<div class='card";
    if (largeView) {
        html += " radar-page-card";
    }
    html += "'>";

    if (largeView) {
        html += "<h2>Radar</h2>";
        html += "<p class='small'>Large live radar view. Aircraft movement is predicted between OpenSky updates and airports are shown using the current overlay settings.</p>";
    } else {
        html += "<h2>Browser Radar Preview</h2>";
        html += "<p class='small'>Prototype renderer for the future TFT radar. Sweep animation is handled locally in the browser for smoother movement.</p>";
    }

    html += "<div class='radar-large-wrap'>";
    html += "<canvas id='radarCanvas' width='";
    html += String(canvasSize);
    html += "' height='";
    html += String(canvasSize);
    html += "' style='width:100%;max-width:";
    html += String(maxWidth);
    html += "px;background:#020802;border:1px solid #245c24;border-radius:12px;display:block;'></canvas>";
    html += "</div>";
    html += "</div>";

    html += R"rawliteral(
<script>
const radarCanvas = document.getElementById('radarCanvas');
const radarContext = radarCanvas.getContext('2d');

let radarData = {
    rangeKm: 25,
    orientationDegrees: 0,
    aircraft: [],
    airports: [],
    airportSettings: {
        showAirports: true,
        showMajorAirports: true,
        showGaAirports: true
    }
};

const sweepDegreesPerSecond = 90;

function normaliseDegrees(value) {
    value = value % 360;
    if (value < 0) value += 360;
    return value;
}

function bearingToCanvasRadians(bearingDegrees) {
    return bearingDegrees * Math.PI / 180;
}

function drawRadarGrid(centerX, centerY, radius, orientationDegrees) {
    radarContext.fillStyle = '#020802';
    radarContext.fillRect(0, 0, radarCanvas.width, radarCanvas.height);

    radarContext.strokeStyle = 'rgba(0, 255, 80, 0.35)';
    radarContext.lineWidth = 1;

    for (let ring = 1; ring <= 4; ring++) {
        radarContext.beginPath();
        radarContext.arc(centerX, centerY, radius * ring / 4, 0, Math.PI * 2);
        radarContext.stroke();
    }

    for (let angle = 0; angle < 360; angle += 45) {
        const displayAngle = normaliseDegrees(angle - orientationDegrees);
        const radians = bearingToCanvasRadians(displayAngle);

        radarContext.beginPath();
        radarContext.moveTo(centerX, centerY);
        radarContext.lineTo(
            centerX + Math.sin(radians) * radius,
            centerY - Math.cos(radians) * radius
        );
        radarContext.stroke();
    }

    radarContext.strokeStyle = 'rgba(0, 255, 80, 0.7)';
    radarContext.beginPath();
    radarContext.arc(centerX, centerY, radius, 0, Math.PI * 2);
    radarContext.stroke();

    radarContext.fillStyle = 'rgba(0, 255, 80, 0.9)';
    radarContext.font = '12px monospace';

    const labels = [
        { text: 'N', angle: 0 },
        { text: 'E', angle: 90 },
        { text: 'S', angle: 180 },
        { text: 'W', angle: 270 }
    ];

    for (const label of labels) {
        const displayAngle = normaliseDegrees(label.angle - orientationDegrees);
        const radians = bearingToCanvasRadians(displayAngle);
        const x = centerX + Math.sin(radians) * (radius + 16);
        const y = centerY - Math.cos(radians) * (radius + 16);

        radarContext.fillText(label.text, x - 4, y + 4);
    }

    radarContext.fillStyle = 'rgba(180, 255, 180, 0.9)';
    radarContext.fillText('Facing ' + Math.round(orientationDegrees) + '°', 12, 20);
}

function drawSweep(centerX, centerY, radius, angleDegrees) {
    const radians = bearingToCanvasRadians(angleDegrees);

    const gradient = radarContext.createRadialGradient(
        centerX,
        centerY,
        0,
        centerX,
        centerY,
        radius
    );

    gradient.addColorStop(0, 'rgba(0, 255, 80, 0.25)');
    gradient.addColorStop(1, 'rgba(0, 255, 80, 0.02)');

    radarContext.save();
    radarContext.beginPath();
    radarContext.moveTo(centerX, centerY);
    radarContext.arc(
        centerX,
        centerY,
        radius,
        radians - 0.45,
        radians
    );
    radarContext.closePath();
    radarContext.fillStyle = gradient;
    radarContext.fill();

    radarContext.restore();
}

function predictAircraftPolar(aircraft) {
    const ageMs = Math.max(0, aircraft.ageMs || 0);
    const ageSeconds = ageMs / 1000;

    const startBearingRadians = bearingToCanvasRadians(aircraft.bearingDegrees || 0);
    let eastKm = Math.sin(startBearingRadians) * (aircraft.distanceKm || 0);
    let northKm = Math.cos(startBearingRadians) * (aircraft.distanceKm || 0);

    const headingRadians = bearingToCanvasRadians(aircraft.headingDegrees || 0);
    const speedKmPerSecond = (aircraft.speedKnots || 0) * 0.000514444;
    const travelledKm = speedKmPerSecond * ageSeconds;

    eastKm += Math.sin(headingRadians) * travelledKm;
    northKm += Math.cos(headingRadians) * travelledKm;

    const predictedDistanceKm = Math.sqrt(
        eastKm * eastKm +
        northKm * northKm
    );

    const predictedBearingDegrees = normaliseDegrees(
        Math.atan2(eastKm, northKm) * 180 / Math.PI
    );

    return {
        distanceKm: predictedDistanceKm,
        bearingDegrees: predictedBearingDegrees
    };
}


function drawAirport(airport, centerX, centerY, radius, rangeKm, orientationDegrees) {
    if (!airport || airport.distanceKm > rangeKm) {
        return;
    }

    const distanceRatio = Math.min(airport.distanceKm / rangeKm, 1);
    const displayBearing = normaliseDegrees(airport.bearingDegrees - orientationDegrees);
    const bearingRadians = bearingToCanvasRadians(displayBearing);

    const x = centerX + Math.sin(bearingRadians) * radius * distanceRatio;
    const y = centerY - Math.cos(bearingRadians) * radius * distanceRatio;

    const isMajor = airport.category === 'major';
    const markerSize = isMajor ? 5 : 3;

    radarContext.strokeStyle = isMajor ? 'rgba(80, 220, 255, 0.95)' : 'rgba(90, 170, 255, 0.75)';
    radarContext.fillStyle = isMajor ? 'rgba(80, 220, 255, 0.95)' : 'rgba(90, 170, 255, 0.75)';
    radarContext.lineWidth = 1;

    radarContext.beginPath();
    radarContext.moveTo(x - markerSize, y);
    radarContext.lineTo(x + markerSize, y);
    radarContext.moveTo(x, y - markerSize);
    radarContext.lineTo(x, y + markerSize);
    radarContext.stroke();

    if (isMajor || airport.distanceKm < 15) {
        radarContext.font = isMajor ? '11px monospace' : '10px monospace';
        radarContext.fillText(airport.ident, x + 6, y + 4);
    }
}

function drawAircraft(aircraft, centerX, centerY, radius, rangeKm, orientationDegrees) {
    const predicted = predictAircraftPolar(aircraft);

    if (predicted.distanceKm > rangeKm) {
        return;
    }

    const distanceRatio = Math.min(predicted.distanceKm / rangeKm, 1);
    const displayBearing = normaliseDegrees(predicted.bearingDegrees - orientationDegrees);
    const bearingRadians = bearingToCanvasRadians(displayBearing);

    const x = centerX + Math.sin(bearingRadians) * radius * distanceRatio;
    const y = centerY - Math.cos(bearingRadians) * radius * distanceRatio;

    const ageMs = Math.max(0, aircraft.ageMs || 0);
    const liveFadePercent = Math.max(0, 100 - ((ageMs / 60000) * 100));
    const alpha = Math.max(0.15, liveFadePercent / 100);

    radarContext.fillStyle = `rgba(0, 255, 80, ${alpha})`;
    radarContext.beginPath();
    radarContext.arc(x, y, 4, 0, Math.PI * 2);
    radarContext.fill();

    const displayHeading = normaliseDegrees(aircraft.headingDegrees - orientationDegrees);
    const headingRadians = bearingToCanvasRadians(displayHeading);

    radarContext.strokeStyle = `rgba(0, 255, 80, ${alpha})`;
    radarContext.beginPath();
    radarContext.moveTo(x, y);
    radarContext.lineTo(
        x + Math.sin(headingRadians) * 12,
        y - Math.cos(headingRadians) * 12
    );
    radarContext.stroke();

    radarContext.fillStyle = `rgba(180, 255, 180, ${alpha})`;
    radarContext.font = '11px monospace';

    const labelSettings = radarData.labelSettings || {};
    const labelLines = [];

    labelLines.push(aircraft.callsign || aircraft.registration || aircraft.icao24);

    if (labelSettings.registration && aircraft.registration && aircraft.registration !== 'Unknown') {
        labelLines.push(aircraft.registration);
    }

    if (labelSettings.model && aircraft.model && aircraft.model !== 'Unknown') {
        labelLines.push(aircraft.model);
    }

    if (labelSettings.type && aircraft.type && aircraft.type !== 'Unknown') {
        labelLines.push(aircraft.type);
    }

    if (labelSettings.distance) {
        labelLines.push((predicted.distanceKm || 0).toFixed(1) + ' km');
    }

    if (labelSettings.altitude) {
        labelLines.push(Math.round(aircraft.altitudeFeet || 0) + ' ft');
    }

    if (labelSettings.speed) {
        labelLines.push(Math.round(aircraft.speedKnots || 0) + ' kt');
    }

    if (labelSettings.heading) {
        labelLines.push('HDG ' + Math.round(aircraft.headingDegrees || 0) + '°');
    }

    for (let i = 0; i < labelLines.length; i++) {
        radarContext.fillText(labelLines[i], x + 7, y - 7 + (i * 12));
    }
}

async function fetchRadarData() {
    try {
        const response = await fetch('/aircraft.json');
        radarData = await response.json();
    } catch (error) {
        console.log('Radar data update failed', error);
    }
}

function drawBrowserRadarFrame(timestamp) {
    const centerX = radarCanvas.width / 2;
    const centerY = radarCanvas.height / 2;
    const radius = Math.min(centerX, centerY) - 34;
    const orientationDegrees = radarData.orientationDegrees || 0;

    drawRadarGrid(centerX, centerY, radius, orientationDegrees);

    const sweepAngle = normaliseDegrees(
        (timestamp / 1000) * sweepDegreesPerSecond
    );

    drawSweep(centerX, centerY, radius, sweepAngle);

    for (const airport of radarData.airports || []) {
        drawAirport(
            airport,
            centerX,
            centerY,
            radius,
            radarData.rangeKm || 25,
            orientationDegrees
        );
    }

    for (const aircraft of radarData.aircraft || []) {
        drawAircraft(
            aircraft,
            centerX,
            centerY,
            radius,
            radarData.rangeKm || 25,
            orientationDegrees
        );
    }

    requestAnimationFrame(drawBrowserRadarFrame);
}

setInterval(fetchRadarData, 3000);
fetchRadarData();
requestAnimationFrame(drawBrowserRadarFrame);
</script>
)rawliteral";
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


void handleRadarPage() {
    String html = buildPageHeader("DeskDar Radar", "radar");

    appendSetupNotice(html);
    appendBrowserRadar(html, true);

    html += buildPageFooter();

    server.send(200, "text/html", html);
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
    html += "<p><strong>Orientation:</strong> ";
    html += String(config.radarOrientationDegrees, 0);
    html += " deg</p>";
    html += "<p><strong>Current aircraft:</strong> ";
    html += String(currentAircraftCount);
    html += "</p>";
    html += "</div>";

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

    appendBrowserRadar(html);
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

    if (server.hasArg("saved")) {
        html += "<div class='notice'>Settings saved and applied without restart.</div>";
    }

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

    html += "<label>Radar Orientation / Facing Direction (degrees)</label>";
    html += "<input name='radar_orientation' type='number' min='0' max='359' step='1' value='";
    html += String(config.radarOrientationDegrees, 0);
    html += "'>";
    html += "<p class='small'>0 = north-up. Set this to the direction you face at your desk, for example 146 for south-east.</p>";

    html += "<h3>Radar Label Display</h3>";
    html += "<p class='small'>Choose what appears under each aircraft callsign on the browser radar.</p>";

    html += "<label class='setting-row'><span>Registration</span><input type='checkbox' name='show_registration' ";
    html += config.showLabelRegistration ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Aircraft model</span><input type='checkbox' name='show_model' ";
    html += config.showLabelModel ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Aircraft type</span><input type='checkbox' name='show_type' ";
    html += config.showLabelType ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Distance</span><input type='checkbox' name='show_distance' ";
    html += config.showLabelDistance ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Altitude</span><input type='checkbox' name='show_altitude' ";
    html += config.showLabelAltitude ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Speed</span><input type='checkbox' name='show_speed' ";
    html += config.showLabelSpeed ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Heading</span><input type='checkbox' name='show_heading' ";
    html += config.showLabelHeading ? "checked" : "";
    html += "></label>";


    html += "<h3>Airport Display</h3>";
    html += "<p class='small'>Show fixed UK airport and airfield markers on the browser radar.</p>";

    html += "<label class='setting-row'><span>Show Airports</span><input type='checkbox' name='show_airports' ";
    html += config.showAirports ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>Major Airports</span><input type='checkbox' name='show_major_airports' ";
    html += config.showMajorAirports ? "checked" : "";
    html += "></label>";

    html += "<label class='setting-row'><span>GA / Small Airfields</span><input type='checkbox' name='show_ga_airports' ";
    html += config.showGaAirports ? "checked" : "";
    html += "></label>";

    html += "<button type='submit'>Save Settings</button>";
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
    String oldPostcode = config.postcode;
    String oldOpenSkyClientId = config.openSkyClientId;
    String oldOpenSkyClientSecret = config.openSkyClientSecret;

    String postcode = server.arg("postcode");
    String openSkyClientId = server.arg("opensky_client_id");
    String openSkyClientSecret = server.arg("opensky_client_secret");
    float radarOrientation = normaliseDegrees(server.arg("radar_orientation").toFloat());

    config.postcode = postcode;
    config.openSkyClientId = openSkyClientId;
    config.openSkyClientSecret = openSkyClientSecret;
    config.radarOrientationDegrees = radarOrientation;

    config.showLabelRegistration = server.hasArg("show_registration");
    config.showLabelModel = server.hasArg("show_model");
    config.showLabelType = server.hasArg("show_type");
    config.showLabelDistance = server.hasArg("show_distance");
    config.showLabelAltitude = server.hasArg("show_altitude");
    config.showLabelSpeed = server.hasArg("show_speed");
    config.showLabelHeading = server.hasArg("show_heading");

    config.showAirports = server.hasArg("show_airports");
    config.showMajorAirports = server.hasArg("show_major_airports");
    config.showGaAirports = server.hasArg("show_ga_airports");

    saveConfig(config);

    addDebugLog("Settings saved without restart.");

    bool appSettingsChanged =
        oldPostcode != config.postcode ||
        oldOpenSkyClientId != config.openSkyClientId ||
        oldOpenSkyClientSecret != config.openSkyClientSecret;

    if (appSettingsChanged || !appConfigReady || !locationReady) {
        addDebugLog("Applying updated app settings...");
        initialiseDeskDarServices();
    }

    server.sendHeader("Location", "/settings?saved=1", true);
    server.send(302, "text/plain", "");
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

    if (server.hasArg("saved")) {
        html += "<div class='notice'>Settings saved and applied without restart.</div>";
    }

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




String getLatestFirmwareVersion() {
    if (WiFi.status() != WL_CONNECTED) {
        return "";
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(15000);
    https.useHTTP10(true);

    if (!https.begin(client, UPDATE_VERSION_URL)) {
        return "";
    }

    int httpCode = https.GET();

    if (httpCode != 200) {
        https.end();
        return "";
    }

    String latestVersion = https.getString();
    https.end();

    latestVersion.trim();
    return latestVersion;
}

bool downloadAndInstallLatestFirmware(String& message) {
    if (WiFi.status() != WL_CONNECTED) {
        message = "WiFi is not connected.";
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(60000);
    https.useHTTP10(true);

    if (!https.begin(client, UPDATE_FIRMWARE_URL)) {
        message = "Could not start firmware download.";
        return false;
    }

    int httpCode = https.GET();

    if (httpCode != 200) {
        message = "Firmware download failed. HTTP status: " + String(httpCode);
        https.end();
        return false;
    }

    int contentLength = https.getSize();

    if (contentLength <= 0) {
        message = "Firmware download has no valid content length.";
        https.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        message = "Could not begin firmware update.";
        Update.printError(Serial);
        https.end();
        return false;
    }

    WiFiClient* stream = https.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != (size_t)contentLength) {
        message = "Firmware write incomplete. Wrote " + String(written) + " of " + String(contentLength) + " bytes.";
        Update.printError(Serial);
        https.end();
        return false;
    }

    if (!Update.end()) {
        message = "Firmware update finalisation failed.";
        Update.printError(Serial);
        https.end();
        return false;
    }

    if (!Update.isFinished()) {
        message = "Firmware update did not finish cleanly.";
        https.end();
        return false;
    }

    https.end();

    message = "Firmware downloaded and installed successfully.";
    return true;
}

void handleUpdateCheck() {
    String latestTag = getLatestFirmwareVersion();

    String json = "{";
    json += "\"currentVersion\":\"";
    json += jsonEscape(FIRMWARE_VERSION);
    json += "\",";
    json += "\"latestVersion\":\"";
    json += jsonEscape(latestTag);
    json += "\",";
    json += "\"updateAvailable\":";
    json += (latestTag.length() > 0 && latestTag != String(FIRMWARE_VERSION)) ? "true" : "false";
    json += ",\"ok\":";
    json += latestTag.length() > 0 ? "true" : "false";
    json += "}";

    server.send(200, "application/json", json);
}


void handleInstallLatestUpdate() {
    String latestVersion = getLatestFirmwareVersion();

    if (latestVersion.length() == 0) {
        server.send(500, "application/json", "{\"ok\":false,\"message\":\"Could not check latest version.\"}");
        return;
    }

    if (latestVersion == String(FIRMWARE_VERSION)) {
        server.send(200, "application/json", "{\"ok\":true,\"message\":\"DeskDar is already up to date.\",\"restart\":false}");
        return;
    }

    addDebugLog("HTTP OTA update requested. Latest version: " + latestVersion);

    String message;
    bool success = downloadAndInstallLatestFirmware(message);

    if (!success) {
        addDebugLog("HTTP OTA update failed: " + message);

        String json = "{";
        json += "\"ok\":false,";
        json += "\"message\":\"";
        json += jsonEscape(message);
        json += "\"";
        json += "}";

        server.send(500, "application/json", json);
        return;
    }

    addDebugLog("HTTP OTA update complete. Restarting.");

    String json = "{";
    json += "\"ok\":true,";
    json += "\"message\":\"";
    json += jsonEscape(message);
    json += "\",";
    json += "\"restart\":true";
    json += "}";

    server.send(200, "application/json", json);

    delay(1500);
    ESP.restart();
}

void handleOtaUploadPage() {
    String html = buildPageHeader("DeskDar OTA Update", "system");

    html += R"rawliteral(
<div class='card'>
<h2>OTA Firmware Update</h2>
<p class='small'>Upload a compiled PlatformIO firmware binary. Usually this file is <code>.pio/build/esp32dev/firmware.bin</code>.</p>
<p class='small'><strong>Important:</strong> keep DeskDar powered on during the update. The device will restart automatically after a successful upload.</p>

<div class='notice' id='updateStatus'>Checking for updates...</div>

<div style='margin-top:12px;'>
    <button type='button' id='installLatestButton' disabled>Install Latest Update</button>
</div>
<p class='small'>Automatic update downloads <code>firmware.bin</code> from the DeskDar update server. Manual upload remains available below as a fallback.</p>

<form
    id='otaForm'
    method='POST'
    action='/ota-update'
    enctype='multipart/form-data'
>
    <label>Firmware .bin file</label>
    <input type='file' id='firmwareFile' name='firmware' accept='.bin' required>
    <button type='button' id='uploadButton'>Upload Firmware</button>
</form>

<div style='margin-top:16px;background:#061006;border:1px solid #397a39;border-radius:8px;overflow:hidden;'>
    <div id='otaProgressBar' style='height:22px;width:0%;background:#18a118;text-align:center;color:white;font-size:13px;line-height:22px;'>0%</div>
</div>

<div id='otaLog' class='logbox' style='height:220px;margin-top:16px;'></div>
</div>

<script>
const firmwareFile = document.getElementById('firmwareFile');
const uploadButton = document.getElementById('uploadButton');
const progressBar = document.getElementById('otaProgressBar');
const otaLog = document.getElementById('otaLog');
const updateStatus = document.getElementById('updateStatus');
const installLatestButton = document.getElementById('installLatestButton');

function addOtaLog(message) {
    const now = new Date().toLocaleTimeString();
    otaLog.textContent += '[' + now + '] ' + message + '\n';
    otaLog.scrollTop = otaLog.scrollHeight;
}

firmwareFile.addEventListener('change', function() {
    if (firmwareFile.files.length) {
        addOtaLog('Selected firmware: ' + firmwareFile.files[0].name);
    }
});

async function checkForUpdates() {
    try {
        const response = await fetch('/update-check');
        const data = await response.json();

        if (!data.ok) {
            updateStatus.innerHTML = 'Could not check for updates.';
            addOtaLog('Update check failed. You can still upload firmware manually.');
            return;
        }

        if (data.updateAvailable) {
            updateStatus.innerHTML = '<strong>Update available:</strong> ' + data.latestVersion + '<br>Current version: ' + data.currentVersion;
            installLatestButton.disabled = false;
            addOtaLog('New version available: ' + data.latestVersion);
        } else {
            updateStatus.innerHTML = 'DeskDar is up to date.<br>Current version: ' + data.currentVersion;
            installLatestButton.disabled = true;
            addOtaLog('No newer version found.');
        }
    } catch (error) {
        updateStatus.innerHTML = 'Could not check for updates.';
        addOtaLog('Update check error: ' + error);
    }
}


installLatestButton.addEventListener('click', async function() {
    installLatestButton.disabled = true;
    uploadButton.disabled = true;
    progressBar.style.width = '15%';
    progressBar.textContent = 'Starting...';

    addOtaLog('Starting automatic HTTP OTA update.');
    addOtaLog('Downloading latest firmware. Keep DeskDar powered on.');

    try {
        const response = await fetch('/install-latest', { method: 'POST' });
        const data = await response.json();

        if (!response.ok || !data.ok) {
            progressBar.style.width = '0%';
            progressBar.textContent = 'Failed';
            addOtaLog('Automatic update failed: ' + (data.message || response.status));
            uploadButton.disabled = false;
            await checkForUpdates();
            return;
        }

        progressBar.style.width = '100%';
        progressBar.textContent = '100%';
        addOtaLog(data.message || 'Automatic update installed.');
        addOtaLog('Device is restarting. Reconnect in around 10 seconds.');
    } catch (error) {
        progressBar.style.width = '0%';
        progressBar.textContent = 'Failed';
        addOtaLog('Automatic update error: ' + error);
        uploadButton.disabled = false;
        await checkForUpdates();
    }
});


uploadButton.addEventListener('click', function() {
    if (!firmwareFile.files.length) {
        addOtaLog('No firmware file selected. Choose firmware.bin first.');
        return;
    }

    const file = firmwareFile.files[0];
    const formData = new FormData();
    formData.append('firmware', file, file.name);

    const request = new XMLHttpRequest();

    uploadButton.disabled = true;
    progressBar.style.width = '0%';
    progressBar.textContent = '0%';
    addOtaLog('Starting OTA upload: ' + file.name);
    addOtaLog('Uploading firmware...');

    request.upload.addEventListener('progress', function(event) {
        if (event.lengthComputable) {
            const percent = Math.round((event.loaded / event.total) * 100);
            progressBar.style.width = percent + '%';
            progressBar.textContent = percent + '%';

            if (percent >= 100) {
                addOtaLog('Upload complete. Writing firmware to flash...');
            }
        }
    });

    request.addEventListener('load', function() {
        if (request.status >= 200 && request.status < 300) {
            progressBar.style.width = '100%';
            progressBar.textContent = '100%';
            addOtaLog('Firmware accepted by DeskDar.');
            addOtaLog('Device is restarting. Reconnect in around 10 seconds.');
        } else {
            uploadButton.disabled = false;
            addOtaLog('OTA update failed. HTTP status: ' + request.status);
            addOtaLog(request.responseText || 'No response body received.');
        }
    });

    request.addEventListener('error', function() {
        uploadButton.disabled = false;
        addOtaLog('OTA upload failed. Network connection was interrupted.');
    });

    request.open('POST', '/ota-update');
    request.send(formData);
});

addOtaLog('OTA page ready.');
checkForUpdates();
</script>
)rawliteral";

    html += buildPageFooter();
    server.send(200, "text/html", html);
}

void handleOtaUploadResult() {
    bool success = !Update.hasError();

    String html = buildPageHeader("DeskDar OTA Update", "system");

    html += "<div class='card'>";
    if (success) {
        html += "<h2>Update successful</h2>";
        html += "<p>DeskDar will restart now.</p>";
    } else {
        html += "<h2>Update failed</h2>";
        html += "<p>Firmware upload failed. Check the file and try again.</p>";
    }
    html += "</div>";

    html += buildPageFooter();

    server.send(success ? 200 : 500, "text/html", html);

    if (success) {
        addDebugLog("OTA update complete. Restarting.");
        delay(1500);
        ESP.restart();
    }
}

void handleOtaUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        addDebugLog("OTA update started: " + upload.filename);

        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            addDebugLog("OTA update begin failed.");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            addDebugLog("OTA update write failed.");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            addDebugLog("OTA update uploaded: " + String(upload.totalSize) + " bytes");
        } else {
            addDebugLog("OTA update end failed.");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        addDebugLog("OTA update aborted.");
    }
}


void handleSystemPage() {
    String html = buildPageHeader("DeskDar System", "system");

    html += "<div class='grid'>";

    html += "<div class='card'><h2>Firmware</h2>";
    html += "<p><strong>Version:</strong> ";
    html += FIRMWARE_VERSION;
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

    html += "<div class='card'><h2>OTA Update</h2>";
    html += "<p class='small'>Upload a new <code>firmware.bin</code> from your browser. Saved configuration is preserved.</p>";
    html += "<p><a style='color:#80ff80;font-weight:bold;' href='/ota'>Open OTA updater</a></p>";
    html += "</div>";

    html += "</div>";
    html += buildPageFooter();

    server.send(200, "text/html", html);
}

void handleLogs() {
    server.send(200, "text/plain", debugLog);
}

void handleAircraftJson() {
    String json = "";

    json += "{";
    json += "\"rangeKm\":";
    json += String(getSearchRadiusKm(), 1);
    json += ",";
    json += "\"orientationDegrees\":";
    json += String(config.radarOrientationDegrees, 1);
    json += ",";
    json += "\"labelSettings\":{";
    json += "\"registration\":";
    json += config.showLabelRegistration ? "true" : "false";
    json += ",\"model\":";
    json += config.showLabelModel ? "true" : "false";
    json += ",\"type\":";
    json += config.showLabelType ? "true" : "false";
    json += ",\"distance\":";
    json += config.showLabelDistance ? "true" : "false";
    json += ",\"altitude\":";
    json += config.showLabelAltitude ? "true" : "false";
    json += ",\"speed\":";
    json += config.showLabelSpeed ? "true" : "false";
    json += ",\"heading\":";
    json += config.showLabelHeading ? "true" : "false";
    json += "},";
    json += "\"airportSettings\":{";
    json += "\"showAirports\":";
    json += config.showAirports ? "true" : "false";
    json += ",\"showMajorAirports\":";
    json += config.showMajorAirports ? "true" : "false";
    json += ",\"showGaAirports\":";
    json += config.showGaAirports ? "true" : "false";
    json += "},";
    json += "\"airports\":[";

    bool firstAirport = true;

    if (locationReady && config.showAirports) {
        for (int i = 0; i < UK_AIRPORT_COUNT; i++) {
            const AirportLandmark& airport = UK_AIRPORTS[i];
            bool isMajor = airport.category == AIRPORT_MAJOR;
            bool isGa = airport.category == AIRPORT_GA;

            if ((isMajor && !config.showMajorAirports) || (isGa && !config.showGaAirports)) {
                continue;
            }

            float distanceKm = dashboardDistanceKm(
                userLatitude,
                userLongitude,
                airport.latitude,
                airport.longitude
            );

            if (distanceKm > getSearchRadiusKm()) {
                continue;
            }

            float bearingDegrees = dashboardBearingDegrees(
                userLatitude,
                userLongitude,
                airport.latitude,
                airport.longitude
            );

            if (!firstAirport) {
                json += ",";
            }

            firstAirport = false;

            json += "{";
            json += "\"ident\":\"";
            json += jsonEscape(String(airport.ident));
            json += "\",";
            json += "\"name\":\"";
            json += jsonEscape(String(airport.name));
            json += "\",";
            json += "\"category\":\"";
            json += isMajor ? "major" : "ga";
            json += "\",";
            json += "\"distanceKm\":";
            json += String(distanceKm, 2);
            json += ",";
            json += "\"bearingDegrees\":";
            json += String(bearingDegrees, 1);
            json += "}";
        }
    }

    json += "],";
    json += "\"aircraft\":[";

    for (int i = 0; i < currentAircraftCount; i++) {
        if (i > 0) {
            json += ",";
        }

        const Aircraft& aircraft = currentAircraftList[i];

        json += "{";
        json += "\"icao24\":\"";
        json += jsonEscape(aircraft.icao24);
        json += "\",";
        json += "\"callsign\":\"";
        json += jsonEscape(aircraft.callsign);
        json += "\",";
        json += "\"registration\":\"";
        json += jsonEscape(aircraft.registration);
        json += "\",";
        json += "\"model\":\"";
        json += jsonEscape(aircraft.aircraftModel);
        json += "\",";
        json += "\"type\":\"";
        json += jsonEscape(aircraft.aircraftType);
        json += "\",";
        json += "\"distanceKm\":";
        json += String(aircraft.distanceKm, 2);
        json += ",";
        json += "\"bearingDegrees\":";
        json += String(aircraft.bearingDegrees, 1);
        json += ",";
        json += "\"headingDegrees\":";
        json += String(aircraft.headingDegrees, 1);
        json += ",";
        json += "\"altitudeFeet\":";
        json += String(aircraft.altitudeFeet, 0);
        json += ",";
        json += "\"speedKnots\":";
        json += String(aircraft.speedKnots, 0);
        json += ",";
        json += "\"ageMs\":";
        json += String(millis() - aircraft.lastSeenMillis);
        json += ",";
        json += "\"fadePercent\":";
        json += String(getAircraftFadePercent(aircraft, AIRCRAFT_FADE_MS));
        json += "}";
    }

    json += "]";
    json += "}";

    server.send(200, "application/json", json);
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
    server.on("/radar", handleRadarPage);
    server.on("/logs-page", handleLogsPage);
    server.on("/settings", handleSettingsPage);
    server.on("/save-settings", HTTP_POST, handleSaveSettings);
    server.on("/reset-config", HTTP_POST, handleResetConfig);
    server.on("/aircraft", handleAircraftPage);
    server.on("/aircraft.json", handleAircraftJson);
    server.on("/system", handleSystemPage);
    server.on("/ota", HTTP_GET, handleOtaUploadPage);
    server.on("/update-check", HTTP_GET, handleUpdateCheck);
    server.on("/install-latest", HTTP_POST, handleInstallLatestUpdate);
    server.on(
        "/ota-update",
        HTTP_POST,
        handleOtaUploadResult,
        handleOtaUpload
    );
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
