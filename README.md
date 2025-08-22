# GPS Tracker Simulator with Azure DPS Integration

A comprehensive C++20 GPS tracker simulator that connects to Azure IoT Hub using **Azure Device Provisioning Service (DPS)** with **X.509 certificate authentication**. Designed for embedded portability with desktop development support.

**✅ PRODUCTION READY** - Successfully tested with Azure DPS provisioning and IoT Hub telemetry publishing.

## 🏗️ Architecture Overview

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Simulator     │    │  Azure DPS       │    │  Azure IoT Hub  │
│                 │───▶│  Provisioning    │───▶│   vynd-dev-*    │
│  Device: IMEI   │    │  Service         │    │   azure-*.net   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
        │
        ▼
┌─────────────────┐
│  X.509 Certs    │
│  • device.cert  │
│  • device.key   │
│  • device.chain │
│  • rootCA.pem   │
└─────────────────┘
```

### Key Features
- **Event-driven architecture**: Sends JSON messages only when events occur
- **Azure DPS Integration**: Automatic hub assignment with X.509 certificate authentication
- **Device Twin Support**: Bidirectional configuration management with Azure IoT Hub
- **Legacy Support**: Backward compatible with SAS token connections
- **State machine**: Idle/Driving/Parked/LowBattery states with automatic transitions
- **Geofencing**: Circle-based geofence enter/exit detection
- **Battery simulation**: Realistic drain model with low battery alerts
- **Movement simulation**: GPS coordinate movement with configurable routes
- **Resilient connectivity**: Exponential backoff reconnection and offline message queuing
- **STM32H ready**: Core logic designed for embedded portability

## 📋 Prerequisites

- **Visual Studio 2022** (Windows) or **GCC 11+** (Linux)
- **CMake 3.20+**
- **vcpkg** package manager (recommended)
- **Azure IoT Hub** with Device Provisioning Service configured
- **DeviceCertGenerator** application (included)

### Required Dependencies (via vcpkg)
```bash
vcpkg install eclipse-paho-mqtt-c openssl nlohmann-json
```

### Optional Dependencies
- **Qt6** (for GUI application, when `-DBUILD_QT=ON`)

## 🔧 Complete Setup Guide

### Step 1: Generate Device Certificates

The simulator requires X.509 certificates for secure authentication with Azure DPS.

```bash
# Navigate to certificate generator
cd DeviceCertGenerator

# Generate certificates for your device
# Replace 123456789101112 with your actual device IMEI
./DeviceCertGenerator.exe --imei 123456789101112

# This creates:
# • device/123456789101112/device.cert.pem
# • device/123456789101112/device.key.pem  
# • device/123456789101112/device.chain.pem
# • cert/RootCA-2025.pem (if not exists)
```

**Certificate Files Generated:**
- `device.cert.pem` - Device public certificate
- `device.key.pem` - Device private key ⚠️ **Keep secure!**
- `device.chain.pem` - Certificate chain for Azure DPS
- `RootCA-2025.pem` - Root CA certificate

### Step 2: Configure Azure Device Provisioning Service

1. **Create DPS Instance** in Azure Portal
   - Go to Azure Portal → Create Resource → Device Provisioning Service
   - Choose resource group and region

2. **Add Enrollment Group**:
   - Navigate to your DPS instance → Manage enrollments → Enrollment groups
   - Click **+ Add enrollment group**
   - **Group name**: `Vynd-Dev-Seals` (or your preferred name)
   - **Attestation Type**: Certificate
   - **Certificate Type**: Root Certificate
   - **Primary Certificate**: Upload `DeviceCertGenerator/cert/RootCA-2025.pem`
   - **IoT Hub assignment policy**: Evenly weighted distribution (or custom)
   - **Select IoT hubs**: Choose your target IoT Hub
   - Click **Save**

3. **Note your ID Scope**:
   - Go to DPS Overview page
   - Copy the **ID Scope** (format: `0ne00XXXXXX`)

### Step 3: Update Configuration

Edit `simulator.toml` with your device and DPS settings:

```toml
# MQTT Tracker Simulator Configuration

