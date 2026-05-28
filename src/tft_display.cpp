#include "tft_display.h"

#if __has_include(<TFT_eSPI.h>)
#include <TFT_eSPI.h>
#include "airport_landmarks.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite radarSprite = TFT_eSprite(&tft);

static bool tftReady = false;
static bool spriteReady = false;
static int spriteDrawX = 0;
static int spriteDrawY = 0;
static int spriteSize = 0;
static int spriteColourDepth = 8;
static unsigned long lastTftRenderMs = 0;

// 10 FPS is a good starting point for ST7789 over SPI.
// Smooth enough for the sweep/prediction, but light enough for the ESP32.
static const unsigned long TFT_RENDER_INTERVAL_MS = 100;

static uint16_t radarGreen = TFT_GREEN;
static uint16_t brightGreen = TFT_GREEN;
static uint16_t dimGreen = 0x03E0;
static uint16_t gridGreen = 0x0200;
static uint16_t sweepGreen = 0x0400;
static uint16_t textGreen = TFT_GREEN;

static float tftSweepAngleDegrees = 0.0;
static unsigned long lastSweepUpdateMs = 0;
static const float TFT_SWEEP_DEGREES_PER_SECOND = 90.0;

static const int TFT_MAX_TRAILS = 30;
static const int TFT_TRAIL_POINTS = 18;
static const unsigned long TFT_TRAIL_SAMPLE_MS = 1000;
static const int TFT_TRAIL_MIN_SPACING_PX = 2;

struct TftTrailPoint {
    int x;
    int y;
    unsigned long timestamp;
};

struct TftAircraftTrail {
    String icao24;
    TftTrailPoint points[TFT_TRAIL_POINTS];
    int count;
    unsigned long lastSeenMs;
    bool activeThisFrame;
};

static TftAircraftTrail tftTrails[TFT_MAX_TRAILS];


static float degreesToRadiansTft(float degrees) {
    return degrees * PI / 180.0;
}

static float radiansToDegreesTft(float radians) {
    return radians * 180.0 / PI;
}

static float normaliseDegreesTft(float degrees) {
    while (degrees < 0.0) {
        degrees += 360.0;
    }

    while (degrees >= 360.0) {
        degrees -= 360.0;
    }

    return degrees;
}

static void updateTftSweep(unsigned long now) {
    if (lastSweepUpdateMs == 0) {
        lastSweepUpdateMs = now;
        return;
    }

    float elapsedSeconds = (now - lastSweepUpdateMs) / 1000.0;
    lastSweepUpdateMs = now;

    tftSweepAngleDegrees = normaliseDegreesTft(
        tftSweepAngleDegrees + TFT_SWEEP_DEGREES_PER_SECOND * elapsedSeconds
    );
}

static void drawCenteredTextSprite(
    TFT_eSprite& canvas,
    const String& text,
    int y,
    uint16_t colour,
    uint8_t size = 1
) {
    canvas.setTextSize(size);
    canvas.setTextColor(colour, TFT_BLACK);

    int16_t x = (canvas.width() - canvas.textWidth(text)) / 2;
    if (x < 0) {
        x = 0;
    }

    canvas.setCursor(x, y);
    for(int i=0,start=0,row=0;i<=text.length();i++){ if(i==text.length()||text[i]=='\n'){ canvas.setCursor(x, y + row*8); canvas.print(text.substring(start,i)); start=i+1; row++; }}
}
static String clippedTftText(String value, int maxChars) {
    value.trim();

    if (value.length() > maxChars) {
        value = value.substring(0, maxChars);
    }

    return value;
}

static void drawTftLabelLine(
    TFT_eSprite& canvas,
    const String& text,
    int x,
    int y,
    uint16_t colour
) {
    if (text.length() == 0) {
        return;
    }

    canvas.setTextSize(1);
    canvas.setTextColor(colour, TFT_BLACK);
    canvas.setCursor(x, y);
    for(int i=0,start=0,row=0;i<=text.length();i++){ if(i==text.length()||text[i]=='\n'){ canvas.setCursor(x, y + row*8); canvas.print(text.substring(start,i)); start=i+1; row++; }}
}

static void appendLabelPart(String& line, const String& part) {
    if (part.length() == 0) {
        return;
    }

    if (line.length() > 0) {
        line += " ";
    }

    line += part;
}

static String buildTftPrimaryLabel(const Aircraft& aircraft) {
    String label = aircraft.callsign;
    label.trim();

    if (label.length() == 0) {
        label = aircraft.registration;
        label.trim();
    }

    if (label.length() == 0) {
        label = aircraft.icao24;
    }

    return clippedTftText(label, 8);
}

