#include "aircraft_metadata.h"

void enrichAircraftMetadata(Aircraft& aircraft) {
    aircraft.registration = "Unknown";
    aircraft.aircraftModel = "Unknown";
    aircraft.aircraftType = "Unknown";

    // Temporary local metadata examples.
    // Full live metadata lookup will be added later via cache/proxy.

    if (aircraft.icao24 == "ab8fcb") {
        aircraft.registration = "N844MC";
        aircraft.aircraftModel = "Boeing 787-9 Dreamliner";
        aircraft.aircraftType = "Wide-body Airliner";
    }
}