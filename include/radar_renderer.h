#pragma once

#include <Arduino.h>
#include "aircraft.h"

void updateRadarSweep(unsigned long now);
float getRadarSweepAngleDegrees();

void renderRadarBackground();
void renderRadarSweep();
void renderAircraft(const Aircraft& aircraft);
void renderRadarFrame(const Aircraft aircraftList[], int aircraftCount);

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
);

int getAircraftFadePercent(const Aircraft& aircraft, unsigned long maxAgeMs);