static void drawAircraftLabelSprite(
    TFT_eSprite& canvas,
    const Aircraft& aircraft,
    int x,
    int y,
    uint16_t colour,
    const DeskDarConfig& config
) {
    const int maxLines = 8;
    const int lineHeight = 8;
    const int labelGap = 5;

    String lines[maxLines];
    int lineCount = 0;

    auto addLine = [&](String value, int maxChars) {
        value.trim();

        if (value.length() == 0 || lineCount >= maxLines) {
            return;
        }

        lines[lineCount] = clippedTftText(value, maxChars);
        lineCount++;
    };

    addLine(buildTftPrimaryLabel(aircraft), 8);

    if (config.showLabelRegistration) {
        String reg = aircraft.registration;
        reg.trim();

        if (reg.length() > 0 && reg != "Unknown") {
            addLine(reg, 8);
        }
    }

    if (config.showLabelType) {
        String type = aircraft.aircraftType;
        type.trim();

        if (type.length() > 0 && type != "Unknown") {
            addLine(type, 6);
        }
    }

    if (config.showLabelDistance) {
        addLine(String(aircraft.distanceKm, 0) + "km", 7);
    }

    if (config.showLabelAltitude) {
        addLine(String((int)aircraft.altitudeFeet) + "ft", 8);
    }

    if (config.showLabelSpeed) {
        addLine(String((int)aircraft.speedKnots) + "kt", 7);
    }

    if (config.showLabelHeading) {
        addLine(String((int)aircraft.headingDegrees) + "deg", 7);
    }

    if (config.showLabelModel) {
        String model = aircraft.aircraftModel;
        model.trim();

        if (model.length() > 0 && model != "Unknown") {
            addLine(model, 10);
        }
    }

    if (lineCount == 0) {
        return;
    }

    canvas.setTextSize(1);
    canvas.setTextColor(colour, TFT_BLACK);

    int labelWidth = 0;

    for (int i = 0; i < lineCount; i++) {
        int width = canvas.textWidth(lines[i]);

        if (width > labelWidth) {
            labelWidth = width;
        }
    }

    int labelHeight = lineCount * lineHeight;

    // Keep labels visually close to the blip. Prefer drawing to the right,
    // but if that would leave the screen, flip to the left. Then clamp.
    int labelX = x + labelGap;

    if (labelX + labelWidth > canvas.width() - 1) {
        labelX = x - labelWidth - labelGap;
    }

    int labelY = y - (labelHeight / 2);

    if (labelX < 0) {
        labelX = 0;
    }

    if (labelX + labelWidth > canvas.width()) {
        labelX = canvas.width() - labelWidth;
    }

    if (labelY < 0) {
        labelY = 0;
    }

    if (labelY + labelHeight > canvas.height()) {
        labelY = canvas.height() - labelHeight;
    }

    if (labelY < 0) {
        labelY = 0;
    }

    for (int i = 0; i < lineCount; i++) {
        drawTftLabelLine(
            canvas,
            lines[i],
            labelX,
            labelY + (i * lineHeight),
            colour
        );
    }
}


