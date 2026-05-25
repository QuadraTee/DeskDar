# DeskDar

DeskDar is a real-time aircraft radar and metadata tracking system built on the ESP32 platform.

It combines:
- OpenSky Network live aircraft telemetry
- ADSBdb aircraft metadata enrichment
- postcode-based geolocation
- ASCII radar visualisation
- FreeRTOS background task scheduling
- onboard metadata caching
- live web debugging tools

The project is designed as a lightweight always-on desktop radar appliance.

---

# Features

## Live Aircraft Tracking
- Real-time nearby aircraft detection
- Distance sorting
- Bearing and compass direction calculations
- Altitude tracking
- Speed tracking
- Heading vectors
- Vertical rate monitoring

## Radar Display
- ASCII radar preview
- Relative aircraft positioning
- Heading indicators

## Aircraft Metadata
- Registration lookup
- Manufacturer lookup
- Aircraft model lookup
- ICAO aircraft type lookup

## Smart Networking
- OpenSky OAuth authentication
- Automatic token refresh
- Background metadata enrichment
- Safe task scheduling
- Rate-limit handling

## Cache System
- In-memory metadata cache
- FIFO cache eviction
- Reduced API usage
- Faster repeat aircraft enrichment

## Web Debug Dashboard
- ESP32-hosted debug webpage
- Live system logs
- Heap monitoring
- WiFi status
- Runtime telemetry

---

# Hardware

## Current Platform
- ESP32 Dev Board

## Planned Hardware
- TFT radar display
- Dedicated enclosure
- Always-on desktop deployment

---

# Software Architecture

```text
WiFi
↓
OpenSky OAuth Authentication
↓
Live Aircraft Fetch
↓
Distance/Bearing Processing
↓
ASCII Radar Rendering
↓
Metadata Queue
↓
FreeRTOS Metadata Task
↓
ADSBdb Lookup
↓
FIFO Metadata Cache
↓
Web Debug Dashboard
```

---

# Current Stability Status

The current architecture has been soak-tested for multiple hours with:
- stable WiFi operation
- stable HTTPS networking
- successful token refresh recovery
- concurrent OpenSky + ADSBdb usage
- stable FreeRTOS task handling
- crash-free metadata caching

---

# Project Structure

```text
src/
├── main.cpp
├── opensky_client.cpp
├── opensky_auth.cpp
├── postcode_client.cpp
├── aircraft_metadata.cpp
├── display.cpp

include/
├── aircraft.h
├── opensky_client.h
├── opensky_auth.h
├── postcode_client.h
├── aircraft_metadata.h
├── display.h
├── secrets.h
```

---

# Configuration

Create:

```text
include/secrets.h
```

Example:

```cpp
#pragma once

#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"

#define POSTCODE "YOUR_POSTCODE"

#define OPENSKY_CLIENT_ID "YOUR_CLIENT_ID"
#define OPENSKY_CLIENT_SECRET "YOUR_CLIENT_SECRET"
```

---

# Git Ignore

Recommended `.gitignore` entries:

```text
.pio
.vscode/.browse.c_cpp.db*
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch

include/secrets.h
credentials.json
```

---

# APIs Used

## OpenSky Network
Real-time aircraft telemetry.

https://opensky-network.org/

## ADSBdb
Aircraft metadata enrichment.

https://www.adsbdb.com/

## Postcodes.io
UK postcode geolocation.

https://postcodes.io/

---

# Debug Dashboard

After boot, open:

```text
http://ESP32_IP_ADDRESS/debug
```

Example:

```text
http://192.168.1.58/debug
```

The dashboard displays:
- WiFi status
- heap usage
- logs
- runtime telemetry
- aircraft fetch activity
- metadata task activity

---

# Roadmap

## Planned Features
- TFT graphical radar display
- Aircraft icons/classes
- Persistent flash metadata database
- Aircraft history
- Motion trails
- Touch controls
- Setup/configuration webpage
- Local aircraft database
- OTA firmware updates

---

# Version

Current stable milestone:

```text
v0.7-stable
```

---

# License

MIT License

---

# Author

Matt Howard
```
