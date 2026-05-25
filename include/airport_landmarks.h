#pragma once

#include <Arduino.h>

enum AirportCategory : uint8_t {
    AIRPORT_MAJOR = 0,
    AIRPORT_GA = 1
};

struct AirportLandmark {
    const char* ident;
    const char* name;
    float latitude;
    float longitude;
    uint8_t category;
};

extern const AirportLandmark UK_AIRPORTS[];
extern const int UK_AIRPORT_COUNT;
