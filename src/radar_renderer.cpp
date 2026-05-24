#include <Arduino.h>
#include "radar_renderer.h"

static float radarSweepAngleDegrees = 0.0;
static unsigned long lastSweepUpdateMillis = 0;

static float rendererDegreesToRadians(float degrees) {
    return degrees * PI / 180.0;
}

static float rendererRadiansToDegrees(float radians) {
    return radians * 180.0 / PI;
}

static float calculateDistanceKmLocal(float lat1, float lon1, float lat2, float lon2) {
    const float earthRadiusKm = 6371.0;

    float dLat = rendererDegreesToRadians(lat2 - lat1);
    float dLon = rendererDegreesToRadians(lon2 - lon1);

    float a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(rendererDegreesToRadians(lat1)) *
        cos(rendererDegreesToRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return earthRadiusKm * c;
}

static float calculateBearingDegreesLocal(float lat1, float lon1, float lat2, float lon2) {
    float phi1 = rendererDegreesToRadians(lat1);
    float phi2 = rendererDegreesToRadians(lat2);
    float deltaLon = rendererDegreesToRadians(lon2 - lon1);

    float y = sin(deltaLon) * cos(phi2);
    float x =
        cos(phi1) * sin(phi2) -
        sin(phi1) * cos(phi2) * cos(deltaLon);

    float bearing = rendererRadiansToDegrees(atan2(y, x));

    if (bearing < 0) {
        bearing += 360.0;
    }

    return bearing;
}

void updateRadarSweep(unsigned long now) {
    if (lastSweepUpdateMillis == 0) {
        lastSweepUpdateMillis = now;
        return;
    }

    unsigned long elapsedMs = now - lastSweepUpdateMillis;
    lastSweepUpdateMillis = now;

    const float sweepDegreesPerSecond = 120.0;
    radarSweepAngleDegrees += sweepDegreesPerSecond * (elapsedMs / 1000.0);

    while (radarSweepAngleDegrees >= 360.0) {
        radarSweepAngleDegrees -= 360.0;
    }
}

float getRadarSweepAngleDegrees() {
    return radarSweepAngleDegrees;
}

int getAircraftFadePercent(const Aircraft& aircraft, unsigned long maxAgeMs) {
    unsigned long now = millis();
    unsigned long ageMs = now - aircraft.lastSeenMillis;

    if (ageMs >= maxAgeMs) {
        return 0;
    }

    return 100 - ((ageMs * 100) / maxAgeMs);
}

void predictAircraftRadarPosition(
    const Aircraft& aircraft,
    float originLatitude,
    float originLongitude,
    float maxRangeKm,
    int centerX,
    int centerY,
    int radarRadius,
    int arrowLength,
    int& predictedRadarX,
    int& predictedRadarY,
    int& predictedHeadingX,
    int& predictedHeadingY,
    float& predictedDistanceKm,
    float& predictedBearingDegrees
) {
    unsigned long now = millis();
    float elapsedSeconds = (now - aircraft.lastSeenMillis) / 1000.0;

    float distanceTravelledKm =
        (aircraft.speedMetersPerSecond * elapsedSeconds) / 1000.0;

    const float earthRadiusKm = 6371.0;

    float bearingRad = rendererDegreesToRadians(aircraft.headingDegrees);
    float lat1 = rendererDegreesToRadians(aircraft.latitude);
    float lon1 = rendererDegreesToRadians(aircraft.longitude);
    float angularDistance = distanceTravelledKm / earthRadiusKm;

    float predictedLatRad = asin(
        sin(lat1) * cos(angularDistance) +
        cos(lat1) * sin(angularDistance) * cos(bearingRad)
    );

    float predictedLonRad = lon1 + atan2(
        sin(bearingRad) * sin(angularDistance) * cos(lat1),
        cos(angularDistance) - sin(lat1) * sin(predictedLatRad)
    );

    float predictedLatitude = rendererRadiansToDegrees(predictedLatRad);
    float predictedLongitude = rendererRadiansToDegrees(predictedLonRad);

    predictedDistanceKm = calculateDistanceKmLocal(
        originLatitude,
        originLongitude,
        predictedLatitude,
        predictedLongitude
    );

    predictedBearingDegrees = calculateBearingDegreesLocal(
        originLatitude,
        originLongitude,
        predictedLatitude,
        predictedLongitude
    );

    float clampedDistance = predictedDistanceKm;

    if (clampedDistance > maxRangeKm) {
        clampedDistance = maxRangeKm;
    }

    float distanceRatio = clampedDistance / maxRangeKm;
    float screenRadius = distanceRatio * radarRadius;
    float radarBearingRad = rendererDegreesToRadians(predictedBearingDegrees);

    predictedRadarX = centerX + sin(radarBearingRad) * screenRadius;
    predictedRadarY = centerY - cos(radarBearingRad) * screenRadius;

    float headingRad = rendererDegreesToRadians(aircraft.headingDegrees);
    predictedHeadingX = predictedRadarX + sin(headingRad) * arrowLength;
    predictedHeadingY = predictedRadarY - cos(headingRad) * arrowLength;
}

void renderRadarBackground() {
    // Future TFT renderer: draw black background, range rings, crosshair and bearing lines.
}

void renderRadarSweep() {
    // Future TFT renderer: draw rotating green sweep at getRadarSweepAngleDegrees().
}

void renderAircraft(const Aircraft& aircraft) {
    // Future TFT renderer: draw one aircraft blip, heading arrow, label and fade/trail state.
    (void)aircraft;
}

void renderRadarFrame(const Aircraft aircraftList[], int aircraftCount) {
    renderRadarBackground();
    renderRadarSweep();

    for (int i = 0; i < aircraftCount; i++) {
        renderAircraft(aircraftList[i]);
    }
}
