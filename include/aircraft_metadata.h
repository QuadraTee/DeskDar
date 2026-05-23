#pragma once

#include <Arduino.h>
#include "aircraft.h"

void enrichAircraftMetadata(Aircraft& aircraft);

void queueMetadataLookup(const String& icao24);

void processMetadataQueue();