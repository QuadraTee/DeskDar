#pragma once

#include "aircraft.h"

const int MAX_AIRCRAFT = 25;

extern float searchRadiusKm;

void setSearchRadiusKm(float radiusKm);
float getSearchRadiusKm();

int fetchNearbyAircraft(
    float latitude,
    float longitude,
    Aircraft aircraftList[],
    int maxAircraft,
    const String& accessToken
);