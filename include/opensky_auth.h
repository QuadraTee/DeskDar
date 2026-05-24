#pragma once

#include <Arduino.h>

bool fetchOpenSkyToken(
    const String& clientId,
    const String& clientSecret,
    String& accessToken
);