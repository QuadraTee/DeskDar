#pragma once

#include <Arduino.h>

const int AIRCRAFT_TRAIL_LENGTH = 6;

struct Aircraft {
    String icao24;
    String callsign;
    String originCountry;
    String registration;
    String aircraftModel;
    String aircraftType;

    float latitude;
    float longitude;

    float altitudeMeters;
    float altitudeFeet;

    float speedMetersPerSecond;
    float speedKnots;

    float headingDegrees;
    float verticalRate;
    float distanceKm;
    float bearingDegrees;

    int radarX;
    int radarY;

    int headingX;
    int headingY;

    int predictedRadarX;
    int predictedRadarY;
    int predictedHeadingX;
    int predictedHeadingY;

    int trailX[AIRCRAFT_TRAIL_LENGTH];
    int trailY[AIRCRAFT_TRAIL_LENGTH];
    int trailCount;

    unsigned long lastSeenMillis;

    String compassDirection;

    bool onGround;
};
