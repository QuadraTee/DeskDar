#pragma once

#include "aircraft.h"

void renderAircraftSummary(const Aircraft& aircraft, int position);
void renderAircraftList(const Aircraft aircraftList[], int aircraftCount);
void renderAsciiRadar(const Aircraft aircraftList[], int aircraftCount);