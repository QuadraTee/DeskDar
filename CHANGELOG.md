
## v0.16-ota-progress-update-check
- Added OTA upload progress logging and progress bar.
- Added GitHub latest release/tag check on the OTA page.
- Improved OTA user guidance during firmware upload and reboot.
## v0.14-settings-layout-polish
- Improved Settings page checkbox alignment for radar label display options.


## v0.13-live-settings-apply
- Dashboard settings now save and apply without restarting the ESP32.
- Radar orientation and browser radar label options update through saved config without reboot.
- Postcode/OpenSky changes reinitialise DeskDar services in-place.


## v0.11-browser-radar-prediction
- Added browser-side dead reckoning for smooth aircraft movement between OpenSky updates.
- Added aircraft age data to `/aircraft.json`.
- Browser radar now predicts aircraft positions using speed, heading, distance, bearing, and elapsed time.
- Browser fade values now update continuously based on aircraft age.

## v0.8-dashboard-settings
- Split first-boot onboarding into two stages:
  - captive portal now collects WiFi only
  - main dashboard collects postcode and OpenSky credentials after joining home WiFi
- Promoted `/` to the main DeskDar dashboard.
- Kept `/debug` as an alias for the dashboard.
- Added navigation tabs for Dashboard, Logs, Settings, Aircraft, and System.
- Added styled web dashboard layout.
- Added Settings page for postcode and OpenSky credential updates.
- Added reset/setup action to restart into WiFi setup mode.
- Added Aircraft page with current aircraft summaries.
- Added System page with firmware, memory, and config status.
- Added WiFi connection timeout fallback to setup portal.

# DeskDar Development Changelog

## v0.12-browser-radar-label-settings
- Removed the solid green browser radar sweep line, leaving only the soft sweep wedge.
- Added radar label display settings for registration, model, type, distance, altitude, speed, and heading.
- Added persistent storage for browser radar label preferences.
- Updated browser radar label rendering to follow the selected settings.

## v0.7-renderer-prep
- Decreased opensky API timing to once every 30s 
- Decreased time between each ABSDBD API call
- Added radar renderer abstraction layer for future TFT display work.
- Added aircraft timestamps for fade, stale-state and prediction support.
- Added predicted radar coordinate helpers using speed and heading dead reckoning.
- Added radar sweep state and frame timing scaffold.
- Added lightweight trail fields to aircraft records for future persistence effects.
- Added current aircraft state storage for future display-frame rendering.

---

## Initial Aircraft Tracking
- Added OpenSky aircraft position fetching
- Implemented nearby aircraft filtering using postcode coordinates
- Added aircraft sorting by nearest distance
- Added bearing and directional calculations
- Added heading vector calculations
- Added altitude, speed, heading, and vertical rate display
- Added ASCII radar preview renderer

---

## Aircraft Data Structure Improvements
- Added ICAO24 aircraft identifiers
- Added flight callsign parsing
- Added aircraft country extraction
- Added placeholder fields for:
  - registration
  - aircraft model
  - aircraft type

---

## OpenSky Authentication
- Created OpenSky authenticated access flow
- Added OAuth token retrieval using OpenSky credentials
- Added bearer token support to aircraft requests
- Moved secrets into `secrets.h`
- Added:
  - WiFi SSID
  - WiFi password
  - postcode
  - OpenSky credentials
- Added `.gitignore` protections for:
  - credentials
  - secrets
  - PlatformIO files

---

## OpenSky Stability Improvements
- Added OpenSky HTTP status debugging
- Added HTTP 429 handling
- Added graceful rate-limit handling
- Prevented crash loops on failed OpenSky requests

---

## ADSBdb Metadata System
- Added ADSBdb aircraft metadata lookups
- Implemented metadata retrieval using ICAO24 identifiers
- Added metadata parsing for:
  - registration
  - manufacturer
  - aircraft model
  - ICAO aircraft type

Example successful lookup:

```text
Registration: G-BYXC
Model: Grob G-115 E Tutor
Type: G115
```

---

## Metadata Queue Architecture
- Added metadata lookup queue
- Prevented duplicate queued ICAO24 requests
- Added queue size limiting
- Added delayed metadata processing
- Added one-aircraft-at-a-time lookup strategy

---

## Crash Investigation & Resolution

