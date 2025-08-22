# MQTT Tracker Simulator - Complete Testing Guide

## 📋 Table of Contents
- [Overview](#overview)
- [Prerequisites & Dependencies](#prerequisites--dependencies)
- [Build Instructions](#build-instructions)
- [Test Environment Setup](#test-environment-setup)
- [Testing Procedures](#testing-procedures)
- [Performance & Load Testing](#performance--load-testing)
- [Troubleshooting](#troubleshooting)
- [Success Criteria](#success-criteria)

## Overview

This comprehensive testing guide covers building, configuring, and validating the C++ Azure IoT Hub Tracker Simulator. The simulator supports both **Azure Device Provisioning Service (DPS)** with X.509 certificates and **legacy SAS token** authentication.

**Key Testing Areas:**
- 🔧 Build system validation
- 🔐 Authentication methods (DPS + SAS)
- 📡 MQTT connectivity and messaging
- 🎯 Event generation and state machine
- 🗺️ GPS simulation and geofencing
- 🔋 Battery modeling and power management
- 🎮 Interactive and automated testing modes

---

## Prerequisites & Dependencies

### System Requirements
- **Windows 10/11** (Primary development environment)
- **Visual Studio 2022** with C++ development tools
- **CMake 3.20+**
- **Git 2.30+**
- **vcpkg package manager** (Recommended)

### Required Dependencies

| Package | Purpose | Installation Method |
|---------|---------|-------------------|
| **OpenSSL 3.0+** | TLS encryption & SAS token HMAC | `vcpkg install openssl:x64-windows` |
| **Eclipse Paho MQTT C** | MQTT client library | `vcpkg install eclipse-paho-mqtt-c:x64-windows` |
| **nlohmann/json** | JSON serialization | `vcpkg install nlohmann-json:x64-windows` |
| **Qt6** (Optional) | GUI application | `vcpkg install qt6:x64-windows` |

### Dependency Installation

#### Method 1: vcpkg (Recommended)
```bash
# Install vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install required packages
.\vcpkg install openssl:x64-windows
.\vcpkg install eclipse-paho-mqtt-c:x64-windows  
.\vcpkg install nlohmann-json:x64-windows

# Optional: GUI support
.\vcpkg install qt6:x64-windows
```

#### Method 2: Package Verification
```bash
# Verify installations
C:\vcpkg\vcpkg.exe list | findstr paho
C:\vcpkg\vcpkg.exe list | findstr openssl
C:\vcpkg\vcpkg.exe list | findstr nlohmann

# Expected output:
# eclipse-paho-mqtt-c:x64-windows    1.3.15    Paho project provides...
# openssl:x64-windows                3.5.2     OpenSSL is an open source...
# nlohmann-json:x64-windows          3.12.0    JSON for Modern C++
```

---

## Build Instructions

### Step 1: Environment Setup
```bash
# Navigate to project directory
cd "C:\Users\afulu\OneDrive - Globaltrack\Documents\Globaltrack\Creative Hub\MQTT Tracking device Simulator"

# Clean previous builds (if any)
if exist build rmdir /s /q build
mkdir build
cd build
```

### Step 2: CMake Configuration

#### Standard Configuration (CLI Only)
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH="C:\vcpkg\installed\x64-windows"
```

#### With Qt GUI Support
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH="C:\vcpkg\installed\x64-windows" -DBUILD_QT=ON
```

#### With Tests Enabled (Default)
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH="C:\vcpkg\installed\x64-windows" -DBUILD_TESTS=ON
```

### Step 3: Build Compilation
```bash
# Build all targets in Release mode
cmake --build . --config Release

# Alternative: Build specific targets
cmake --build . --target sim-cli --config Release
cmake --build . --target sim-tests --config Release
cmake --build . --target sim-qt --config Release  # If BUILD_QT=ON
```

### Step 4: DLL Dependencies (Critical!)
```powershell
# Copy required DLLs to Release directory
Copy-Item 'C:\vcpkg\installed\x64-windows\bin\paho-mqtt3as.dll' 'Release\'
Copy-Item 'C:\vcpkg\installed\x64-windows\bin\libcrypto-3-x64.dll' 'Release\'
Copy-Item 'C:\vcpkg\installed\x64-windows\bin\libssl-3-x64.dll' 'Release\'
```

### Expected Build Outputs
After successful build, verify these files exist:
```
build/Release/
├── sim-cli.exe              # Main CLI application
├── sim-tests.exe            # Unit test executable  
├── sim-qt.exe               # Qt GUI (if enabled)
├── paho-mqtt3as.dll         # MQTT client library
├── libcrypto-3-x64.dll      # OpenSSL crypto
└── libssl-3-x64.dll         # OpenSSL SSL/TLS
```

---

## Test Environment Setup

### Azure DPS Configuration (Recommended)

#### Step 1: Azure Portal Setup
1. **Create Device Provisioning Service**
   - Azure Portal → Create Resource → Device Provisioning Service
   - Note the **ID Scope** (format: `0ne00XXXXXX`)

2. **Create Enrollment Group**
   - DPS → Manage enrollments → Enrollment groups → + Add
   - **Group name**: `Vynd-Dev-Seals`
   - **Attestation Type**: Certificate
   - **Primary Certificate**: Upload `DeviceCertGenerator/cert/RootCA-2025.pem`
   - **IoT Hub assignment**: Select your target IoT Hub

#### Step 2: Certificate Generation
```bash
# Navigate to certificate generator
cd DeviceCertGenerator

# Generate device certificates (replace IMEI as needed)
# This should already exist from setup, verify:
ls device/123456789101112/
# Expected files:
# device.cert.pem
# device.key.pem
# device.chain.pem
```

#### Step 3: Configuration File
```bash
# Copy and verify configuration
cp simulator.toml.example simulator.toml

# Verify DPS configuration in simulator.toml:
```
```toml
[dps]
id_scope = "0ne00FBC8CA"                           # Your DPS ID Scope
imei = "123456789101112"                           # Device IMEI
device_cert_base_path = "DeviceCertGenerator/device"
root_ca_path = "DeviceCertGenerator/cert/RootCA-2025.pem"
verify_server_cert = false                         # Set true for production

[simulation]
heartbeat_seconds = 30
speed_limit_kph = 90.0
start_lat = -26.2041
start_lon = 28.0473
start_alt = 1720.0
```

### Legacy SAS Token Configuration (Alternative)

For backward compatibility testing:
```toml
[connection]
iot_hub_host = "your-hub.azure-devices.net"
device_id = "SIM-001" 
device_key_base64 = "your-base64-shared-access-key"
```

---

## Testing Procedures

### Test 1: Unit Tests
```bash
cd build
.\Release\sim-tests.exe
```

**Expected Output:**
```
Running SAS Token Tests...
Testing URL encoding...
URL encoding tests passed!
Testing Base64 encoding/decoding...
Base64 tests passed!
Testing SAS token generation...
Generated token: SharedAccessSignature sr=test-hub.azure-devices.net%2Fdevices%2Ftest-device&sig=h4%2BOrhY6rnm5MOcJ5VVn3S%2FEN5oVduvy5VFfyXlnc4U%3D&se=1234567890
SAS token generation tests passed!
Testing SAS token config...
SAS token config tests passed!

All tests passed successfully!
```

### Test 2: CLI Help & Version
```bash
# Verify CLI functionality
.\Release\sim-cli.exe --help
```

**Expected Output:**
```
Usage: sim-cli.exe [options]
Options:
  --config [file]    Configuration file (default: simulator.toml)
  --drive [minutes]  Start a driving simulation (default: 10 minutes)
  --spike [count]    Generate a spike of events (default: 10)
  --headless         Run without user interaction
  --help             Show this help message

Configuration file format (TOML):
  [connection]
  iot_hub_host = "your-hub.azure-devices.net"
  device_id = "your-device-id"
  device_key_base64 = "your-base64-key"
```

### Test 3: DPS Connection Test
```bash
# Test DPS provisioning and connection (5-event spike)
.\Release\sim-cli.exe --spike 5
```

**Expected Output (DPS Mode):**
```
Starting MQTT Tracker Simulator
Connection Mode: DPS (X.509 Certificates)
ID Scope: 0ne00FBC8CA
IMEI: 123456789101112
Device Cert: DeviceCertGenerator/device/123456789101112/device.cert.pem
Heartbeat: 30s

[Simulator] Initiating DPS-based connection
[DPS] Starting provisioning for device: 123456789101112
[MQTT] Connecting with TLS to global.azure-devices-provisioning.net:8883
[DPS] Successfully provisioned device to hub: vynd-dev-migration-iot.azure-devices.net
[Simulator] ✅ DPS provisioning successful!

Generating spike of 5 events...
=== EVENT GENERATED ===
Type: ignition_on
Sequence: 1
JSON Payload: { "deviceId": "123456789101112", ... }
✅ Published to Azure IoT Hub
```

### Test 4: Headless Mode Operation
```bash
# Run in headless mode for 60 seconds
timeout 60 .\Release\sim-cli.exe --headless
```

**Expected Behavior:**
- ✅ DPS provisioning completes successfully
- ✅ Connects to assigned IoT Hub
- ✅ Device Twin synchronization
- ✅ Periodic heartbeat events
- ✅ Geofence enter/exit events
- ✅ Automatic state transitions

### Test 5: Driving Simulation
```bash
# 2-minute automated driving test
.\Release\sim-cli.exe --drive 2
```

**Expected Events Sequence:**
1. **ignition_on** - Engine start
2. **motion_start** - Begin movement  
3. **geofence_exit** - Leave starting area
4. **heartbeat** - Periodic status (every 30s)
5. **geofence_enter** - Enter new area
6. **motion_stop** - End movement
7. **ignition_off** - Engine stop

### Test 6: Interactive Mode
```bash
# Launch interactive simulator
.\Release\sim-cli.exe
```

**Interactive Commands:**
```
> i        # Toggle ignition
> s        # Set speed (km/h)
> b        # Set battery percentage  
> d        # Start driving simulation
> p        # Generate event spike
> q        # Quit
```

### Test 7: Configuration Validation
```bash
# Test with different config files
.\Release\sim-cli.exe --config simulator.toml.example --spike 1

# Test with missing config (should use defaults)
.\Release\sim-cli.exe --config nonexistent.toml --spike 1
```

### Test 8: Qt GUI Testing (If Built)
```bash
# Launch GUI application
.\Release\sim-qt.exe
```

**GUI Test Checklist:**
- [ ] Connection status panel shows DPS mode
- [ ] Device certificate paths are loaded
- [ ] Real-time event log displays JSON messages
- [ ] Map view shows device location
- [ ] Manual controls (ignition, speed, battery) work
- [ ] Configuration can be modified and saved

---

## Performance & Load Testing

### Memory Usage Testing
```bash
# Monitor memory usage during operation
.\Release\sim-cli.exe --headless --drive 10 &
# Monitor with Task Manager or Performance Toolkit
```

### Message Throughput Testing  
```bash
# High-frequency event generation
.\Release\sim-cli.exe --spike 100

# Rapid consecutive spikes
for /L %i in (1,1,5) do (
    .\Release\sim-cli.exe --spike 50
    timeout /t 5
)
```

### Network Resilience Testing
```bash
# Long-duration test with network interruptions
.\Release\sim-cli.exe --headless --drive 60
# Manually disconnect/reconnect network during test
# Verify automatic reconnection and message queuing
```

### Multi-Device Simulation
```bash
# Simulate multiple devices (requires different IMEIs)
start .\Release\sim-cli.exe --config device1.toml --headless
start .\Release\sim-cli.exe --config device2.toml --headless  
start .\Release\sim-cli.exe --config device3.toml --headless
```

---

## Azure Monitoring & Verification

### Method 1: Azure Portal
1. Navigate to your IoT Hub → **Device management** → **Devices**
2. Click on device → **Device twin** (verify configuration sync)
3. Go to **Monitoring** → **Metrics** → **Telemetry messages sent**
4. Check **Device-to-cloud messages** count

### Method 2: Azure CLI
```bash
# Real-time message monitoring
az iot hub monitor-events --hub-name your-hub-name --device-id 123456789101112

# Device twin monitoring  
az iot hub device-twin show --hub-name your-hub-name --device-id 123456789101112
```

### Method 3: Azure IoT Explorer
Download and use **Azure IoT Explorer** for visual monitoring:
- Real-time telemetry viewing
- Device twin property management
- Direct method invocation
- Message routing verification

### Expected JSON Message Format
```json
{
  "deviceId": "123456789101112",
  "ts": "2025-08-22T10:30:45.123Z",
  "eventType": "motion_start",
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

---

## Troubleshooting

### Build Issues

#### Error: "Could not find eclipse-paho-mqtt-c"
```bash
# Solution 1: Install via vcpkg
C:\vcpkg\vcpkg.exe install eclipse-paho-mqtt-c:x64-windows

# Solution 2: Verify CMake toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

#### Error: Missing DLLs at Runtime
```bash
# Copy required DLLs to Release directory
Copy-Item 'C:\vcpkg\installed\x64-windows\bin\*.dll' 'build\Release\'

# Verify DLL dependencies
dumpbin /dependents build\Release\sim-cli.exe
```

#### Error: CMake configuration fails
```bash
# Clean and reconfigure
rmdir /s /q build
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH="C:\vcpkg\installed\x64-windows"
```

### Runtime Issues

#### Error: Program exits immediately with no output
**Cause:** Missing DLL dependencies  
**Solution:** Copy required DLLs (see Step 4 in Build Instructions)

#### Error: "Certificate file not found"
```bash
# Verify certificate structure
ls DeviceCertGenerator/device/123456789101112/
# Should contain: device.cert.pem, device.key.pem, device.chain.pem

# Copy certificates to build directory if needed
xcopy DeviceCertGenerator build\DeviceCertGenerator\ /E /I
```

#### Error: DPS connection fails
```bash
# Check DPS configuration
# 1. Verify ID Scope in simulator.toml
# 2. Ensure enrollment group exists in Azure DPS
# 3. Confirm Root CA certificate uploaded to DPS
# 4. Check firewall allows outbound port 8883
```

#### Error: "Device assignment failed"
**Troubleshooting Steps:**
1. Verify IMEI matches certificate Common Name
2. Check DPS enrollment group includes device
3. Ensure IoT Hub is linked to DPS
4. Review Azure DPS logs in portal

#### Error: Authentication failed (Legacy mode)
```bash
# Verify SAS token generation
.\Release\sim-tests.exe  # Should pass SAS token tests

# Check device key format (must be base64)
echo "your-device-key" | base64 -d  # Should decode without error
```

### Network Issues

#### Error: "Connection timeout"
```bash
# Test basic connectivity
ping global.azure-devices-provisioning.net
ping your-hub.azure-devices.net

# Check firewall rules (allow outbound 8883, 443)
netsh advfirewall firewall add rule name="MQTT TLS" dir=out action=allow protocol=TCP localport=8883
```

#### Error: "SSL handshake failed"
```bash
# Update system time (affects certificate validation)
w32tm /resync

# Test TLS connection manually
openssl s_client -connect global.azure-devices-provisioning.net:8883 -servername global.azure-devices-provisioning.net
```

---

## Success Criteria

### ✅ Build Success Criteria
- [ ] All CMake configuration steps complete without errors
- [ ] All build targets compile successfully
- [ ] Required DLLs are present in Release directory
- [ ] Unit tests executable runs and passes all tests

### ✅ Functional Success Criteria  
- [ ] DPS provisioning completes successfully
- [ ] Device connects to assigned IoT Hub
- [ ] Device Twin synchronization works
- [ ] JSON telemetry messages are properly formatted
- [ ] Event state machine transitions correctly
- [ ] GPS simulation generates realistic coordinates
- [ ] Battery drain model behaves appropriately

### ✅ Performance Success Criteria
- [ ] Memory usage remains stable during long runs
- [ ] No memory leaks detected
- [ ] Message delivery latency < 5 seconds
- [ ] Automatic reconnection works after network disruption
- [ ] Can sustain 1 message/second for 1 hour

### ✅ Integration Success Criteria
- [ ] Messages appear in Azure IoT Hub telemetry
- [ ] Device appears "Connected" in Azure Portal
- [ ] Device Twin reported properties update correctly
- [ ] Azure CLI monitoring shows real-time events
- [ ] End-to-end latency (device → cloud) < 10 seconds

### ✅ Quality Assurance Checklist
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] Code follows MSRA C++ standards
- [ ] All configuration options tested
- [ ] Error handling gracefully manages failures
- [ ] Logging provides sufficient debugging information
- [ ] Documentation matches actual behavior

---

## Continuous Testing

### Automated Test Script
```batch
@echo off
echo Starting MQTT Tracker Simulator Test Suite...

echo [1/5] Running unit tests...
build\Release\sim-tests.exe
if %ERRORLEVEL% neq 0 goto :error

echo [2/5] Testing CLI help...
build\Release\sim-cli.exe --help >nul
if %ERRORLEVEL% neq 0 goto :error

echo [3/5] Testing DPS connection...
timeout 30 build\Release\sim-cli.exe --spike 1
if %ERRORLEVEL% neq 0 goto :error

echo [4/5] Testing driving simulation...
build\Release\sim-cli.exe --drive 1
if %ERRORLEVEL% neq 0 goto :error

echo [5/5] Testing headless mode...
timeout 30 build\Release\sim-cli.exe --headless
if %ERRORLEVEL% neq 0 goto :error

echo ✅ All tests passed successfully!
goto :end

:error
echo ❌ Test failed with error code %ERRORLEVEL%
exit /b 1

:end
```

### Development Workflow
1. **Code Changes** → Run unit tests (`sim-tests.exe`)
2. **Build Changes** → Run connection test (`--spike 1`)
3. **Config Changes** → Run full simulation (`--drive 5`)
4. **Release Testing** → Run extended test suite
5. **Deployment** → Monitor Azure IoT Hub metrics

This comprehensive testing guide ensures reliable deployment and operation of the MQTT Tracker Simulator across different environments and use cases.