static float calculateDistanceKmTft(float lat1, float lon1, float lat2, float lon2) {
    const float earthRadiusKm = 6371.0;

    float dLat = degreesToRadiansTft(lat2 - lat1);
    float dLon = degreesToRadiansTft(lon2 - lon1);

    float a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(degreesToRadiansTft(lat1)) *
        cos(degreesToRadiansTft(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return earthRadiusKm * c;
}

static float calculateBearingDegreesTft(float lat1, float lon1, float lat2, float lon2) {
    float phi1 = degreesToRadiansTft(lat1);
    float phi2 = degreesToRadiansTft(lat2);
    float deltaLon = degreesToRadiansTft(lon2 - lon1);

    float y = sin(deltaLon) * cos(phi2);
    float x =
        cos(phi1) * sin(phi2) -
        sin(phi1) * cos(phi2) * cos(deltaLon);

    float bearing = radiansToDegreesTft(atan2(y, x));

    return normaliseDegreesTft(bearing);
}

static void polarToScreenTft(
    float distanceKm,
    float bearingDegrees,
    float radarRangeKm,
    float radarOrientationDegrees,
    int cx,
    int cy,
    int radius,
    int& x,
    int& y
) {
    float displayBearing = normaliseDegreesTft(bearingDegrees - radarOrientationDegrees);
    float distanceRatio = distanceKm / radarRangeKm;

    if (distanceRatio > 1.0) {
        distanceRatio = 1.0;
    }

    float screenRadius = distanceRatio * radius;
    float bearingRad = degreesToRadiansTft(displayBearing);

    x = cx + sin(bearingRad) * screenRadius;
    y = cy - cos(bearingRad) * screenRadius;
}

static int findOrCreateTftTrail(const String& icao24) {
    int freeIndex = -1;
    unsigned long oldestSeen = 0xFFFFFFFFUL;
    int oldestIndex = 0;

    for (int i = 0; i < TFT_MAX_TRAILS; i++) {
        if (tftTrails[i].icao24 == icao24) {
            return i;
        }

        if (tftTrails[i].icao24.length() == 0 && freeIndex == -1) {
            freeIndex = i;
        }

        if (tftTrails[i].lastSeenMs < oldestSeen) {
            oldestSeen = tftTrails[i].lastSeenMs;
            oldestIndex = i;
        }
    }

    int index = freeIndex >= 0 ? freeIndex : oldestIndex;

    tftTrails[index].icao24 = icao24;
    tftTrails[index].count = 0;
    tftTrails[index].lastSeenMs = millis();
    tftTrails[index].activeThisFrame = true;

    return index;
}

static int pixelDistanceSquared(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;

    return dx * dx + dy * dy;
}

static void addTftTrailPoint(const String& icao24, int x, int y, unsigned long now) {
    if (icao24.length() == 0) {
        return;
    }

    int index = findOrCreateTftTrail(icao24);
    TftAircraftTrail& trail = tftTrails[index];

    trail.activeThisFrame = true;
    trail.lastSeenMs = now;

    if (trail.count > 0) {
        TftTrailPoint& lastPoint = trail.points[trail.count - 1];

        if (now - lastPoint.timestamp < TFT_TRAIL_SAMPLE_MS) {
            return;
        }

        if (pixelDistanceSquared(lastPoint.x, lastPoint.y, x, y) <
            TFT_TRAIL_MIN_SPACING_PX * TFT_TRAIL_MIN_SPACING_PX) {
            return;
        }
    }

    if (trail.count >= TFT_TRAIL_POINTS) {
        for (int i = 1; i < TFT_TRAIL_POINTS; i++) {
            trail.points[i - 1] = trail.points[i];
        }

        trail.count = TFT_TRAIL_POINTS - 1;
    }

    trail.points[trail.count].x = x;
    trail.points[trail.count].y = y;
    trail.points[trail.count].timestamp = now;
    trail.count++;
}

static void beginTftTrailFrame() {
    for (int i = 0; i < TFT_MAX_TRAILS; i++) {
        tftTrails[i].activeThisFrame = false;
    }
}

static void cleanupTftTrails(unsigned long now, int trailFadeSeconds) {
    unsigned long fadeMs = (unsigned long)trailFadeSeconds * 1000UL;

    for (int i = 0; i < TFT_MAX_TRAILS; i++) {
        TftAircraftTrail& trail = tftTrails[i];

        if (trail.icao24.length() == 0) {
            continue;
        }

        if (!trail.activeThisFrame || now - trail.lastSeenMs > fadeMs) {
            trail.icao24 = "";
            trail.count = 0;
            trail.lastSeenMs = 0;
            trail.activeThisFrame = false;
            continue;
        }

        int writeIndex = 0;

        for (int p = 0; p < trail.count; p++) {
            if (now - trail.points[p].timestamp <= fadeMs) {
                trail.points[writeIndex] = trail.points[p];
                writeIndex++;
            }
        }

        trail.count = writeIndex;

        if (trail.count == 0) {
            trail.icao24 = "";
            trail.lastSeenMs = 0;
            trail.activeThisFrame = false;
        }
    }
}

static void drawTftAircraftTrailsSprite(TFT_eSprite& canvas, const DeskDarConfig& config) {
    if (!config.showAircraftTrails) {
        return;
    }

    unsigned long now = millis();
    unsigned long fadeMs = (unsigned long)config.trailFadeSeconds * 1000UL;

    if (fadeMs < 10000UL) {
        fadeMs = 10000UL;
    }

    for (int i = 0; i < TFT_MAX_TRAILS; i++) {
        TftAircraftTrail& trail = tftTrails[i];

        if (trail.icao24.length() == 0 || trail.count == 0) {
            continue;
        }

        for (int p = 0; p < trail.count; p++) {
            unsigned long ageMs = now - trail.points[p].timestamp;

            if (ageMs > fadeMs) {
                continue;
            }

            float fade = 1.0 - ((float)ageMs / (float)fadeMs);
            uint16_t colour = gridGreen;

            if (fade > 0.66) {
                colour = brightGreen;
            } else if (fade > 0.33) {
                colour = radarGreen;
            }

            // TFT trails need to be more visible than the browser dots because
            // the 240x240 screen and 8-bit sprite palette make single pixels
            // very easy to miss.
            if (trail.points[p].x >= 1 &&
                trail.points[p].x < canvas.width() - 1 &&
                trail.points[p].y >= 1 &&
                trail.points[p].y < canvas.height() - 1) {
                canvas.fillCircle(trail.points[p].x, trail.points[p].y, 1, colour);
            }
        }
    }
}



static void predictAircraftPolar(const Aircraft& aircraft, float& predictedDistanceKm, float& predictedBearingDegrees);

static void drawPredictedTftAircraftTrailsSprite(
    TFT_eSprite& canvas,
    const Aircraft aircraftList[],
    int aircraftCount,
    int cx,
    int cy,
    int radius,
    float radarRangeKm,
    float radarOrientationDegrees,
    const DeskDarConfig& config
) {
    if (!config.showAircraftTrails) {
        return;
    }

    unsigned long fadeMs = (unsigned long)config.trailFadeSeconds * 1000UL;

    if (fadeMs < 10000UL) {
        fadeMs = 10000UL;
    }

    // Draw a deterministic predicted history trail behind each aircraft.
    // This is intentionally independent of the browser-side JavaScript trail
    // buffer and avoids relying on stored TFT history surviving redraws.
    // It makes TFT trails visible immediately and keeps them aligned with the
    // same prediction model used for the current aircraft blip.
    for (int i = 0; i < aircraftCount; i++) {
        const Aircraft& aircraft = aircraftList[i];

        if (aircraft.speedMetersPerSecond <= 1.0) {
            continue;
        }

        float currentDistanceKm = aircraft.distanceKm;
        float currentBearingDegrees = aircraft.bearingDegrees;

        predictAircraftPolar(
            aircraft,
            currentDistanceKm,
            currentBearingDegrees
        );

        if (currentDistanceKm > radarRangeKm) {
            continue;
        }

        float currentBearingRad = degreesToRadiansTft(currentBearingDegrees);
        float currentEastKm = sin(currentBearingRad) * currentDistanceKm;
        float currentNorthKm = cos(currentBearingRad) * currentDistanceKm;

        float headingRad = degreesToRadiansTft(aircraft.headingDegrees);
        float speedKmPerSecond = aircraft.speedMetersPerSecond / 1000.0;
        float eastPerSecond = sin(headingRad) * speedKmPerSecond;
        float northPerSecond = cos(headingRad) * speedKmPerSecond;

        int previousX = -10000;
        int previousY = -10000;

        // Use a fixed number of visual samples, spread across the configured
        // fade time. This avoids excessive dots and keeps slow aircraft from
        // stacking dots on top of each other.
        const int maxTrailDots = 10;
        unsigned long stepMs = fadeMs / (maxTrailDots + 1);

        if (stepMs < 3000UL) {
            stepMs = 3000UL;
        }

        for (int dot = 1; dot <= maxTrailDots; dot++) {
            unsigned long ageMs = stepMs * dot;

            if (ageMs >= fadeMs) {
                break;
            }

            float ageSeconds = ageMs / 1000.0;
            float eastKm = currentEastKm - (eastPerSecond * ageSeconds);
            float northKm = currentNorthKm - (northPerSecond * ageSeconds);

            float trailDistanceKm = sqrt(eastKm * eastKm + northKm * northKm);

            if (trailDistanceKm > radarRangeKm) {
                continue;
            }

            float trailBearingDegrees = normaliseDegreesTft(
                radiansToDegreesTft(atan2(eastKm, northKm))
            );

            int x = 0;
            int y = 0;

            polarToScreenTft(
                trailDistanceKm,
                trailBearingDegrees,
                radarRangeKm,
                radarOrientationDegrees,
                cx,
                cy,
                radius,
                x,
                y
            );

            if (x < 1 || x >= canvas.width() - 1 || y < 1 || y >= canvas.height() - 1) {
                continue;
            }

            if (previousX > -9999 &&
                pixelDistanceSquared(previousX, previousY, x, y) <
                    TFT_TRAIL_MIN_SPACING_PX * TFT_TRAIL_MIN_SPACING_PX) {
                continue;
            }

            previousX = x;
            previousY = y;

            float fade = 1.0 - ((float)ageMs / (float)fadeMs);
            uint16_t colour = gridGreen;

            if (fade > 0.66) {
                colour = radarGreen;
            } else if (fade > 0.33) {
                colour = dimGreen;
            } else {
                colour = gridGreen;
            }

            // Older dots are smaller/dimmer; newer dots are slightly larger.
            if (fade > 0.55) {
                canvas.fillCircle(x, y, 1, colour);
            } else {
                canvas.drawPixel(x, y, colour);
            }
        }
    }
}


static void drawAirportMarkersSprite(
    TFT_eSprite& canvas,
    int cx,
    int cy,
    int radius,
    float radarRangeKm,
    float radarOrientationDegrees,
    float userLatitude,
    float userLongitude,
    const DeskDarConfig& config
) {
    if (!config.showAirports) {
        return;
    }

    for (int i = 0; i < UK_AIRPORT_COUNT; i++) {
        const AirportLandmark& airport = UK_AIRPORTS[i];

        if (airport.category == AIRPORT_MAJOR && !config.showMajorAirports) {
            continue;
        }

        if (airport.category == AIRPORT_GA && !config.showGaAirports) {
            continue;
        }

        float distanceKm = calculateDistanceKmTft(
            userLatitude,
            userLongitude,
            airport.latitude,
            airport.longitude
        );

        if (distanceKm > radarRangeKm) {
            continue;
        }

        float bearingDegrees = calculateBearingDegreesTft(
            userLatitude,
            userLongitude,
            airport.latitude,
            airport.longitude
        );

        int x = 0;
        int y = 0;

        polarToScreenTft(
            distanceKm,
            bearingDegrees,
            radarRangeKm,
            radarOrientationDegrees,
            cx,
            cy,
            radius,
            x,
            y
        );

        if (x < 1 || x >= canvas.width() - 1 || y < 1 || y >= canvas.height() - 1) {
            continue;
        }

        uint16_t airportBlue = tft.color565(80, 180, 255);
        uint16_t colour = airportBlue;

        canvas.drawRect(x - 1, y - 1, 3, 3, colour);

        if (airport.category == AIRPORT_MAJOR) {
            String ident = airport.ident;
            int textWidth = canvas.textWidth(ident);
            int labelX = x + 3;
            int labelY = y - 4;

            if (labelX + textWidth > canvas.width()) {
                labelX = x - textWidth - 3;
            }

            if (labelX < 0) labelX = 0;
            if (labelY < 0) labelY = 0;
            if (labelY > canvas.height() - 8) labelY = canvas.height() - 8;

            canvas.setTextSize(1);
            canvas.setTextColor(colour, TFT_BLACK);
            canvas.setCursor(labelX, labelY);
            canvas.print(ident);
        }
    }
}

static void drawRadarSweepSprite(
    TFT_eSprite& canvas,
    int cx,
    int cy,
    int radius,
    float radarOrientationDegrees
) {
    const float displayAngle = normaliseDegreesTft(
        tftSweepAngleDegrees - radarOrientationDegrees
    );

    float rad = degreesToRadiansTft(displayAngle);

    int x = cx + sin(rad) * radius;
    int y = cy - cos(rad) * radius;

    canvas.drawLine(cx, cy, x, y, brightGreen);
}

static void drawCompassLabelSprite(TFT_eSprite& canvas, const String& label, float trueBearingDegrees, float radarOrientationDegrees, int cx, int cy, int radius) {
    float displayAngle = normaliseDegreesTft(trueBearingDegrees - radarOrientationDegrees);
    float rad = degreesToRadiansTft(displayAngle);

    int labelRadius = radius + 8;
    int x = cx + sin(rad) * labelRadius;
    int y = cy - cos(rad) * labelRadius;

    canvas.setTextSize(1);
    canvas.setTextColor(dimGreen, TFT_BLACK);

    int textWidth = canvas.textWidth(label);
    int textHeight = 8;

    x -= textWidth / 2;
    y -= textHeight / 2;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > canvas.width() - textWidth) x = canvas.width() - textWidth;
    if (y > canvas.height() - textHeight) y = canvas.height() - textHeight;

    canvas.setCursor(x, y);
    canvas.print(label);
}

static void drawRadarGridSprite(TFT_eSprite& canvas, int cx, int cy, int radius, float radarOrientationDegrees) {
    canvas.drawCircle(cx, cy, radius, gridGreen);
    canvas.drawCircle(cx, cy, radius * 2 / 3, gridGreen);
    canvas.drawCircle(cx, cy, radius / 3, gridGreen);

    canvas.drawLine(cx - radius, cy, cx + radius, cy, gridGreen);
    canvas.drawLine(cx, cy - radius, cx, cy + radius, gridGreen);

    for (int angle = 45; angle < 360; angle += 45) {
        float rad = degreesToRadiansTft(angle);
        int x = cx + sin(rad) * radius;
        int y = cy - cos(rad) * radius;
        canvas.drawLine(cx, cy, x, y, gridGreen);
    }

    drawCompassLabelSprite(canvas, "N", 0.0, radarOrientationDegrees, cx, cy, radius);
    drawCompassLabelSprite(canvas, "E", 90.0, radarOrientationDegrees, cx, cy, radius);
    drawCompassLabelSprite(canvas, "S", 180.0, radarOrientationDegrees, cx, cy, radius);
    drawCompassLabelSprite(canvas, "W", 270.0, radarOrientationDegrees, cx, cy, radius);

    canvas.fillCircle(cx, cy, 2, radarGreen);
}

static void predictAircraftPolar(
    const Aircraft& aircraft,
    float& predictedDistanceKm,
    float& predictedBearingDegrees
) {
    unsigned long now = millis();
    unsigned long ageMs = 0;

    if (aircraft.lastSeenMillis > 0 && now >= aircraft.lastSeenMillis) {
        ageMs = now - aircraft.lastSeenMillis;
    }

    float elapsedSeconds = ageMs / 1000.0;

    // Convert current polar radar location to local tangent-plane km.
    // eastKm = +right/east, northKm = +up/north.
    float bearingRad = degreesToRadiansTft(aircraft.bearingDegrees);
    float eastKm = sin(bearingRad) * aircraft.distanceKm;
    float northKm = cos(bearingRad) * aircraft.distanceKm;

    // Move the aircraft using speed and true heading.
    float travelKm = (aircraft.speedMetersPerSecond * elapsedSeconds) / 1000.0;
    float headingRad = degreesToRadiansTft(aircraft.headingDegrees);

    eastKm += sin(headingRad) * travelKm;
    northKm += cos(headingRad) * travelKm;

    predictedDistanceKm = sqrt(eastKm * eastKm + northKm * northKm);
    predictedBearingDegrees = normaliseDegreesTft(
        radiansToDegreesTft(atan2(eastKm, northKm))
    );
}

static uint16_t aircraftColourForAge(const Aircraft& aircraft, const DeskDarConfig& config) {
    if (config.disableAircraftFade) {
        return brightGreen;
    }

    unsigned long now = millis();

    if (aircraft.lastSeenMillis == 0 || now < aircraft.lastSeenMillis) {
        return brightGreen;
    }

    unsigned long ageMs = now - aircraft.lastSeenMillis;

    if (ageMs < 10000) {
        return brightGreen;
    }

    if (ageMs < 22000) {
        return radarGreen;
    }

    return dimGreen;
}


static void updateTftAircraftTrailsFromAircraftList(
    const Aircraft aircraftList[],
    int aircraftCount,
    int cx,
    int cy,
    int radius,
    float radarRangeKm,
    float radarOrientationDegrees,
    const DeskDarConfig& config
) {
    if (!config.showAircraftTrails) {
        return;
    }

    unsigned long now = millis();

    for (int i = 0; i < aircraftCount; i++) {
        const Aircraft& aircraft = aircraftList[i];

        float predictedDistanceKm = aircraft.distanceKm;
        float predictedBearingDegrees = aircraft.bearingDegrees;

        predictAircraftPolar(
            aircraft,
            predictedDistanceKm,
            predictedBearingDegrees
        );

        // If the aircraft has left the current radar range, do not mark its
        // trail active. cleanupTftTrails() will remove it immediately.
        if (predictedDistanceKm > radarRangeKm) {
            continue;
        }

        int x = 0;
        int y = 0;

        polarToScreenTft(
            predictedDistanceKm,
            predictedBearingDegrees,
            radarRangeKm,
            radarOrientationDegrees,
            cx,
            cy,
            radius,
            x,
            y
        );

        if (x < 0 || x >= spriteSize || y < 0 || y >= spriteSize) {
            continue;
        }

        addTftTrailPoint(aircraft.icao24, x, y, now);
    }
}

static void drawAircraftBlipsSprite(
    TFT_eSprite& canvas,
    const Aircraft aircraftList[],
    int aircraftCount,
    int cx,
    int cy,
    int radius,
    float radarRangeKm,
    float radarOrientationDegrees,
    const DeskDarConfig& config
) {
    for (int i = 0; i < aircraftCount; i++) {
        const Aircraft& aircraft = aircraftList[i];

        float predictedDistanceKm = aircraft.distanceKm;
        float predictedBearingDegrees = aircraft.bearingDegrees;

        predictAircraftPolar(
            aircraft,
            predictedDistanceKm,
            predictedBearingDegrees
        );

        if (predictedDistanceKm > radarRangeKm) {
            continue;
        }

        float displayBearing =
            normaliseDegreesTft(predictedBearingDegrees - radarOrientationDegrees);

        float distanceRatio = predictedDistanceKm / radarRangeKm;
        if (distanceRatio > 1.0) {
            distanceRatio = 1.0;
        }

        float screenRadius = distanceRatio * radius;
        float bearingRad = degreesToRadiansTft(displayBearing);

        int x = cx + sin(bearingRad) * screenRadius;
        int y = cy - cos(bearingRad) * screenRadius;

        if (x < 0 || x >= canvas.width() || y < 0 || y >= canvas.height()) {
            continue;
        }

        uint16_t aircraftColour = aircraftColourForAge(aircraft, config);

        canvas.fillCircle(x, y, 2, aircraftColour);

        float displayHeading =
            normaliseDegreesTft(aircraft.headingDegrees - radarOrientationDegrees);

        float headingRad = degreesToRadiansTft(displayHeading);
        int hx = x + sin(headingRad) * 7;
        int hy = y - cos(headingRad) * 7;
        canvas.drawLine(x, y, hx, hy, aircraftColour);

        drawAircraftLabelSprite(
            canvas,
            aircraft,
            x,
            y,
            aircraftColour,
            config
        );
    }
}


static bool tryCreateRadarSpriteAtDepth(int colourDepth) {
    radarSprite.deleteSprite();
    radarSprite.setColorDepth(colourDepth);

    int maxSpriteSize = min(tft.width(), tft.height());
    const int candidateSizes[] = {240, 220, 200, 180, 160, 140};

    for (int i = 0; i < 6; i++) {
        int candidateSize = candidateSizes[i];

        if (candidateSize > maxSpriteSize) {
            continue;
        }

        void* spriteMemory = radarSprite.createSprite(candidateSize, candidateSize);

        if (spriteMemory != nullptr) {
            spriteSize = candidateSize;
            spriteColourDepth = colourDepth;
            spriteDrawX = (tft.width() - spriteSize) / 2;
            spriteDrawY = (tft.height() - spriteSize) / 2;

            radarSprite.setTextFont(1);
            radarSprite.setTextSize(1);
            radarSprite.fillSprite(TFT_BLACK);

            Serial.print("TFT sprite ready: ");
            Serial.print(spriteSize);
            Serial.print("x");
            Serial.print(spriteSize);
            Serial.print(" depth ");
            Serial.println(spriteColourDepth);

            return true;
        }

        radarSprite.deleteSprite();
    }

    return false;
}

static bool createRadarSpriteAdaptive() {
    // Prefer 8-bit colour. If heap is fragmented, progressively reduce sprite size.
    if (tryCreateRadarSpriteAtDepth(8)) {
        return true;
    }

    // 4-bit fallback uses far less RAM. Good enough for green radar graphics.
    if (tryCreateRadarSpriteAtDepth(4)) {
        return true;
    }

    // 1-bit fallback avoids failure messages and keeps the device usable.
    if (tryCreateRadarSpriteAtDepth(1)) {
        return true;
    }

    return false;
}

static void pushRadarSprite() {
    if (!spriteReady) {
        return;
    }

    radarSprite.pushSprite(spriteDrawX, spriteDrawY);
}

static void drawDirectFallback(
    const Aircraft aircraftList[],
    int aircraftCount,
    bool locationReady,
    bool appConfigReady,
    float radarRangeKm,
    float radarOrientationDegrees,
    const String& statusLine,
    const String& ipAddress
) {
    static unsigned long lastDirectRenderMs = 0;
    unsigned long now = millis();

    if (now - lastDirectRenderMs < 500) {
        return;
    }

    lastDirectRenderMs = now;

    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    tft.setCursor(8, 8);
    tft.print("DeskDar TFT");

    tft.setCursor(8, 24);
    tft.print("Sprite unavailable");

    tft.setCursor(8, 40);
    tft.print("Heap:");
    tft.print(ESP.getFreeHeap());

    tft.setCursor(8, 56);
    tft.print("AC:");
    tft.print(aircraftCount);

    // Range/orientation text is controlled by user display settings in sprite mode.
    // Direct fallback keeps minimal diagnostics visible.
    tft.setCursor(8, 72);
    tft.print((int)radarRangeKm);
    tft.print("km ");
    tft.print((int)radarOrientationDegrees);
    tft.print("deg");

    if (!appConfigReady) {
        tft.setCursor(8, 96);
        tft.print("Setup incomplete");
        tft.setCursor(8, 112);
        tft.print("Open browser:");
        tft.setCursor(8, 128);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.print("http://");
        tft.print(ipAddress);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
    } else if (!locationReady) {
        tft.setCursor(8, 96);
        tft.print("Waiting location/API");
    } else if (statusLine.length() > 0) {
        tft.setCursor(8, 96);
        String clipped = statusLine;
        if (clipped.length() > 24) {
            clipped = clipped.substring(0, 24);
        }
        tft.print(clipped);
    }

    (void)aircraftList;
}

void initTftDisplay() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    delay(50);
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    // Create the sprite once only. Use adaptive sizing/depth so the display
    // still starts even if contiguous heap is lower than expected.
    spriteReady = createRadarSpriteAdaptive();

    if (spriteReady) {
        tft.fillScreen(TFT_BLACK);
        drawCenteredTextSprite(radarSprite, "DeskDar", 10, TFT_GREEN, 2);
        drawCenteredTextSprite(radarSprite, "TFT online", 34, TFT_GREEN, 1);
        pushRadarSprite();
    } else {
        Serial.println("TFT sprite allocation failed; using direct fallback renderer.");
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(10, 10);
        tft.print("DeskDar");
        tft.setCursor(10, 30);
        tft.print("Direct TFT mode");
    }

    tftReady = true;
}


