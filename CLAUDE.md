# Claude Code Prompt: C++ Azure IoT Hub Tracker Simulator (+ Optional Qt UI)

## Goal
Build a **C++20 tracker simulator** that connects directly to **Azure IoT Hub** using **MQTT over TLS (port 8883)** and authenticates with **SAS tokens** (no Azure SDK).  
The simulator must be **event-driven** (send JSON messages when events occur, plus configurable heartbeat) and architected so the **core logic** can later be compiled for **STM32H** (FreeRTOS/LwIP/mbedTLS).

## Architecture
- **Core (/core)**
  - Pure C++20, no OS/GUI deps
  - State machine for Idle/Driving/Parked/LowBattery
  - Movement & geofence detection
  - Battery drain model
  - JSON serialization (nlohmann/json)
  - Interfaces: `IMqttClient`, `IClock`, `IRng`
- **Networking (/net/mqtt)**
  - Desktop: Paho MQTT C/C++ or libmosquitto + OpenSSL
  - MCU (later): Paho Embedded C or coreMQTT + mbedTLS
- **Crypto (/crypto)**
  - `SasToken` helper (HMAC-SHA256 + Base64 + URL-encode)
- **Platform/desktop**
  - `main_cli.cpp` runner (headless mode)
- **UI (optional /ui/qt)**
  - Qt 6 GUI with connection controls, event log, JSON preview, map view (Leaflet in QWebEngine)

## MQTT / IoT Hub Settings
- Host: `{hub}.azure-devices.net`
- Port: 8883 (TLS)
- ClientId: `{deviceId}`
- Username: `{hub}.azure-devices.net/{deviceId}/?api-version=2021-04-12`
- Password: `SharedAccessSignature sr=...&sig=...&se=...`
- Publish: `devices/{deviceId}/messages/events/`
- Subscribe: `devices/{deviceId}/messages/devicebound/#`

## JSON Payload (example)
```json
{
  "deviceId": "SIM-001",
  "ts": "2025-08-19T12:45:29Z",
  "eventType": "motion_start",
  "seq": 1287,
  "loc": { "lat": -26.2041, "lon": 28.0473, "alt": 1720, "acc": 12.5 },
  "speedKph": 18.2,
  "heading": 240,
  "battery": { "pct": 83, "voltage": 3.98 },
  "network": { "rssi": -72, "rat": "LTE" },
  "extras": { "geofenceId": null }
}
