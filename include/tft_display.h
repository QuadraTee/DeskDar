#pragma once

#include <Arduino.h>
#include "aircraft.h"
#include "config_manager.h"

void initTftDisplay();
void showTftSetupPortalScreen();
void showTftAppSetupIncompleteScreen(const String& ipAddress);
void updateTftDisplay(
    const Aircraft aircraftList[],
    int aircraftCount,
    bool locationReady,
    bool appConfigReady,
    float radarRangeKm,
    float userLatitude,
    float userLongitude,
    const DeskDarConfig& config,
    const String& statusLine,
    const String& ipAddress
);