void showTftSetupPortalScreen() {
    if (!tftReady) {
        return;
    }

    if (spriteReady) {
        radarSprite.fillSprite(TFT_BLACK);
        drawCenteredTextSprite(radarSprite, "DeskDar", 12, TFT_GREEN, 2);
        drawCenteredTextSprite(radarSprite, "WiFi Setup", 42, TFT_YELLOW, 1);
        drawCenteredTextSprite(radarSprite, "Connect to:", 68, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "DeskDar Setup", 86, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "Then open:", 116, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "192.168.4.1", 134, TFT_CYAN, 1);
        drawCenteredTextSprite(radarSprite, "in your browser", 154, TFT_GREEN, 1);
        pushRadarSprite();
        return;
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(24, 16);
    tft.print("DeskDar");

    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(20, 48);
    tft.print("WiFi Setup");

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(20, 74);
    tft.print("Connect to:");
    tft.setCursor(20, 90);
    tft.print("DeskDar Setup");
    tft.setCursor(20, 120);
    tft.print("Then open:");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(20, 136);
    tft.print("192.168.4.1");
}


void showTftAppSetupIncompleteScreen(const String& ipAddress) {
    if (!tftReady) {
        return;
    }

    if (spriteReady) {
        radarSprite.fillSprite(TFT_BLACK);
        drawCenteredTextSprite(radarSprite, "DeskDar", 8, TFT_GREEN, 2);
        drawCenteredTextSprite(radarSprite, "Setup incomplete", 38, TFT_YELLOW, 1);
        drawCenteredTextSprite(radarSprite, "Open in browser:", 62, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "http://" + ipAddress, 82, TFT_CYAN, 1);
        drawCenteredTextSprite(radarSprite, "Then enter:", 104, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "Postcode", 126, TFT_CYAN, 1);
        drawCenteredTextSprite(radarSprite, "OpenSky keys", 144, TFT_CYAN, 1);

        pushRadarSprite();
        lastTftRenderMs = millis();
        return;
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(24, 12);
    tft.print("DeskDar");

    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(12, 44);
    tft.print("Setup incomplete");

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(12, 68);
    tft.print("Open in browser:");
    tft.setCursor(10, 72);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print(ipAddress);
    tft.setCursor(12, 84);
    tft.print("to enter:");

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(12, 108);
    tft.print("Postcode");
    tft.setCursor(12, 124);
    tft.print("OpenSky keys");

    if (ipAddress.length() > 0) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(12, 156);
        tft.print("http://");
        tft.print(ipAddress);
    }

    lastTftRenderMs = millis();
}


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
) {
    if (!tftReady) {
        return;
    }

    unsigned long now = millis();
    if (now - lastTftRenderMs < TFT_RENDER_INTERVAL_MS) {
        return;
    }

    lastTftRenderMs = now;
    updateTftSweep(now);

    if (!spriteReady) {
        drawDirectFallback(
            aircraftList,
            aircraftCount,
            locationReady,
            appConfigReady,
            radarRangeKm,
            config.radarOrientationDegrees,
            statusLine,
            ipAddress
        );
        return;
    }

    radarSprite.fillSprite(TFT_BLACK);

    if (!appConfigReady) {
        drawCenteredTextSprite(radarSprite, "DeskDar", 8, TFT_GREEN, 2);
        drawCenteredTextSprite(radarSprite, "Setup incomplete", 38, TFT_YELLOW, 1);
        drawCenteredTextSprite(radarSprite, "Open in browser:", 62, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "http://" + ipAddress, 82, TFT_CYAN, 1);
        drawCenteredTextSprite(radarSprite, "Then enter:", 104, TFT_GREEN, 1);
        drawCenteredTextSprite(radarSprite, "Postcode", 126, TFT_CYAN, 1);
        drawCenteredTextSprite(radarSprite, "OpenSky keys", 144, TFT_CYAN, 1);
        pushRadarSprite();
        return;
    }

    if (!locationReady) {
        drawCenteredTextSprite(radarSprite, "DeskDar", 10, TFT_GREEN, 2);
        drawCenteredTextSprite(radarSprite, "Waiting for", 42, TFT_YELLOW, 1);
        drawCenteredTextSprite(radarSprite, "location/API", 58, TFT_YELLOW, 1);
        pushRadarSprite();
        return;
    }

    int width = radarSprite.width();
    int height = radarSprite.height();

    int cx = width / 2;
    int cy = height / 2 + 4;

    int radius = min(width, height) / 2 - 20;
    if (radius < 20) {
        radius = 20;
    }

    drawRadarSweepSprite(
        radarSprite,
        cx,
        cy,
        radius,
        config.radarOrientationDegrees
    );

    drawRadarGridSprite(radarSprite, cx, cy, radius, config.radarOrientationDegrees);

    drawAirportMarkersSprite(
        radarSprite,
        cx,
        cy,
        radius,
        radarRangeKm,
        config.radarOrientationDegrees,
        userLatitude,
        userLongitude,
        config
    );

    // TFT trails are drawn from predicted aircraft motion so they are visible
    // immediately on the physical display, independent of browser-side history.
    drawPredictedTftAircraftTrailsSprite(
        radarSprite,
        aircraftList,
        aircraftCount,
        cx,
        cy,
        radius,
        radarRangeKm,
        config.radarOrientationDegrees,
        config
    );

    drawAircraftBlipsSprite(
        radarSprite,
        aircraftList,
        aircraftCount,
        cx,
        cy,
        radius,
        radarRangeKm,
        config.radarOrientationDegrees,
        config
    );

    radarSprite.setTextSize(1);
    radarSprite.setTextColor(textGreen, TFT_BLACK);
    radarSprite.setCursor(2, 2);
    radarSprite.print("DeskDar");

    if (config.showTftRadarRange) {
        radarSprite.print(" ");
        radarSprite.print((int)radarRangeKm);
        radarSprite.print("km");
    }

    radarSprite.setCursor(2, height - 10);
    radarSprite.print("AC:");
    radarSprite.print(aircraftCount);

    radarSprite.setCursor(width - 54, height - 10);
    radarSprite.print((int)config.radarOrientationDegrees);
    radarSprite.print("deg");

    if (config.showTftIpAddress && statusLine.length() > 0) {
        radarSprite.setCursor(2, 14);
        String clipped = statusLine;
        if (clipped.length() > 22) {
            clipped = clipped.substring(0, 22);
        }
        radarSprite.print(clipped);
    }

    pushRadarSprite();
}

