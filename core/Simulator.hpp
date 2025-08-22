/**
 * @file Simulator.hpp
 * @brief GPS tracker simulator core engine interface
 * 
 * Defines the main simulation class for a GPS tracking device that connects
 * to Azure IoT Hub via MQTT over TLS. Provides event-driven architecture
 * with state machine management, battery simulation, and geofencing.
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Designed for embedded portability - can be ported to STM32H platforms
 * @note Follows MSRA C++ coding standards for embedded development
 * @note Uses dependency injection for testability and platform independence
 */

#pragma once

#include "Event.hpp"
#include "StateMachine.hpp"
#include "Geo.hpp"
#include "Battery.hpp"
#include "JsonCodec.hpp"
#include "IMqttClient.hpp"
#include "IClock.hpp"
#include "IRng.hpp"
#include "DpsConnectionManager.hpp"
#include <memory>
#include <vector>
#include <chrono>

namespace tracker {

/**
 * @brief Configuration structure for GPS tracker simulator
 * 
 * Contains all parameters needed to configure the simulator including
 * Azure IoT Hub connection details, simulation parameters, and route/geofence data.
 * 
 * @note All string fields must be properly initialized before use
 * @note Location coordinates use WGS84 decimal degrees format
 * @note Route and geofences vectors can be empty for basic simulation
 */
struct SimulatorConfig {
    std::string deviceId = "SIM-001";        ///< Unique device identifier (fallback if not from DPS)
    
    // DPS Configuration (preferred method)
    std::string idScope;                      ///< Azure DPS ID Scope
    std::string imei;                         ///< Device IMEI (used as registration ID)
    std::string deviceCertPath;               ///< Path to device.cert.pem
    std::string deviceKeyPath;                ///< Path to device.key.pem
    std::string deviceChainPath;              ///< Path to device.chain.pem
    std::string rootCaPath;                   ///< Path to root CA certificate
    bool verifyServerCert = true;             ///< Enable server certificate verification
    
    // Legacy Configuration (for backward compatibility)
    std::string iotHubHost;                   ///< Azure IoT Hub hostname (deprecated, use DPS)
    std::string deviceKeyBase64;              ///< Base64-encoded device shared access key (deprecated)
    
    Location startLocation = {-26.2041, 28.0473, 1720.0, 12.5};  ///< Initial GPS coordinates (lat, lon, alt, accuracy)
    double speedLimitKph = 90.0;              ///< Speed limit for violation detection (km/h)
    int heartbeatSeconds = 60;                ///< Interval between periodic heartbeat messages
    
    std::vector<RoutePoint> route;            ///< Optional predefined route waypoints
    std::vector<Geofence> geofences;          ///< Circular geofences for enter/exit detection
    
    // Check if DPS configuration is available
    bool hasDpsConfig() const {
        return !idScope.empty() && !imei.empty() && 
               !deviceCertPath.empty() && !deviceKeyPath.empty() && !rootCaPath.empty();
    }
};

/**
 * @brief Main GPS tracker simulator engine
 * 
 * Implements a comprehensive GPS tracking device simulation with Azure IoT Hub
 * connectivity. Provides event-driven architecture with state management,
 * battery simulation, geofencing, and automated driving scenarios.
 * 
 * @note Thread-safe design suitable for embedded real-time systems
 * @note Uses dependency injection for platform independence
 * @note Designed for low-power embedded operation with efficient algorithms
 */
class Simulator {
public:
    /**
     * @brief Construct simulator with dependency injection
     * @param mqttClient MQTT client implementation for cloud connectivity
     * @param clock System clock abstraction for timing operations
     * @param rng Random number generator for realistic simulation
     */
    Simulator(std::shared_ptr<IMqttClient> mqttClient,
             std::shared_ptr<IClock> clock,
             std::shared_ptr<IRng> rng);
    
    /**
     * @brief Configure simulator with device and connection parameters
     * @param config Complete configuration structure
     * @pre config must contain valid Azure IoT Hub credentials
     */
    void configure(const SimulatorConfig& config);
    
    /**
     * @brief Start the GPS tracker simulation
     * @post Simulation begins and IoT Hub connection is attempted
     */
    void start();
    
    /**
     * @brief Stop the GPS tracker simulation
     * @post Simulation stops and connections are closed
     */
    void stop();
    
    /**
     * @brief Check if simulation is currently running
     * @return True if simulation is active, false otherwise
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Process one simulation frame (call regularly)
     * @note Should be called at consistent intervals (e.g., 1Hz)
     * @post All simulation subsystems are updated
     */
    void tick();
    
    /**
     * @brief Simulate ignition on/off event
     * @param on True for ignition on, false for ignition off
     */
    void setIgnition(bool on);
    
    /**
     * @brief Set vehicle speed and trigger motion events
     * @param speedKph Speed in kilometers per hour (>= 0)
     */
    void setSpeed(double speedKph);
    
    /**
     * @brief Manually set battery percentage for testing
     * @param pct Battery percentage (0.0 to 100.0)
     */
    void setBatteryPercentage(double pct);
    
