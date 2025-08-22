# C++ Azure IoT Hub Tracker Simulator - Testing Guide

## Overview
This guide provides step-by-step instructions for building, configuring, and testing the C++ Azure IoT Hub Tracker Simulator. The simulator connects to Azure IoT Hub using MQTT over TLS and sends GPS tracking events.

## Prerequisites

### System Requirements
- **Windows 10/11** (current development environment)
- **Visual Studio 2022** or **MinGW-w64** (GCC 10+)
- **CMake 3.20+**
- **Git**

### Required Dependencies
- **OpenSSL** (for TLS encryption)
- **Paho MQTT C library** (libpaho-mqtt3cs)
- **nlohmann/json** (JSON serialization)
- **Qt6** (optional, for GUI)

### Dependency Installation (Windows)

#### Option 1: vcpkg (Recommended)
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies
.\vcpkg install openssl:x64-windows
.\vcpkg install paho-mqtt:x64-windows
.\vcpkg install nlohmann-json:x64-windows
.\vcpkg install qt6:x64-windows  # Optional for GUI
```

#### Option 2: MSYS2/MinGW
```bash
# Install MSYS2 first, then:
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-openssl
pacman -S mingw-w64-x86_64-paho.mqtt.c
pacman -S mingw-w64-x86_64-nlohmann-json
pacman -S mingw-w64-x86_64-qt6  # Optional
```

## Build Instructions

### Step 1: Clone and Navigate
```bash
cd "C:\Users\afulu\OneDrive - Globaltrack\Documents\Globaltrack\Creative Hub\MQTT Tracking device Simulator"
```

### Step 2: Create Build Directory
```bash
mkdir build
cd build
```

### Step 3: Configure CMake

#### For CLI Only (Headless)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

#### For CLI + Qt GUI
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_QT=ON
```

#### With vcpkg
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Step 4: Build
```bash
cmake --build . --config Release
```

### Expected Build Outputs
- `sim-cli.exe` - Command-line tracker simulator
- `sim-qt.exe` - Qt GUI application (if BUILD_QT=ON)
- `sim-tests.exe` - Unit tests

## Azure IoT Hub Configuration