#else


static bool tryCreateRadarSpriteAtDepth(int colourDepth) {
    radarSprite.deleteSprite();
    radarSprite.setColorDepth(colourDepth);

    int maxSpriteSize = min(tft.width(), tft.height());
    const int candidateSizes[] = {240, 220, 200, 180, 160, 140};

    for (int i = 0; i < 6; i++) {
        int candidateSize = candidateSizes[i];

        if (candidateSize > maxSpriteSize) {
            continue;
        }

        void* spriteMemory = radarSprite.createSprite(candidateSize, candidateSize);

        if (spriteMemory != nullptr) {
            spriteSize = candidateSize;
            spriteColourDepth = colourDepth;
            spriteDrawX = (tft.width() - spriteSize) / 2;
            spriteDrawY = (tft.height() - spriteSize) / 2;

            radarSprite.setTextFont(1);
            radarSprite.setTextSize(1);
            radarSprite.fillSprite(TFT_BLACK);

            Serial.print("TFT sprite ready: ");
            Serial.print(spriteSize);
            Serial.print("x");
            Serial.print(spriteSize);
            Serial.print(" depth ");
            Serial.println(spriteColourDepth);

            return true;
        }

        radarSprite.deleteSprite();
    }

    return false;
}

static bool createRadarSpriteAdaptive() {
    // Prefer 8-bit colour. If heap is fragmented, progressively reduce sprite size.
    if (tryCreateRadarSpriteAtDepth(8)) {
        return true;
    }

    // 4-bit fallback uses far less RAM. Good enough for green radar graphics.
    if (tryCreateRadarSpriteAtDepth(4)) {
        return true;
    }

    // 1-bit fallback avoids failure messages and keeps the device usable.
    if (tryCreateRadarSpriteAtDepth(1)) {
        return true;
    }

    return false;
}

