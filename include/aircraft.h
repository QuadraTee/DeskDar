#pragma once



struct Aircraft {
    String icao24;
    String callsign;
    String originCountry;

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