### Step 1: Create Azure IoT Hub
1. Log into [Azure Portal](https://portal.azure.com)
2. Create new **IoT Hub** resource
3. Note the **Hostname**: `your-iot-hub.azure-devices.net`

### Step 2: Register Device
1. Go to IoT Hub → **Device management** → **Devices**
2. Click **+ Add Device**
3. Enter Device ID (e.g., `SIM-001`)
4. Authentication: **Symmetric Key**
5. Click **Save**
6. Copy the **Primary Key** (base64 encoded)

### Step 3: Configure Simulator
Copy and modify the configuration file:
```bash
cp simulator.toml.example simulator.toml
```

Edit `simulator.toml`:
```toml
[connection]
iot_hub_host = "your-iot-hub.azure-devices.net"  # Replace with your IoT Hub
device_id = "SIM-001"                             # Replace with your device ID
device_key_base64 = "your-base64-device-key"     # Replace with primary key

[simulation]
heartbeat_seconds = 60        # Heartbeat interval
speed_limit_kph = 90.0       # Maximum speed
start_lat = -26.2041         # Starting latitude
start_lon = 28.0473          # Starting longitude  
start_alt = 1720.0           # Starting altitude

[[route]]                    # Define route waypoints
lat = -26.2041
lon = 28.0473

[[route]]
lat = -26.2000
lon = 28.0500

[[geofences]]               # Define geofences
id = "office"
lat = -26.2041
lon = 28.0473
radius_meters = 100.0
```

## Testing Procedures

### Test 1: Unit Tests
```bash
# Run unit tests
cd build
.\sim-tests.exe
```

**Expected Output:**
```
Testing SAS Token generation...
✓ SAS Token format valid
✓ HMAC-SHA256 signature correct
✓ Token expiration time valid
All tests passed!
```

### Test 2: CLI Simulator (Dry Run)
```bash
# Run simulator in dry-run mode (no MQTT connection)
.\sim-cli.exe --config ..\simulator.toml --dry-run
```

**Expected Output:**
```
MQTT Tracker Simulator v1.0.0
Config loaded: ../simulator.toml
Device: SIM-001
Starting position: -26.2041, 28.0473
Dry-run mode: MQTT disabled

[12:45:29] EVENT: ignition_on | Lat: -26.2041, Lon: 28.0473 | Battery: 100%
[12:45:30] EVENT: motion_start | Speed: 0.0 kph | Heading: 0°
[12:46:15] EVENT: geofence_exit | Geofence: office | Speed: 25.3 kph
[12:46:29] HEARTBEAT | Lat: -26.2000, Lon: 28.0500 | Battery: 99%
```

### Test 3: MQTT Connection Test
```bash
# Test actual MQTT connection
.\sim-cli.exe --config ..\simulator.toml --test-connection
```

**Expected Output:**
```
MQTT Tracker Simulator v1.0.0
Testing connection to your-iot-hub.azure-devices.net:8883
SAS Token: SharedAccessSignature sr=your-iot-hub.azure-devices.net%2Fdevices%2FSIM-001&sig=...&se=1724076329
✓ TLS connection established
✓ MQTT CONNECT successful
✓ Topic subscription successful: devices/SIM-001/messages/devicebound/#
✓ Test message published to: devices/SIM-001/messages/events/
Connection test PASSED
```

### Test 4: Full Simulation Run
```bash
# Run full simulation for 5 minutes
.\sim-cli.exe --config ..\simulator.toml --duration 300
```

**Expected Behavior:**
- Connects to Azure IoT Hub via MQTT over TLS
- Sends initial `ignition_on` event
- Follows predefined route with realistic speed/acceleration
- Sends events: `motion_start`, `geofence_exit`, `geofence_enter`, `motion_stop`
- Sends periodic heartbeats (every 60 seconds)
- Battery drains gradually based on activity
- Handles network reconnection automatically

### Test 5: Qt GUI Testing (Optional)
```bash
# Launch GUI application
.\sim-qt.exe
```

**GUI Features to Test:**
1. **Connection Panel**
   - Enter IoT Hub details
   - Test connection button
   - Connection status indicator

2. **Simulation Control**
   - Start/Stop simulation
   - Speed adjustment
   - Manual event triggers

3. **Event Log**
   - Real-time event display
   - JSON message preview
   - Export functionality

4. **Map View**
   - Current device location
   - Route visualization
   - Geofence boundaries
   - Real-time position updates

## Monitoring and Verification

### Azure IoT Hub Monitoring
1. **Azure Portal**: IoT Hub → **Monitoring** → **Device telemetry**
2. **Azure CLI**:
   ```bash
   az iot hub monitor-events --hub-name your-iot-hub --device-id SIM-001
   ```

### Expected JSON Message Format
```json
{
  "deviceId": "SIM-001",
  "ts": "2025-08-19T12:45:29Z",
  "eventType": "motion_start",
  "seq": 1287,
  "loc": {
    "lat": -26.2041,
    "lon": 28.0473,
    "alt": 1720,
    "acc": 12.5
  },
  "speedKph": 18.2,
  "heading": 240,
  "battery": {
    "pct": 83,
    "voltage": 3.98
  },
  "network": {
    "rssi": -72,
    "rat": "LTE"
  },
  "extras": {
    "geofenceId": null
  }
}
```

## Troubleshooting

### Common Build Issues

**Error: "Could not find OpenSSL"**
```bash
# Install OpenSSL via vcpkg or specify path manually
cmake .. -DOPENSSL_ROOT_DIR=C:/vcpkg/installed/x64-windows
```

**Error: "paho-mqtt3cs not found"**
```bash
# Install via vcpkg or pkg-config
vcpkg install paho-mqtt:x64-windows
```

### Common Runtime Issues

**Error: "Connection refused (8883)"**
- Check IoT Hub hostname
- Verify port 8883 is not blocked by firewall
- Ensure device is registered in IoT Hub

**Error: "Authentication failed"**
- Verify device key is correct and base64 encoded
- Check SAS token expiration (default: 1 hour)
- Ensure device ID matches registration

**Error: "SSL/TLS handshake failed"**
- Update OpenSSL to latest version
- Check system time (affects certificate validation)
- Verify CA certificates are installed

## Performance Testing

### Load Testing
```bash
# Simulate multiple devices (10 concurrent)
for i in {1..10}; do
  .\sim-cli.exe --config ..\simulator.toml --device-suffix $i &
done
```

### Memory Testing
```bash
# Run with memory profiling (if available)
.\sim-cli.exe --config ..\simulator.toml --profile-memory
```

### Network Resilience Testing
- Disconnect/reconnect network during simulation
- Verify automatic reconnection
- Check message queuing during disconnection
- Validate no message loss after reconnection

## Success Criteria
✅ **Build Success**: All executables compile without errors  
✅ **Unit Tests**: All crypto and core logic tests pass  
✅ **MQTT Connection**: Successful TLS connection to Azure IoT Hub  
✅ **Authentication**: Valid SAS token generation and acceptance  
✅ **Message Delivery**: JSON messages appear in Azure IoT Hub telemetry  
✅ **Event Generation**: Realistic GPS tracking events based on state machine  
✅ **Network Resilience**: Automatic reconnection after network disruption  
✅ **Resource Usage**: Stable memory usage, no memory leaks  
✅ **GUI Functionality**: Qt interface displays real-time data (if enabled)