### Identified Issues
- Stack canary crashes during ADSBdb HTTPS requests
- ESP32 loop task stack exhaustion
- Concurrent HTTPS pressure from OpenSky + ADSBdb

### Solutions Implemented
- Separated aircraft fetch and metadata fetch timing
- Added network quiet windows
- Added safe scheduling logic
- Prevented simultaneous network operations
- Moved metadata lookups into dedicated FreeRTOS task

---

## FreeRTOS Background Metadata Task
- Added dedicated metadata task using:
  - `xTaskCreatePinnedToCore`
- Increased metadata task stack size to 16384 bytes
- Added metadata task state tracking
- Prevented concurrent metadata task creation

---

## Smart Network Scheduling

Implemented timing windows:

```cpp
AIRCRAFT_REFRESH_MS = 60000
METADATA_REFRESH_MS = 5000
MIN_GAP_BETWEEN_NETWORK_TASKS_MS = 15000
```

Scheduling behaviour:
- OpenSky aircraft fetch every 60s
- Metadata opportunities every 5s
- Metadata only runs safely between OpenSky windows

---

## Metadata Cache System

### In-Memory Aircraft Cache
Added:
- ICAO24 metadata cache
- registration cache
- model cache
- aircraft type cache

### Cache Features
- Instant metadata retrieval for known aircraft
- Reduced ADSBdb API usage
- Reduced network overhead
- Faster aircraft enrichment

---

## FIFO Cache Eviction

Implemented:
- FIFO cache eviction strategy
- Automatic oldest-aircraft removal when full

Behaviour:

```text
Cache full
↓
Oldest aircraft removed
↓
Newest aircraft inserted
```

---

## Cache Telemetry & Debugging
Added:
- live cache usage counters
- cache save notifications
- metadata queue visibility

Example output:

```text
Metadata saved to cache for: 400ea0
Cache usage: 12/30
```

---

## Stability Milestones Reached

Successfully achieved:
- Long-running stable operation
- Concurrent OpenSky + ADSBdb integration
- Background metadata enrichment
- Crash-free FreeRTOS scheduling
- Safe HTTPS networking
- Stateful aircraft learning system

---

# Current DeskDar Architecture

```text
WiFi
↓
OpenSky OAuth Authentication
↓
Live Aircraft Fetch
↓
Distance/Bearing/Radar Processing
↓
ASCII Radar Rendering
↓
Metadata Queue
↓
Background FreeRTOS Metadata Task
↓
ADSBdb Lookup
↓
FIFO Metadata Cache
↓
Persistent Aircraft Knowledge During Runtime
```

---

# Current Feature Set

## Aircraft Tracking
- Live nearby aircraft
- Distance sorting
- Heading vectors
- Bearing calculations
- Altitude/speed tracking

## Radar
- ASCII radar renderer
- Relative aircraft positioning
- Directional heading indicators

## Metadata
- Registration lookup
- Aircraft manufacturer
- Aircraft model
- ICAO aircraft type

## Reliability
- OpenSky authentication
- Rate-limit handling
- Task isolation
- Queue management
- FIFO cache eviction
- Long-runtime stability

---

# Recommended Next Steps

## High Priority
- Long-duration soak testing
- Free heap telemetry
- Cache persistence using LittleFS

## Medium Priority
- TFT display integration
- Graphical radar UI
- Aircraft icons/classes
- Touch/button controls

## Advanced
- Persistent aircraft database
- Motion trails
- Aircraft history
- Local metadata database
- Multi-layer radar rendering
## v0.9-browser-radar
- Added browser-based radar preview using HTML canvas.
- Added `/aircraft.json` endpoint for live aircraft dashboard data.
- Added live sweep rendering, range rings, bearing grid, aircraft blips, labels, headings, and fade values in the dashboard.
- Kept `/debug` as an alias for the main dashboard.


## v0.10-browser-radar-orientation
- Added persistent radar orientation / facing direction setting.
- Browser radar now rotates aircraft and headings relative to configured facing direction.
- Browser radar sweep now animates locally using `requestAnimationFrame()` for smoother motion.
- Removed stale server-driven sweep value from the dashboard radar summary.



## v0.15-ota-updates
- Added web-based OTA firmware upload from the System page.
- Added OTA update progress logging and automatic restart after successful update.
- Saved DeskDar configuration remains in ESP32 NVS across firmware updates.