static void pushRadarSprite() {
    if (!spriteReady) {
        return;
    }

    radarSprite.pushSprite(spriteDrawX, spriteDrawY);
}

static void drawDirectFallback(
    const Aircraft aircraftList[],
    int aircraftCount,
    bool locationReady,
    bool appConfigReady,
    float radarRangeKm,
    float radarOrientationDegrees,
    const String& statusLine,
    const String& ipAddress
) {
    static unsigned long lastDirectRenderMs = 0;
    unsigned long now = millis();

    if (now - lastDirectRenderMs < 500) {
        return;
    }

    lastDirectRenderMs = now;

    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    tft.setCursor(8, 8);
    tft.print("DeskDar TFT");

    tft.setCursor(8, 24);
    tft.print("Sprite unavailable");

    tft.setCursor(8, 40);
    tft.print("Heap:");
    tft.print(ESP.getFreeHeap());

    tft.setCursor(8, 56);
    tft.print("AC:");
    tft.print(aircraftCount);

    // Range/orientation text is controlled by user display settings in sprite mode.
    // Direct fallback keeps minimal diagnostics visible.
    tft.setCursor(8, 72);
    tft.print((int)radarRangeKm);
    tft.print("km ");
    tft.print((int)radarOrientationDegrees);
    tft.print("deg");

    if (!appConfigReady) {
        tft.setCursor(8, 96);
        tft.print("Setup incomplete");
        tft.setCursor(8, 112);
        tft.print("Open browser:");
        tft.setCursor(8, 128);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.print("http://");
        tft.print(ipAddress);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
    } else if (!locationReady) {
        tft.setCursor(8, 96);
        tft.print("Waiting location/API");
    } else if (statusLine.length() > 0) {
        tft.setCursor(8, 96);
        String clipped = statusLine;
        if (clipped.length() > 24) {
            clipped = clipped.substring(0, 24);
        }
        tft.print(clipped);
    }

    (void)aircraftList;
}

void initTftDisplay() {
    Serial.println("TFT_eSPI not available; TFT display disabled.");
}

void showTftSetupPortalScreen() {
    Serial.println("TFT_eSPI not available; setup screen disabled.");
}

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
) {
    (void)userLatitude;
    (void)userLongitude;
    (void)aircraftList;
    (void)aircraftCount;
    (void)locationReady;
    (void)appConfigReady;
    (void)radarRangeKm;
    (void)config;
    (void)statusLine;
    (void)ipAddress;
}

#endif