[dps]
id_scope = "0ne00FBC8CA"                           # Your DPS ID Scope from Azure Portal
imei = "123456789101112"                           # Device IMEI (must match certificate CN)
device_cert_base_path = "DeviceCertGenerator/device"  # Base path to device certificates
root_ca_path = "DeviceCertGenerator/cert/RootCA-2025.pem"  # Root CA certificate
verify_server_cert = false                         # Set to true for production

[simulation]
heartbeat_seconds = 30      # Heartbeat message interval (seconds)
speed_limit_kph = 90.0     # Speed limit for violation detection
start_lat = -26.2041       # Starting latitude (Johannesburg example)
start_lon = 28.0473        # Starting longitude
start_alt = 1720.0         # Starting altitude (meters)

# Optional: Define route waypoints for automated driving
[[route]]
lat = -26.2041
lon = 28.0473

[[route]]
lat = -26.2000
lon = 28.0500

[[route]]
lat = -26.1950
lon = 28.0520

[[route]]
lat = -26.1920
lon = 28.0480

# Optional: Define geofences for enter/exit events
[[geofences]]
id = "office"
lat = -26.2041
lon = 28.0473
radius_meters = 100.0

[[geofences]]
id = "warehouse"
lat = -26.1920
lon = 28.0480
radius_meters = 150.0
```

### Step 4: Build the Simulator

```bash
# Create and navigate to build directory
mkdir build
cd build

# Configure project with CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build the project (Release configuration recommended)
cmake --build . --config Release

# Copy configuration to build directory (important!)
cp ../simulator.toml .

# Copy certificates to build directory structure
mkdir -p DeviceCertGenerator/device/123456789101112
mkdir -p DeviceCertGenerator/cert

