# MQTT Tracker Simulator - Project Structure & Architecture

## 📋 Table of Contents
- [Overview](#overview)
- [Architecture Principles](#architecture-principles)
- [Directory Structure](#directory-structure)
- [Core Components](#core-components)
- [Platform Implementations](#platform-implementations)
- [Configuration & Documentation](#configuration--documentation)
- [Build System](#build-system)
- [Data Flow Architecture](#data-flow-architecture)
- [Design Patterns](#design-patterns)
- [Embedded Portability](#embedded-portability)

## Overview

The MQTT Tracker Simulator is a C++20 GPS tracking device simulator designed with embedded portability in mind. It follows **Clean Architecture** principles with clear separation between business logic, platform-specific code, and external interfaces.

**Key Design Goals:**
- 🎯 **Embedded Portability** - Core logic can be compiled for STM32H/FreeRTOS
- 🔌 **Dependency Injection** - Platform abstractions for easy testing and porting
- 🏗️ **Hexagonal Architecture** - Domain logic isolated from external concerns
- 🔄 **Event-Driven Design** - Reactive messaging with minimal resource usage
- 🛡️ **Type Safety** - Modern C++20 features with compile-time guarantees

## Architecture Principles

### 1. Layered Architecture
```
┌─────────────────────────────────────────────────┐
│                UI Layer (Optional)              │  ← Qt GUI, CLI Interface
├─────────────────────────────────────────────────┤
│              Application Layer                  │  ← Configuration, Orchestration
├─────────────────────────────────────────────────┤
│                Domain Layer                     │  ← Business Logic (Platform Independent)
├─────────────────────────────────────────────────┤
│             Infrastructure Layer                │  ← MQTT, Crypto, Platform Services
├─────────────────────────────────────────────────┤
│              Platform Layer                     │  ← OS/Hardware Abstractions
└─────────────────────────────────────────────────┘
```

### 2. Dependency Flow
- **Inward Dependencies Only** - External layers depend on inner layers, never the reverse
- **Interface Segregation** - Small, focused interfaces (`IMqttClient`, `IClock`, `IRng`)
- **Dependency Inversion** - Core logic depends on abstractions, not implementations

### 3. Embedded-First Design
- **No Dynamic Allocation** in core simulation loops
- **Stack-Based Objects** with predictable memory usage
- **Real-Time Friendly** - No blocking operations in time-critical paths
- **Resource Constrained** - Optimized for limited RAM/Flash

---

## Directory Structure

```
📁 mqtt-tracker-simulator/
├── 📁 core/                    # Platform-Independent Domain Logic
│   ├── 📁 adapters/           # Hexagonal Architecture Adapters
│   ├── 📁 domain/             # Pure Business Logic
│   ├── 📁 ports/              # Interface Definitions
│   └── 📁 sim/                # Testing & Simulation Utilities
├── 📁 crypto/                 # Cryptographic Services
├── 📁 net/                    # Network Protocol Implementations
│   └── 📁 mqtt/               # MQTT Client Implementation
├── 📁 platform/               # Platform-Specific Code
│   └── 📁 desktop/            # Desktop/PC Implementation
├── 📁 ui/                     # User Interface Components
│   └── 📁 qt/                 # Qt GUI Application
├── 📁 tests/                  # Unit Tests & Validation
├── 📁 DeviceCertGenerator/    # X.509 Certificate Tooling
└── 📁 build/                  # Build Artifacts (Generated)
```

---

## Core Components

The `core/` directory contains the heart of the simulation engine, designed for maximum portability and testability.

### 📁 core/ - Platform-Independent Domain Logic

#### Primary Simulation Engine
| File | Purpose | Dependencies |
|------|---------|--------------|
| **`Simulator.hpp/.cpp`** | Main orchestration engine, manages all subsystems | All core interfaces |
| **`StateMachine.hpp/.cpp`** | Vehicle state logic (Idle/Driving/Parked/LowBattery) | Event system |
| **`Event.hpp/.cpp`** | Event data structures and type definitions | JSON codec |
| **`JsonCodec.hpp/.cpp`** | JSON serialization for telemetry messages | nlohmann/json |

#### Sensor & Environment Simulation
| File | Purpose | Dependencies |
|------|---------|--------------|
| **`Geo.hpp/.cpp`** | GPS coordinate math, geofencing, route calculation | Standard math |
| **`Battery.hpp/.cpp`** | Battery drain model with realistic voltage curves | Time interfaces |

#### Azure IoT Integration
| File | Purpose | Dependencies |
|------|---------|--------------|
| **`DpsProvisioning.hpp/.cpp`** | Azure Device Provisioning Service workflow | MQTT client |
| **`DpsConnectionManager.hpp/.cpp`** | High-level DPS connection management | DPS + MQTT |
| **`TwinHandler.hpp/.cpp`** | Device Twin configuration management | MQTT client |

#### Platform Abstraction Interfaces
| File | Purpose | Implementation |
|------|---------|----------------|
| **`IMqttClient.hpp`** | MQTT client interface | `PahoMqttClient` (desktop), `LwIPMqttClient` (embedded) |
| **`IClock.hpp/.cpp`** | Time/clock abstraction | `SystemClock` (desktop), `HALClock` (STM32) |
| **`IRng.hpp`** | Random number generation | `StandardRng` (desktop), `TrngRng` (hardware) |

### 📁 core/adapters/ - Hexagonal Architecture Adapters

| File | Purpose | Pattern |
|------|---------|---------|
| **`DefaultPolicies.hpp`** | Default configuration policies and constants | Strategy Pattern |
| **`MqttTransportAdapter.hpp/.cpp`** | MQTT transport abstraction layer | Adapter Pattern |

**Purpose:** These adapters translate between the domain model and external services, enabling the core logic to remain independent of specific implementations.

### 📁 core/domain/ - Pure Business Logic

| File | Purpose | Dependencies |
|------|---------|--------------|
| **`DeviceStateMachine.hpp/.cpp`** | Device-specific state transitions and behaviors | Core events |
| **`EventBus.hpp/.cpp`** | Event publishing and subscription mechanism | Event definitions |
| **`TelemetryPipeline.hpp/.cpp`** | Telemetry data processing and transformation | JSON codec |
| **`TrackerSimulator.hpp`** | High-level tracker simulation interface | All domain components |

**Key Characteristics:**
- **Zero External Dependencies** - Only depends on C++ standard library
- **Pure Functions** - Deterministic, testable business logic
- **Immutable Data** - Event objects are read-only after creation

### 📁 core/ports/ - Interface Definitions

| File | Purpose | Usage |
|------|---------|-------|
| **`IEventBus.hpp`** | Event system interface | Publish/Subscribe pattern |
| **`IPolicyEngine.hpp`** | Configuration policy interface | Strategy pattern for device behavior |
| **`ITransport.hpp`** | Generic transport layer interface | Network abstraction |

**Purpose:** Define contracts for external services that the domain layer needs, enabling dependency inversion and easy testing.

### 📁 core/sim/ - Testing & Simulation Utilities

| File | Purpose | Usage |
|------|---------|-------|
| **`MockTransport.hpp/.cpp`** | Mock transport for unit testing | Test doubles |
| **`SimulatedClock.hpp/.cpp`** | Controllable time source for testing | Deterministic testing |

**Purpose:** Provide test doubles and simulation utilities for deterministic testing of time-dependent and network-dependent code.

---

## Platform Implementations

### 📁 crypto/ - Cryptographic Services

| File | Purpose | Implementation |
|------|---------|----------------|
| **`SasToken.hpp/.cpp`** | Azure IoT Hub SAS token generation | OpenSSL HMAC-SHA256 + Base64 |

**Security Features:**
- **HMAC-SHA256** signature generation
- **Base64 encoding/decoding**
- **URL-safe encoding** for Azure IoT Hub
- **Token expiration** management

**Embedded Portability:** Can be replaced with mbedTLS for resource-constrained devices.

### 📁 net/mqtt/ - Network Protocol Implementations

| File | Purpose | Implementation |
|------|---------|----------------|
| **`PahoMqttClient.hpp/.cpp`** | Desktop MQTT client with TLS support | Eclipse Paho MQTT C library |

**Features:**
- **MQTT 3.1.1** protocol support
- **TLS/SSL encryption** with certificate authentication
- **Automatic reconnection** with exponential backoff
- **QoS support** (0, 1, 2)
- **Message queuing** for offline scenarios

**Embedded Alternative:** For STM32H deployment, this would be replaced with a lightweight MQTT implementation using LwIP + mbedTLS.

### 📁 platform/desktop/ - Desktop Platform Implementation

| File | Purpose | Dependencies |
|------|---------|--------------|
| **`main_cli.cpp`** | Command-line application entry point | Core simulator |
| **`TomlConfig.hpp`** | TOML configuration file parser | Filesystem |

**CLI Features:**
- **Interactive mode** - Real-time command input
- **Automated modes** - Drive simulation, event spikes
- **Headless operation** - For background/service deployment
- **Signal handling** - Graceful shutdown on SIGINT/SIGTERM

### 📁 ui/qt/ - GUI Application (Optional)

| File | Purpose | Dependencies |
|------|---------|--------------|
| **`main.cpp`** | Qt application entry point | Qt6 Core |
| **`MainWindow.hpp/.cpp`** | Main GUI window and controls | Qt6 Widgets, WebEngine |

**GUI Features:**
- **Real-time status** - Connection state, message counts
- **Interactive controls** - Speed, ignition, battery simulation
- **Event log** - Scrollable history of generated events
- **Map view** - GPS tracking visualization using Leaflet/OpenStreetMap
- **Configuration** - GUI-based settings management

---

## Configuration & Documentation

### Configuration Files

| File | Purpose | Format |
|------|---------|--------|
| **`simulator.toml`** | Runtime configuration (not in repo - contains secrets) | TOML |
| **`simulator.toml.example`** | Configuration template | TOML |
| **`config_applied.json`** | Last applied Device Twin configuration | JSON |

### Documentation Files

| File | Purpose | Audience |
|------|---------|----------|
| **`README.md`** | Complete setup and usage guide | End users, developers |
| **`PROJECT_STRUCTURE.md`** | Architecture and codebase documentation | Developers |
| **`CLAUDE.md`** | Development instructions and requirements | Claude Code AI |
| **`DPS_TESTING_GUIDE.md`** | Azure DPS testing procedures | QA, DevOps |
| **`TESTING_GUIDE.md`** | General testing and validation | QA, developers |

### Certificate Management

| File/Directory | Purpose | Security Level |
|----------------|---------|----------------|
| **`DeviceCertGenerator/`** | X.509 certificate generation tooling | Development only |
| **`DeviceCertGenerator/cert/RootCA-*.pem`** | Root CA certificates | Public (checked in) |
| **`DeviceCertGenerator/device/{IMEI}/`** | Per-device certificates | Private (gitignored) |

---

## Build System

### Primary Build Configuration

| File | Purpose | Build System |
|------|---------|--------------|
| **`CMakeLists.txt`** | Main build configuration | CMake 3.20+ |
| **`.gitignore`** | Git exclusion rules | Git |

### Build Targets

| Target | Type | Purpose |
|--------|------|---------|
| **`tracker_core`** | Static Library | Platform-independent simulation engine |
| **`tracker_crypto`** | Static Library | Cryptographic services |
| **`tracker_mqtt`** | Static Library | MQTT networking |
| **`sim-cli`** | Executable | Command-line application |
| **`sim-qt`** | Executable | GUI application (optional) |
| **`sim-tests`** | Executable | Unit test suite |

### Build Options

| Option | Default | Purpose |
|--------|---------|---------|
| **`BUILD_QT`** | OFF | Enable Qt GUI application |
| **`BUILD_TESTS`** | ON | Enable unit test compilation |
| **`EMBEDDED_BUILD`** | OFF | Optimize for embedded targets |

---

## Data Flow Architecture

### 1. Initialization Flow
```
Configuration Load → Certificate Validation → DPS Connection → IoT Hub Assignment → Twin Sync → Ready
```

### 2. Event Generation Flow
```
Trigger Detection → State Machine → Event Creation → JSON Serialization → MQTT Publish → Confirmation
```

### 3. Device Twin Configuration Flow
```
Azure Portal → IoT Hub → Device Twin → MQTT Subscribe → Config Parser → State Update → Acknowledgment
```

### 4. Telemetry Message Flow
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Sensor    │───▶│    Event    │───▶│    JSON     │───▶│    MQTT     │
│ Simulation  │    │  Generator  │    │  Encoder    │    │  Publisher  │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │                   │
       ▼                   ▼                   ▼                   ▼
  GPS Movement      State Changes      Message Format      Network Delivery
  Battery Drain     Event Types       IoT Hub Schema        QoS Handling
  Geofence Check    Sequence Numbers  Timestamp Format       Retry Logic
```

---

## Design Patterns

### 1. Dependency Injection
```cpp
// Platform-agnostic core construction
class Simulator {
public:
    Simulator(std::shared_ptr<IMqttClient> mqtt,
              std::shared_ptr<IClock> clock,
              std::shared_ptr<IRng> rng);
};

// Desktop implementation
auto mqtt = std::make_shared<PahoMqttClient>();
auto clock = std::make_shared<SystemClock>();
auto rng = std::make_shared<StandardRng>();
Simulator sim(mqtt, clock, rng);

// Embedded implementation (future)
auto mqtt = std::make_shared<LwIPMqttClient>();
auto clock = std::make_shared<STM32Clock>();
auto rng = std::make_shared<HardwareRng>();
Simulator sim(mqtt, clock, rng);
```

### 2. Observer Pattern (Event Bus)
```cpp
// Event subscription
eventBus->subscribe<MotionStartEvent>([](const MotionStartEvent& event) {
    // Handle motion start
});

// Event publishing
eventBus->publish(MotionStartEvent{location, speed, timestamp});
```

### 3. State Machine Pattern
```cpp
enum class DeviceState {
    Idle, Driving, Parked, LowBattery
};

class StateMachine {
    DeviceState currentState_;
public:
    void handleEvent(const Event& event);
    DeviceState getCurrentState() const;
};
```

### 4. Strategy Pattern (Policies)
```cpp
struct DefaultPolicies {
    static constexpr double SPEED_THRESHOLD_KPH = 5.0;
    static constexpr int LOW_BATTERY_THRESHOLD = 20;
    static constexpr std::chrono::seconds HEARTBEAT_INTERVAL{60};
};
```

### 5. Adapter Pattern (Transport)
```cpp
class MqttTransportAdapter : public ITransport {
    std::shared_ptr<IMqttClient> mqttClient_;
public:
    void send(const Message& message) override;
    void connect(const ConnectionParams& params) override;
};
```

---

## Embedded Portability

### Current Desktop Implementation
| Component | Desktop Implementation | Dependencies |
|-----------|----------------------|--------------|
| **MQTT Client** | Paho MQTT C library | TCP sockets, OpenSSL |
| **TLS/Crypto** | OpenSSL | System crypto libraries |
| **Time Source** | `std::chrono` | System clock |
| **Random Generator** | `std::random_device` | OS entropy |
| **File System** | `std::filesystem` | POSIX file API |

### Target Embedded Implementation (STM32H)
| Component | Embedded Implementation | Dependencies |
|-----------|------------------------|--------------|
| **MQTT Client** | LwIP MQTT + custom | LwIP TCP/IP stack |
| **TLS/Crypto** | mbedTLS | Hardware crypto accelerator |
| **Time Source** | HAL_GetTick() | STM32 SysTick timer |
| **Random Generator** | Hardware TRNG | STM32 RNG peripheral |
| **File System** | LittleFS or FatFS | Flash memory |

### Porting Checklist
- [ ] Replace `PahoMqttClient` with embedded MQTT implementation
- [ ] Port `SasToken` crypto from OpenSSL to mbedTLS
- [ ] Implement embedded-specific `IClock` using HAL timers
- [ ] Replace `std::filesystem` with embedded file system
- [ ] Adapt memory allocation patterns for embedded heap
- [ ] Configure FreeRTOS task priorities and stack sizes
- [ ] Optimize JSON encoding for minimal memory usage
- [ ] Implement flash-based certificate storage

### Memory Usage Estimates
| Component | Desktop (Debug) | Desktop (Release) | STM32H Target |
|-----------|----------------|-------------------|---------------|
| **Code Size** | ~2MB | ~500KB | ~256KB |
| **Static RAM** | ~100KB | ~50KB | ~32KB |
| **Dynamic RAM** | ~200KB | ~100KB | ~64KB |
| **Total Flash** | N/A | N/A | ~512KB |

---

## Testing Strategy

### 📁 tests/ - Test Organization

| File | Purpose | Test Type |
|------|---------|-----------|
| **`test_sas_token.cpp`** | Cryptographic function validation | Unit tests |
| **`test_clean_architecture.cpp`** | Architecture compliance validation | Integration tests |

### Test Categories

1. **Unit Tests**
   - Individual component functionality
   - Mock dependencies for isolation
   - Deterministic, fast execution

2. **Integration Tests**
   - Component interaction validation
   - Real Azure DPS/IoT Hub connectivity
   - End-to-end message flow

3. **Performance Tests**
   - Memory usage validation
   - Message throughput measurement
   - Connection stability testing

4. **Embedded Simulation**
   - Resource constraint validation
   - Real-time behavior verification
   - Hardware abstraction testing

---

## Future Architecture Considerations

### Multi-Device Support
- **Device Management** - Central orchestration of multiple simulator instances
- **Load Balancing** - Distribute devices across IoT Hub units
- **Configuration Management** - Centralized device configuration and updates

### Advanced Features
- **OTA Updates** - Firmware update simulation
- **Edge Computing** - Local processing before cloud transmission
- **Machine Learning** - Predictive behavior modeling
- **Digital Twin** - 3D vehicle simulation integration

### Cloud Integration Extensions
- **Azure Stream Analytics** - Real-time telemetry processing
- **Azure Functions** - Serverless event processing
- **Power BI** - Dashboard and reporting integration
- **Azure Maps** - Advanced geofencing and routing

---

## Conclusion

This project structure provides a solid foundation for GPS tracker simulation with clear separation of concerns, embedded portability, and production readiness. The architecture supports both current desktop development needs and future embedded deployment requirements.

**Key Strengths:**
- ✅ **Modular Design** - Clear component boundaries
- ✅ **Platform Independence** - Core logic isolated from platform code
- ✅ **Testability** - Comprehensive dependency injection and mocking
- ✅ **Scalability** - Architecture supports multiple devices and advanced features
- ✅ **Maintainability** - Well-documented, consistent coding patterns

The codebase is ready for production deployment and provides a clear path for embedded porting when needed.