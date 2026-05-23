# DeskDar Development Changelog

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