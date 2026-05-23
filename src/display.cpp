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

void renderAsciiRadar(const Aircraft aircraftList[], int aircraftCount) {
    const int width = 31;
    const int height = 15;

    char radar[height][width + 1];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            radar[y][x] = ' ';
        }
        radar[y][width] = '\0';
    }

    int centerX = width / 2;
    int centerY = height / 2;

    radar[centerY][centerX] = '+';

    for (int i = 0; i < aircraftCount; i++) {
        int x = map(aircraftList[i].radarX, 10, 230, 0, width - 1);
        int y = map(aircraftList[i].radarY, 10, 230, 0, height - 1);

        if (x >= 0 && x < width && y >= 0 && y < height) {
            radar[y][x] = '*';
        }
    }

    Serial.println();
    Serial.println("ASCII Radar Preview:");
    Serial.println("+-------------------------------+");

    for (int y = 0; y < height; y++) {
        Serial.print("|");
        Serial.print(radar[y]);
        Serial.println("|");
    }

    Serial.println("+-------------------------------+");
}