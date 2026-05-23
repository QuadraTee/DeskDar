#include <Arduino.h>

#include "aircraft_metadata.h"

const int MAX_METADATA_QUEUE = 10;

String metadataQueue[MAX_METADATA_QUEUE];
int metadataQueueCount = 0;

bool isInQueue(const String& icao24) {
    for (int i = 0; i < metadataQueueCount; i++) {
        if (metadataQueue[i] == icao24) {
            return true;
        }
    }

    return false;
}

void queueMetadataLookup(const String& icao24) {
    if (icao24.length() == 0) {
        return;
    }

    if (isInQueue(icao24)) {
        return;
    }

    if (metadataQueueCount >= MAX_METADATA_QUEUE) {
        return;
    }

    metadataQueue[metadataQueueCount] = icao24;
    metadataQueueCount++;

    Serial.print("Queued metadata lookup: ");
    Serial.println(icao24);
}

void enrichAircraftMetadata(Aircraft& aircraft) {
    aircraft.registration = "Unknown";
    aircraft.aircraftModel = "Unknown";
    aircraft.aircraftType = "Unknown";

    if (aircraft.icao24 == "ab8fcb") {
        aircraft.registration = "N844MC";
        aircraft.aircraftModel = "Boeing 787-9 Dreamliner";
        aircraft.aircraftType = "Wide-body Airliner";
        return;
    }

    queueMetadataLookup(aircraft.icao24);
}

void processMetadataQueue() {
    if (metadataQueueCount == 0) {
        return;
    }

    String icao24 = metadataQueue[0];

    Serial.print("Metadata queued for later lookup: ");
    Serial.println(icao24);

    for (int i = 1; i < metadataQueueCount; i++) {
        metadataQueue[i - 1] = metadataQueue[i];
    }

    metadataQueueCount--;
}