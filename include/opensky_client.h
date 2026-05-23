#pragma once

#include "aircraft.h"

const int MAX_AIRCRAFT = 20;

int fetchNearbyAircraft(
    float latitude,
    float longitude,
    Aircraft aircraftList[],
    int maxAircraft
);