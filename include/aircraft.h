#pragma once

#include <Arduino.h>



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

    String compassDirection;

    bool onGround;
};