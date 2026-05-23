#include <Arduino.h>
#include "display.h"

void renderAircraftSummary(const Aircraft& aircraft, int position) {
    Serial.println();

    Serial.print("#");
    Serial.println(position);

    Serial.println("--------------------");

    Serial.print("Flight: ");
    Serial.println(aircraft.callsign);

    Serial.print("Country: ");
    Serial.println(aircraft.originCountry);

    Serial.print("Distance: ");
    Serial.print(aircraft.distanceKm, 1);
    Serial.println(" km");

    Serial.print("Direction: ");
    Serial.print(aircraft.compassDirection);
    Serial.print(" / ");
    Serial.print(aircraft.bearingDegrees, 0);
    Serial.println(" deg");

    Serial.print("Radar XY: ");
    Serial.print(aircraft.radarX);
    Serial.print(", ");
    Serial.println(aircraft.radarY);

    Serial.print("Heading XY: ");
    Serial.print(aircraft.headingX);
    Serial.print(", ");
    Serial.println(aircraft.headingY);

    Serial.print("Altitude: ");
    Serial.print(aircraft.altitudeFeet, 0);
    Serial.println(" ft");

    Serial.print("Speed: ");
    Serial.print(aircraft.speedKnots, 0);
    Serial.println(" kt");

    Serial.print("Heading: ");
    Serial.print(aircraft.headingDegrees, 0);
    Serial.println(" deg");

    Serial.print("Vertical rate: ");

    if (aircraft.verticalRate > 0) {
        Serial.println("climbing");
    } else if (aircraft.verticalRate < 0) {
        Serial.println("descending");
    } else {
        Serial.println("level");
    }
}

void renderAircraftList(const Aircraft aircraftList[], int aircraftCount) {
    Serial.println();
    Serial.println("Closest aircraft first:");
    Serial.println("====================");

    for (int i = 0; i < aircraftCount; i++) {
        renderAircraftSummary(aircraftList[i], i + 1);
    }
}