# Copy certificate files (replace IMEI as needed)
cp ../DeviceCertGenerator/device/123456789101112/* DeviceCertGenerator/device/123456789101112/
cp ../DeviceCertGenerator/cert/RootCA-2025.pem DeviceCertGenerator/cert/
```

### Step 5: Run the Simulator

```bash
# Navigate to build directory
cd build

# Run simulator in interactive mode
./Release/sim-cli.exe

# Alternative running modes:
./Release/sim-cli.exe --drive 15        # 15-minute automated driving simulation
./Release/sim-cli.exe --spike 20        # Generate 20 random events quickly
./Release/sim-cli.exe --headless        # Run without user interaction
./Release/sim-cli.exe --config custom.toml  # Use custom configuration file
```

## 📊 Expected Output

### Successful DPS Connection Flow
```
Starting MQTT Tracker Simulator
Connection Mode: DPS (X.509 Certificates)
ID Scope: 0ne00FBC8CA
IMEI: 123456789101112
Device Cert: DeviceCertGenerator/device/123456789101112/device.cert.pem
Heartbeat: 30s

[Simulator] Initiating DPS-based connection
[Simulator] Device: 123456789101112 | Scope: 0ne00FBC8CA
[DPS Connection Manager] Starting DPS provisioning for device: 123456789101112
[DPS] Starting provisioning for device: 123456789101112
[MQTT] Connecting with TLS to global.azure-devices-provisioning.net:8883

[DPS] Successfully provisioned device 123456789101112 to hub vynd-dev-migration-iot.azure-devices.net
[DPS Connection Manager] Provisioning successful. Connecting to IoT Hub: vynd-dev-migration-iot.azure-devices.net
[MQTT] Connecting with TLS to vynd-dev-migration-iot.azure-devices.net:8883
[Simulator] ✅ DPS provisioning successful!
[Simulator] Assigned Hub: vynd-dev-migration-iot.azure-devices.net

Connected to IoT Hub

=== EVENT GENERATED ===
Type: heartbeat
Sequence: 1
Timestamp: 2025-08-21T13:15:30.123Z
JSON Payload:
{
  "deviceId": "123456789101112",
  "ts": "2025-08-21T13:15:30.123Z",
  "eventType": "heartbeat",
  "seq": 1,
  "loc": { "lat": -26.2041, "lon": 28.0473, "alt": 1720, "acc": 12.5 },
  "speedKph": 0.0,
  "heading": 0,
  "battery": { "pct": 99, "voltage": 4.2 },
  "network": { "rssi": -72, "rat": "LTE" }
}
📤 Publishing to topic: devices/123456789101112/messages/events/
✅ Published to Azure IoT Hub
========================
```

### Legacy SAS Token Mode (Backward Compatibility)
```
Starting MQTT Tracker Simulator
Connection Mode: Legacy (SAS Token)
Device ID: SIM-001
IoT Hub Host: your-hub.azure-devices.net
Heartbeat: 30s
[Simulator] Using legacy SAS token authentication

Connected to IoT Hub
```

## 🎮 Interactive Commands

Once the simulator is running, use these commands:

| Command | Description | Example Usage |
|---------|-------------|---------------|
| `i` | Toggle ignition (on/off) | Generates `ignition_on` or `ignition_off` events |
| `s` | Set vehicle speed (km/h) | Triggers `motion_start`, `motion_stop`, or `speed_violation` |
| `b` | Set battery percentage | Can trigger `low_battery` events |
| `d` | Start automated driving simulation | Simulates realistic driving with GPS movement |
| `p` | Generate spike of random events | Creates burst of test events for load testing |
| `q` | Quit simulator | Graceful shutdown with cleanup |

### Example Interactive Session
```
> i
[User] Ignition turned ON
=== EVENT GENERATED ===
Type: ignition_on
...

> s
Enter speed (km/h): 65
[User] Speed set to 65.0 km/h
=== EVENT GENERATED ===
Type: motion_start
...

> d
[User] Starting automated driving simulation (10.0 minutes)
[Driving] Route progress: 15% - Speed: 45 km/h
=== EVENT GENERATED ===
Type: geofence_exit
...
```

## 📝 Generated Event Types

The simulator generates realistic GPS tracker events based on real-world scenarios:

| Event Type | Trigger Condition | Additional Data |
|------------|------------------|-----------------|
| **`heartbeat`** | Periodic timer (configurable interval) | Standard telemetry |
| **`motion_start`** | Vehicle speed > 0 km/h | Speed, heading |
| **`motion_stop`** | Vehicle speed = 0 km/h | Final location |
| **`geofence_enter`** | GPS enters defined geofence | Geofence ID, entry point |
| **`geofence_exit`** | GPS exits defined geofence | Geofence ID, exit point |
| **`speed_violation`** | Speed > configured limit | Speed limit, measured speed |
| **`low_battery`** | Battery ≤ 20% | Battery level, voltage |
| **`ignition_on`** | Engine started (manual/automatic) | Engine status change |
| **`ignition_off`** | Engine stopped (manual/automatic) | Engine status change |

### Sample JSON Message Structure
```json
{
  "deviceId": "123456789101112",
  "ts": "2025-08-21T13:15:30.123Z",
  "eventType": "geofence_enter",
  "seq": 42,
  "loc": {
    "lat": -26.2041,
    "lon": 28.0473,
    "alt": 1720.0,
    "acc": 12.5
  },
  "speedKph": 25.3,
  "heading": 180,
  "battery": {
    "pct": 85,
    "voltage": 4.05
  },
  "network": {
    "rssi": -68,
    "rat": "LTE"
  },
  "extras": {
    "geofenceId": "office"
  }
}
```

## 🔄 Complete DPS Connection Flow

1. **Configuration Loading** - Parse TOML file and validate DPS parameters
2. **Certificate Validation** - Verify all certificate files exist and are readable
3. **DPS Connection** - Establish MQTT over TLS to `global.azure-devices-provisioning.net:8883`
4. **Device Registration** - Send registration request with device IMEI and certificate
5. **Polling Assignment** - Wait for Azure DPS to assign device to specific IoT Hub
6. **Hub Assignment Response** - Receive assigned IoT Hub hostname
7. **IoT Hub Connection** - Connect to assigned hub using same certificate
8. **Subscription Setup** - Subscribe to cloud-to-device message topics
9. **Telemetry Publishing** - Begin sending device events to IoT Hub
10. **Resilient Operation** - Handle disconnections with automatic reconnection

### DPS Message Flow Details
```
1. Device → DPS: PUT /registrations/{registrationId}/register
2. DPS → Device: 202 Accepted {"operationId": "xxx", "status": "assigning"}
3. Device → DPS: GET /registrations/{registrationId}/operations/{operationId}
4. DPS → Device: 200 OK {"status": "assigned", "assignedHub": "hub.azure-devices.net"}
5. Device → IoT Hub: MQTT Connect with X.509 certificate
6. Device ↔ IoT Hub: Bidirectional telemetry and command flow
```

## 🛠️ Troubleshooting Guide

### Certificate Issues

#### Problem: Certificate File Not Found
```
[Config] Warning: Device certificate not found: DeviceCertGenerator/device/123456789101112/device.chain.pem
[Error] Device certificate not found: DeviceCertGenerator/device/123456789101112/device.chain.pem
```
**Solution:**
- Verify certificates exist in correct directory structure
- Ensure IMEI in configuration matches certificate directory name exactly
- Copy certificates to build directory: `cp -r DeviceCertGenerator build/`

#### Problem: Invalid Certificate Chain
```
[MQTT] Certificate chain validation failed
```
**Solution:**
- Regenerate certificates using DeviceCertGenerator
- Ensure Root CA certificate is uploaded to Azure DPS enrollment group
- Verify certificate chain: `openssl verify -CAfile rootCA.pem device.chain.pem`

### DPS Connection Issues

#### Problem: DPS Connection Timeout
```
[MQTT] Connection failed to global.azure-devices-provisioning.net:8883
[DPS Connection Manager] DPS connection failed: Connection timeout
```
**Solutions:**
- Check internet connectivity: `ping global.azure-devices-provisioning.net`
- Verify firewall allows outbound HTTPS (443) and MQTT-TLS (8883)
- Ensure system time is correct (affects certificate validity)

#### Problem: Device Not Found in DPS
```
[DPS] Device assignment failed: Device not found
```
**Solutions:**
- Verify DPS ID Scope is correct in configuration
- Ensure enrollment group exists in Azure DPS
- Check Root CA certificate is properly uploaded to DPS
- Verify IMEI matches device certificate Common Name (CN)

#### Problem: Hub Assignment Failed
```
[DPS] Received assignment error: Device assignment failed
```
**Solutions:**
- Check Azure DPS enrollment group configuration
- Verify IoT Hub is linked to DPS instance
- Ensure IoT Hub has available device capacity
- Review Azure DPS logs in Azure Portal

### Configuration Issues

#### Problem: Simulator Uses Legacy Mode Instead of DPS
```
Connection Mode: Legacy (SAS Token)
```
**Solutions:**
- Ensure `[dps]` section exists in simulator.toml
- Verify all required DPS parameters are provided:
  - `id_scope`
  - `imei`
  - `device_cert_base_path`
  - `root_ca_path`
- Delete old simulator.toml files in build directory
- Check for syntax errors in TOML file

#### Problem: Certificate Path Construction Failed
```
[Config] Warning: Device certificate not found
Has DPS Config: NO
```
**Solution:**
- Ensure IMEI is specified before device_cert_base_path in TOML file
- Verify certificate directory structure matches: `device/{IMEI}/`

### Network and Connectivity Issues

#### Problem: Messages Not Reaching IoT Hub
```
⚠️  Offline - Message queued for later delivery
```
**Solutions:**
- Verify device exists in target IoT Hub
- Check device status is "Enabled" in Azure Portal
- Ensure device certificate is valid and not expired
- Monitor Azure IoT Hub metrics for connection attempts

#### Problem: Frequent Reconnections
```
Attempting to reconnect (attempt 3)...
```
**Solutions:**
- Check network stability
- Verify certificate validity period
- Review Azure IoT Hub connection policies
- Consider increasing heartbeat interval to reduce traffic

### Build and Development Issues

#### Problem: CMake Cannot Find Dependencies
```
CMake Error: Could not find a package configuration file provided by "eclipse-paho-mqtt-c"
```
**Solutions:**
- Install dependencies via vcpkg: `vcpkg install eclipse-paho-mqtt-c openssl nlohmann-json`
- Use vcpkg toolchain: `cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg.cmake`
- Verify environment variables: `VCPKG_ROOT`

#### Problem: Compilation Errors
```
error: 'class Simulator' has no member named 'hasDpsConfig'
```
**Solution:**
- Ensure C++20 compiler support: `cmake .. -DCMAKE_CXX_STANDARD=20`
- Clean and rebuild: `rm -rf build && mkdir build && cd build && cmake ..`

## 🏢 Production Deployment

### Security Best Practices

1. **Enable Certificate Verification**
   ```toml
   [dps]
   verify_server_cert = true  # Enable for production
   ```

2. **Secure Private Key Storage**
   - Store `device.key.pem` in secure location
   - Use appropriate file permissions: `chmod 600 device.key.pem`
   - Consider hardware security modules (HSM) for embedded devices

3. **Certificate Management**
   - Implement certificate rotation process
   - Monitor certificate expiration dates
   - Use intermediate certificates for better security

4. **Network Security**
   ```bash
   # Allow outbound MQTT over TLS and HTTPS
   iptables -A OUTPUT -p tcp --dport 8883 -j ACCEPT  # MQTT-TLS
   iptables -A OUTPUT -p tcp --dport 443 -j ACCEPT   # HTTPS (DPS)
   ```

### Environment Variables (Alternative Configuration)
```bash
# Legacy SAS Token mode (backward compatibility)
export IOT_HOST="your-hub.azure-devices.net"
export DEVICE_ID="SIM-001"
export DEVICE_KEY="your-base64-shared-access-key"
export HEARTBEAT_SEC="60"
export SPEED_LIMIT_KPH="120"
```

### Service Deployment (systemd example)
```ini
[Unit]
Description=GPS Tracker Simulator
After=network.target

[Service]
Type=simple
User=simulator
WorkingDirectory=/opt/gps-simulator
ExecStart=/opt/gps-simulator/sim-cli --headless --drive 1440
Restart=always
RestartSec=30

[Install]
WantedBy=multi-user.target
```

## 📚 Architecture Details

### Core Components

| Component | Purpose | Dependencies |
|-----------|---------|--------------|
| **`core/Simulator.hpp`** | Main simulation engine and orchestration | Core interfaces |
| **`core/DpsProvisioning.hpp`** | Azure DPS provisioning workflow | MQTT client |
| **`core/DpsConnectionManager.hpp`** | High-level connection management | DPS + MQTT |
| **`net/mqtt/PahoMqttClient.hpp`** | MQTT over TLS implementation | Paho MQTT C |
| **`crypto/SasToken.hpp`** | Legacy SAS token generation | OpenSSL |
| **`platform/desktop/TomlConfig.hpp`** | Configuration file parsing | Standard library |

### Dependency Injection Pattern
```cpp
// Platform-independent core
auto mqttClient = std::make_shared<PahoMqttClient>();    // Desktop MQTT
auto clock = std::make_shared<SystemClock>();            // System time
auto rng = std::make_shared<StandardRng>();              // Standard RNG

// For embedded: replace with platform-specific implementations
// auto mqttClient = std::make_shared<EmbeddedMqttClient>(); // FreeRTOS
// auto clock = std::make_shared<HalClock>();               // STM32 HAL
// auto rng = std::make_shared<TrngRng>();                  // Hardware RNG
```

### Platform Independence Design
The core simulation logic is architected for embedded portability:

- **No OS Dependencies** - Pure C++20 with interface abstractions
- **Minimal Memory Usage** - Stack-allocated objects, controlled heap usage
- **Real-time Friendly** - Event-driven, no blocking operations
- **Hardware Abstraction** - Dependency injection for platform-specific code

### STM32H Portability Roadmap
```cpp
// Desktop Implementation      →  STM32H Implementation
PahoMqttClient                 →  LwIPMqttClient + mbedTLS
OpenSSL (SasToken)            →  mbedTLS HMAC-SHA256
SystemClock                   →  HAL_GetTick() wrapper
StandardRng                   →  Hardware TRNG (RNG peripheral)
std::filesystem               →  FatFS or LittleFS
```

## 🧪 Testing and Validation

### Unit Tests
```bash
# Build and run unit tests
cmake --build build --target sim-tests
./build/Release/sim-tests.exe

# Test individual components
./build/Release/sim-tests --test-sas-token
./build/Release/sim-tests --test-geofencing
```

### Integration Testing
```bash
# Short validation test (5 minutes)
./build/Release/sim-cli.exe --headless --drive 5

# Load testing with event bursts
./build/Release/sim-cli.exe --spike 100

# Long-duration stability test (24 hours)
nohup ./build/Release/sim-cli.exe --headless --drive 1440 &
```

### Azure IoT Hub Verification
1. **Device Explorer** - Monitor incoming messages in Azure Portal
2. **IoT Hub Metrics** - Check connection and message counts
3. **Device Twin** - Verify device properties and status
4. **DPS Logs** - Review provisioning attempts in DPS Overview

### Performance Benchmarking
```bash
# Message throughput test
./build/Release/sim-cli.exe --spike 1000 --headless

# Memory usage monitoring
valgrind --tool=memcheck ./build/Release/sim-cli.exe --headless --drive 30

# CPU profiling
perf record ./build/Release/sim-cli.exe --headless --drive 10
perf report
```

## 📖 Configuration Reference

### Complete TOML Configuration
```toml
# MQTT Tracker Simulator Configuration

[dps]
# Azure Device Provisioning Service settings
id_scope = "0ne00FBC8CA"                    # DPS ID Scope from Azure Portal
imei = "123456789101112"                    # Device IMEI (15 digits, matches certificate CN)
device_cert_base_path = "DeviceCertGenerator/device"  # Base path to device certificates
root_ca_path = "DeviceCertGenerator/cert/RootCA-2025.pem"  # Root CA certificate path
verify_server_cert = false                 # Server certificate verification (set true for production)

[connection]
# Legacy SAS token configuration (backward compatibility)
connection_string = "HostName=hub.azure-devices.net;DeviceId=SIM-001;SharedAccessKey=base64key"
# OR individual components:
iot_hub_host = "your-hub.azure-devices.net"
device_id = "SIM-001"
device_key_base64 = "your-base64-shared-access-key"

[simulation]
# Simulation behavior parameters
heartbeat_seconds = 30                      # Heartbeat message interval
speed_limit_kph = 90.0                     # Speed limit for violation events
start_lat = -26.2041                       # Initial GPS latitude (decimal degrees)
start_lon = 28.0473                        # Initial GPS longitude (decimal degrees)
start_alt = 1720.0                         # Initial altitude (meters)

# Route waypoints for automated driving (optional)
[[route]]
lat = -26.2041      # Johannesburg CBD
lon = 28.0473

[[route]]
lat = -26.2000      # North movement
lon = 28.0500

[[route]]
lat = -26.1950      # Northeast
lon = 28.0520

[[route]]
lat = -26.1920      # Return south
lon = 28.0480

# Geofence definitions for enter/exit events (optional)
[[geofences]]
id = "office"               # Unique geofence identifier
lat = -26.2041              # Center latitude
lon = 28.0473               # Center longitude
radius_meters = 100.0       # Radius in meters

[[geofences]]
id = "warehouse"
lat = -26.1920
lon = 28.0480
radius_meters = 150.0

[[geofences]]
id = "home"
lat = -26.1850
lon = 28.0400
radius_meters = 200.0
```

### Parameter Validation Rules

| Parameter | Type | Range/Format | Required |
|-----------|------|--------------|----------|
| `id_scope` | String | `0ne[0-9A-F]{8}` | Yes (DPS mode) |
| `imei` | String | 15 digits | Yes (DPS mode) |
| `heartbeat_seconds` | Integer | 10-3600 | No (default: 60) |
| `speed_limit_kph` | Float | 1.0-300.0 | No (default: 90.0) |
| `start_lat` | Float | -90.0 to 90.0 | No (default: -26.2041) |
| `start_lon` | Float | -180.0 to 180.0 | No (default: 28.0473) |
| `verify_server_cert` | Boolean | true/false | No (default: true) |

## 🚀 Command Line Reference

### sim-cli.exe Options
```bash
./sim-cli.exe [OPTIONS]

OPTIONS:
  --config FILE         Configuration file path (default: simulator.toml)
  --drive MINUTES       Start automated driving simulation (default: 10.0)
  --spike COUNT         Generate burst of random events (default: 10)
  --headless            Run without user interaction
  --help                Show help message and exit

EXAMPLES:
  ./sim-cli.exe                              # Interactive mode with default config
  ./sim-cli.exe --config production.toml    # Use custom configuration
  ./sim-cli.exe --drive 30                  # 30-minute automated driving
  ./sim-cli.exe --spike 50 --headless       # Generate 50 events and exit
  ./sim-cli.exe --headless --drive 1440     # 24-hour simulation (production)
```

### Exit Codes
| Code | Description |
|------|-------------|
| 0 | Success |
| 1 | Configuration error |
| 2 | Connection failure |
| 3 | Certificate error |
| 4 | Runtime error |

## 🤝 Support and Maintenance

### Logging Levels
The simulator provides comprehensive logging for debugging and monitoring:

- **Info**: Normal operation events (connection status, message publishing)
- **Warning**: Non-critical issues (offline queuing, retry attempts)
- **Error**: Critical failures (certificate issues, connection failures)
- **Debug**: Detailed protocol information (DPS messages, MQTT topics)

### Monitoring Integration
For production deployment, consider integrating with:
- **Azure Monitor** - IoT Hub telemetry and alerts
- **Application Insights** - Application performance monitoring
- **Log Analytics** - Centralized log collection and analysis

### Common Support Scenarios

1. **New Device Onboarding**
   - Generate certificates with DeviceCertGenerator
   - Update simulator.toml with new IMEI
   - Verify DPS enrollment group includes device

2. **Certificate Renewal**
   - Regenerate certificates before expiration
   - Update certificate paths in configuration
   - Test connection before deployment

3. **Scaling to Multiple Devices**
   - Use different IMEI for each device instance
   - Implement device management system
   - Monitor Azure IoT Hub device limits

### Troubleshooting Checklist
- [ ] Certificate files exist and are readable
- [ ] IMEI matches certificate Common Name
- [ ] DPS ID Scope is correct
- [ ] Azure DPS enrollment group configured
- [ ] Root CA uploaded to DPS
- [ ] Network allows outbound 8883 and 443
- [ ] System time is synchronized
- [ ] IoT Hub has device capacity

## 📄 License and Contributing

This project follows MSRA C++ coding standards and is designed for embedded GPS tracking applications with Azure IoT Hub integration.

### Contributing Guidelines
1. Follow MSRA C++ coding standards
2. Maintain embedded portability (no OS dependencies in core/)
3. Add comprehensive unit tests for new features
4. Update documentation for API changes
5. Test with both DPS and legacy authentication modes

### Code Quality Standards
- **Documentation**: All public APIs documented with Doxygen comments
- **Error Handling**: Comprehensive error checking and logging
- **Memory Safety**: RAII patterns, smart pointers, bounded arrays
- **Threading**: Thread-safe design suitable for embedded RTOS
- **Testing**: Unit test coverage >90% for core components