    /**
     * @brief Start automated driving simulation
     * @param durationMinutes Duration of driving session
     */
    void startDriving(double durationMinutes = 10.0);
    
    /**
     * @brief Generate burst of random events for testing
     * @param eventCount Number of events to generate
     */
    void generateSpike(int eventCount = 10);
    
    /**
     * @brief Integrate Device Twin configuration management (Adapter pattern)
     * @param twinHandler Device Twin protocol adapter instance
     * @pre twinHandler must be valid and configured for the device
     * @post Device Twin functionality available after IoT Hub connection
     * @note Follows Hexagonal Architecture - domain core remains transport-agnostic
     */
    void setTwinHandler(std::shared_ptr<class TwinHandler> twinHandler);
    
private:
    // === Azure IoT Hub Connection Management ===
    
    /** @brief Establish secure MQTT connection to Azure IoT Hub */
    void connectToIoTHub();
    
    /** @brief Process incoming cloud-to-device commands */
    void onMqttMessage(const MqttMessage& message);
    
    /** @brief Handle MQTT connection status changes */
    void onMqttConnection(bool connected, const std::string& reason);
    
    // === Event Processing and Telemetry ===
    
    /** @brief Emit tracking event to Azure IoT Hub with logging */
    void emitEvent(const Event& event);
    
    /** @brief Update GPS location based on movement model */
    void updateLocation();
    
    /** @brief Check for geofence enter/exit events */
    void checkGeofences();
    
    /** @brief Send periodic heartbeat messages */
    void checkHeartbeat();
    
    /** @brief Create base event with current telemetry data */
    Event createBaseEvent(EventType type) const;
    
    // === Dependency Injection Components ===
    std::shared_ptr<IMqttClient> mqttClient_;  ///< MQTT client for Azure IoT Hub communication (legacy)
    std::shared_ptr<IClock> clock_;            ///< System clock abstraction for timestamps
    std::shared_ptr<IRng> rng_;                ///< Random number generator for realistic simulation
    std::unique_ptr<DpsConnectionManager> dpsConnectionManager_;  ///< DPS-based connection manager
    std::shared_ptr<class TwinHandler> twinHandler_;  ///< Device Twin adapter (Hexagonal Architecture)
    
    // === Core Simulation Components ===
    SimulatorConfig config_;                   ///< Device configuration parameters
    StateMachine stateMachine_;                ///< Device state management (Idle/Driving/Parked/LowBattery)
    Battery battery_;                          ///< Battery simulation with realistic drain model
    
    // === Runtime State ===
    bool running_ = false;                     ///< Simulation running flag
    bool connected_ = false;                   ///< MQTT connection status
    
    // === Current Telemetry Data ===
    Location currentLocation_;                 ///< Current GPS coordinates
    double currentSpeed_ = 0.0;                ///< Current vehicle speed (km/h)
    double currentHeading_ = 0.0;              ///< Current direction of travel (degrees, 0-359)
    NetworkInfo networkInfo_;                  ///< Network signal strength and type
    
    // === Message Sequencing and Timing ===
    uint64_t sequenceNumber_ = 0;              ///< Message sequence counter for ordering
    std::chrono::steady_clock::time_point lastHeartbeat_;  ///< Last heartbeat transmission time
    std::chrono::steady_clock::time_point lastTick_;       ///< Last simulation tick time
    
    // === Geofencing State ===
    std::vector<std::string> currentGeofenceIds_;  ///< Currently entered geofences
    
    // === Route Following ===
    double routeProgress_ = 0.0;               ///< Progress along predefined route (0.0-1.0)
    bool followingRoute_ = false;              ///< Route following active flag
    std::chrono::steady_clock::time_point driveStartTime_;  ///< Automated driving start time
    double driveDurationSeconds_ = 0.0;        ///< Automated driving duration
    
    // === MQTT Topics ===
    std::string d2cTopic_;                     ///< Device-to-cloud topic for telemetry
    std::string c2dTopic_;                     ///< Cloud-to-device topic for commands
    
    // === Resilient Connectivity ===
    bool shouldReconnect_ = false;             ///< Reconnection required flag
    std::chrono::steady_clock::time_point lastReconnectAttempt_;  ///< Last reconnection attempt time
    int reconnectAttempts_ = 0;                ///< Current reconnection attempt counter
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;  ///< Maximum reconnection attempts before giving up
    
    /** @brief Attempt automatic reconnection with exponential backoff */
    void attemptReconnection();
    
    /** @brief Validate DPS configuration parameters */
    bool validateDpsConfiguration() const;
    
    /** @brief Validate legacy IoT Hub configuration parameters */
    bool validateLegacyConfiguration() const;
    
    /** @brief Handle completion of DPS connection process */
    void onDpsConnectionComplete(bool connected, const std::string& reason);
    
    /** @brief Initialize Device Twin adapter after IoT Hub connection */
    void initializeDeviceTwinAdapter();
};

} // namespace tracker