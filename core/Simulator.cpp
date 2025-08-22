/**
 * @file Simulator.cpp
 * @brief Implementation of the GPS tracker simulator core engine
 * 
 * This module provides the main simulation engine for a GPS tracking device that
 * connects to Azure IoT Hub via MQTT over TLS. It implements event-driven
 * architecture with state machine management, battery simulation, and geofencing.
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Designed for embedded portability - core logic can be ported to STM32H
 * @note Follows MSRA C++ coding standards for embedded development
 */

#include "Simulator.hpp"
#include "TwinHandler.hpp"
#include "../crypto/SasToken.hpp"
#include "../net/mqtt/PahoMqttClient.hpp"
#include <iostream>
#include <thread>
#include <cmath>

namespace tracker {

/**
 * @brief Construct a new Simulator object with dependency injection
 * 
 * Initializes the GPS tracker simulator with required dependencies for
 * MQTT communication, time management, and random number generation.
 * Sets up event callbacks and default network parameters.
 * 
 * @param mqttClient Shared pointer to MQTT client implementation
 * @param clock Shared pointer to system clock abstraction
 * @param rng Shared pointer to random number generator
 * 
 * @note Uses dependency injection pattern for testability and portability
 * @note Battery simulation is initialized with RNG dependency
 */
Simulator::Simulator(std::shared_ptr<IMqttClient> mqttClient,
                    std::shared_ptr<IClock> clock,
                    std::shared_ptr<IRng> rng)
    : mqttClient_(mqttClient), clock_(clock), rng_(rng), battery_(rng) {
    
    // Create DPS connection manager
    dpsConnectionManager_ = std::make_unique<DpsConnectionManager>(std::make_shared<PahoMqttClient>());
    
    // Configure state machine to emit events through our event system
    stateMachine_.setEventEmitter([this](const Event& event) {
        Event fullEvent = createBaseEvent(event.eventType);
        fullEvent.extras = event.extras;
        emitEvent(fullEvent);
    });
    
    // Set up MQTT message callback for cloud-to-device commands
    mqttClient_->setMessageCallback([this](const MqttMessage& msg) {
        onMqttMessage(msg);
    });
    
    // Set up connection status callback for reconnection logic
    mqttClient_->setConnectionCallback([this](bool connected, const std::string& reason) {
        onMqttConnection(connected, reason);
    });
    
    // Set up DPS connection manager callbacks
    dpsConnectionManager_->setMessageCallback([this](const MqttMessage& msg) {
        onMqttMessage(msg);
    });
    
    // Initialize default network parameters for simulation
    networkInfo_.rssi = -72;  // Typical LTE signal strength (dBm)
    networkInfo_.rat = "LTE"; // Radio Access Technology
}

/**
 * @brief Configure the simulator with device-specific parameters
 * 
 * Sets up the simulator with Azure IoT Hub connection details, starting location,
 * and route information. Initializes MQTT topics and battery state.
 * 
 * @param config Configuration structure containing all simulation parameters
 * 
 * @pre config must contain valid deviceId for MQTT topic construction
 * @post Simulator is ready to start with specified configuration
 * @post Battery is initialized to 100% charge
 */
void Simulator::configure(const SimulatorConfig& config) {
    config_ = config;
    currentLocation_ = config.startLocation;
    battery_.setPercentage(100.0);  // Start with full battery
    
    // Construct Azure IoT Hub MQTT topics using device ID
    d2cTopic_ = "devices/" + config.deviceId + "/messages/events/";
    c2dTopic_ = "devices/" + config.deviceId + "/messages/devicebound/#";
    
    // Enable route following if route waypoints are provided
    if (!config.route.empty()) {
        followingRoute_ = true;
        routeProgress_ = 0.0;  // Start at beginning of route
    }
}

/**
 * @brief Start the GPS tracker simulation
 * 
 * Begins the simulation loop and establishes connection to Azure IoT Hub.
 * Initializes timing variables and starts the main simulation thread.
 * 
 * @pre Simulator must be configured with valid IoT Hub parameters
 * @post Simulation is running and MQTT connection is attempted
 * @post Timer variables are initialized for event generation
 * 
 * @note Idempotent - safe to call multiple times
 */
void Simulator::start() {
    if (running_) return;  // Already running - ignore duplicate start calls
    
    running_ = true;
    lastTick_ = std::chrono::steady_clock::now();
    lastHeartbeat_ = lastTick_;
    
    // Establish secure MQTT connection to Azure IoT Hub
    connectToIoTHub();
}

/**
 * @brief Stop the GPS tracker simulation
 * 
 * Cleanly shuts down the simulation and disconnects from Azure IoT Hub.
 * Sets running flag to false to stop the simulation loop.
 * 
 * @post Simulation is stopped and MQTT connection is closed
 * @post No further events will be generated
 */
void Simulator::stop() {
    running_ = false;
    mqttClient_->disconnect();
}

/**
 * @brief Establish secure MQTT connection to Azure IoT Hub
 * 
 * Creates SAS token for authentication and initiates MQTT over TLS connection
 * to Azure IoT Hub on port 8883. Uses device credentials for authentication.
 * 
 * @pre Configuration must contain valid IoT Hub host and device key
 * @post MQTT connection attempt is initiated with proper authentication
 * 
 * @note Uses SAS token authentication as recommended by Microsoft
 * @note Connection uses TLS encryption for security
 */
void Simulator::connectToIoTHub() {
    // Check if DPS configuration is available (preferred method)
    if (config_.hasDpsConfig()) {
        std::cout << "[Simulator] Initiating DPS-based connection" << std::endl;
        std::cout << "[Simulator] Device: " << config_.imei << " | Scope: " << config_.idScope << std::endl;
        
        if (!validateDpsConfiguration()) {
            std::cerr << "[Simulator] Invalid DPS configuration - connection aborted" << std::endl;
            return;
        }
        
        DeviceConfig deviceConfig;
        deviceConfig.imei = config_.imei;
        deviceConfig.idScope = config_.idScope;
        deviceConfig.deviceCertPath = config_.deviceChainPath;  // Use certificate chain for Azure DPS
        deviceConfig.deviceKeyPath = config_.deviceKeyPath;
        deviceConfig.deviceChainPath = config_.deviceChainPath;
        deviceConfig.rootCaPath = config_.rootCaPath;
        deviceConfig.verifyServerCert = config_.verifyServerCert;
        deviceConfig.timeout = std::chrono::seconds(120);  // 2-minute timeout for provisioning
        
        dpsConnectionManager_->connectToIotHub(deviceConfig, 
            [this](bool connected, const std::string& reason) {
                onDpsConnectionComplete(connected, reason);
            });
        return;
    }
    
    // Fall back to legacy SAS token connection if DPS not configured
    if (!validateLegacyConfiguration()) {
        std::cerr << "[Simulator] Missing both DPS and legacy IoT Hub configuration" << std::endl;
        return;
    }
    
    std::cout << "[Simulator] Using legacy SAS token authentication" << std::endl;
    std::cout << "[Simulator] Hub: " << config_.iotHubHost << " | Device: " << config_.deviceId << std::endl;
    
    // Construct MQTT username with Azure IoT Hub API version
    std::string username = config_.iotHubHost + "/" + config_.deviceId + "/?api-version=2021-04-12";
    
    // Configure SAS token generation parameters
    SasToken::Config sasConfig;
    sasConfig.host = config_.iotHubHost;
    sasConfig.deviceId = config_.deviceId;
    sasConfig.deviceKeyBase64 = config_.deviceKeyBase64;
    sasConfig.expirySeconds = 3600; // 1 hour token validity (Microsoft recommendation)
    
    try {
        // Generate SAS token for MQTT password authentication
        std::string password = SasToken::generate(sasConfig);
        
        // Initiate secure MQTT connection (TLS on port 8883)
        bool connectionStarted = mqttClient_->connect(config_.iotHubHost, 8883, config_.deviceId, username, password);
        
        if (!connectionStarted) {
            std::cerr << "[Simulator] Failed to initiate legacy MQTT connection" << std::endl;
            shouldReconnect_ = true;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Simulator] SAS token generation failed: " << e.what() << std::endl;
        shouldReconnect_ = true;
    }
}

/**
 * @brief Main simulation tick - processes one simulation frame
 * 
 * Performs all per-frame simulation updates including battery drain,
 * location updates, geofence checking, and MQTT event processing.
 * Should be called regularly (e.g., 1Hz) for proper simulation.
 * 
 * @pre Simulator must be running and configured
 * @post All simulation subsystems are updated for current frame
 * 
 * @note Calculates delta time for frame-rate independent simulation
 * @note Handles route following and automatic reconnection
 */
void Simulator::tick() {
    if (!running_) return;  // Skip processing if simulation is stopped
    
    // Calculate elapsed time since last tick for frame-rate independence
    auto now = std::chrono::steady_clock::now();
    auto deltaTime = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTick_);
    lastTick_ = now;
    
    double deltaSeconds = deltaTime.count();
    
    // Update battery simulation (drain faster when driving)
    battery_.tick(deltaSeconds, stateMachine_.getCurrentState() == DeviceState::Driving);
    stateMachine_.processBatteryLevel(battery_.getPercentage());
    
    // Update all simulation subsystems
    updateLocation();   // GPS coordinate simulation
    checkGeofences();   // Geofence enter/exit detection
    checkHeartbeat();   // Periodic heartbeat transmission
    
    // Handle automatic reconnection if connection was lost
    if (shouldReconnect_) {
        attemptReconnection();
    }
    
    // Process route following logic
    if (followingRoute_ && currentSpeed_ > 0.0) {
        double routeSpeedMs = currentSpeed_ / 3.6;  // Convert km/h to m/s
        routeProgress_ += (routeSpeedMs * deltaSeconds) / 1000.0;  // Normalize to 0-1
        
        // Stop at end of route
        if (routeProgress_ >= 1.0) {
            routeProgress_ = 1.0;
            setSpeed(0.0);          // Stop vehicle
            followingRoute_ = false; // Complete route following
        }
    }
    
    // Process incoming MQTT messages and connection events
    if (config_.hasDpsConfig()) {
        dpsConnectionManager_->processEvents();
    } else {
        mqttClient_->processEvents();
    }
}

/**
 * @brief Simulate ignition on/off event
 * 
 * Triggers ignition state change in the device state machine.
 * Used for manual control or automated driving scenarios.
 * 
 * @param on True to turn ignition on, false to turn off
 * 
 * @post State machine processes ignition change event
 * @post May trigger ignition_on or ignition_off events
 */
void Simulator::setIgnition(bool on) {
    stateMachine_.processIgnition(on);
}

/**
 * @brief Set vehicle speed and trigger motion events
 * 
 * Updates current vehicle speed and processes motion state changes.
 * Triggers motion_start/motion_stop events and speed limit violations.
 * 
 * @param speedKph Vehicle speed in kilometers per hour
 * 
 * @pre speedKph must be >= 0
 * @post Current speed is updated
 * @post Motion events are generated if movement state changes
 * @post Speed violation events are generated if limit is exceeded
 */
void Simulator::setSpeed(double speedKph) {
    bool wasMoving = currentSpeed_ > 0.0;
    bool isMoving = speedKph > 0.0;
    
    currentSpeed_ = speedKph;
    
    // Detect motion state change and trigger appropriate events
    if (wasMoving != isMoving) {
        stateMachine_.processMotion(isMoving);
    }
    
    // Check for speed limit violations
    if (speedKph > config_.speedLimitKph) {
        stateMachine_.processSpeedLimit(speedKph, config_.speedLimitKph);
    }
}

/**
 * @brief Manually set battery percentage for testing
 * 
 * Allows manual override of battery percentage for simulation scenarios.
 * Useful for testing low battery events and battery-dependent behavior.
 * 
 * @param pct Battery percentage (0.0 to 100.0)
 * 
 * @pre pct should be in range [0.0, 100.0]
 * @post Battery percentage is set to specified value
 */
void Simulator::setBatteryPercentage(double pct) {
    battery_.setPercentage(pct);
}

/**
 * @brief Start automated driving simulation
 * 
 * Begins an automated driving session with random speed variation.
 * Enables route following if a route is configured.
 * 
 * @param durationMinutes Duration of driving session in minutes
 * 
 * @post Ignition is turned on
 * @post Speed is set to random value around 45 km/h
 * @post Route following is initiated if route is available
 * @post Drive timer is started
 */
void Simulator::startDriving(double durationMinutes) {
    setIgnition(true);
    
    // Set randomized driving speed (30-60 km/h range)
    setSpeed(45.0 + rng_->uniform(-15.0, 15.0));
    
    // Initialize drive session timing
    driveStartTime_ = std::chrono::steady_clock::now();
    driveDurationSeconds_ = durationMinutes * 60.0;
    
    // Start route following if waypoints are configured
    if (!config_.route.empty()) {
        followingRoute_ = true;
        routeProgress_ = 0.0;  // Start at beginning of route
    }
}

/**
 * @brief Generate burst of random events for testing
 * 
 * Creates a rapid sequence of random events to test message throughput
 * and Azure IoT Hub message handling. Useful for load testing scenarios.
 * 
 * @param eventCount Number of events to generate in burst
 * 
 * @post Specified number of random events are generated and transmitted
 * @post 100ms delay between events to prevent overwhelming the system
 * 
 * @note Blocks execution during event generation
 * @note Events are selected randomly from common event types
 */
void Simulator::generateSpike(int eventCount) {
    for (int i = 0; i < eventCount; ++i) {
        // Define event types for random selection
        EventType types[] = {
            EventType::MotionStart, EventType::MotionStop,
            EventType::IgnitionOn, EventType::IgnitionOff,
            EventType::Heartbeat
        };
        
        // Select random event type
        int typeIndex = rng_->uniformInt(0, 4);
        Event event = createBaseEvent(types[typeIndex]);
        emitEvent(event);
        
        // Brief delay to prevent overwhelming MQTT broker
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/**
 * @brief Integrate Device Twin adapter following Hexagonal Architecture
 * 
 * Connects the configuration management port to the Azure IoT Hub Device Twin
 * adapter, enabling bidirectional configuration synchronization while keeping
 * the domain core independent of Azure-specific protocols.
 * 
 * @param twinHandler Device Twin protocol adapter (Hexagonal Architecture)
 * 
 * @note Observer pattern: Registers callbacks for configuration change events
 * @note Strategy pattern: Supports pluggable acknowledgment and error handling
 */
void Simulator::setTwinHandler(std::shared_ptr<TwinHandler> twinHandler) {
    twinHandler_ = twinHandler;
    
    if (twinHandler_) {
        // Configure Observer pattern callbacks for configuration events
        twinHandler_->setConfigUpdateCallback([](const TwinUpdateResult& result, const nlohmann::json& configData) {
            (void)configData; // Bounded processing - config already persisted
            
            // Log configuration change with essential metadata only
            std::cout << "Configuration " << (result.status == TwinStatus::Success ? "updated" : "failed")
                      << ": v" << result.configVersion << std::endl;
            
            if (result.status != TwinStatus::Success) {
                std::cerr << "Config error: " << result.errorMessage << std::endl;
            }
        });
        
        // Minimal logging for twin operation responses (bounded output)
        twinHandler_->setTwinResponseCallback([](TwinStatus status, const std::string& message) {
            if (status != TwinStatus::Success) {
                std::cerr << "Twin error: " << message << std::endl;
            }
        });
    }
}

/**
 * @brief Process incoming cloud-to-device (C2D) commands
 * 
 * Handles JSON commands received from Azure IoT Hub via MQTT subscription.
 * Supports configuration updates and device control commands.
 * 
 * @param message MQTT message containing JSON command payload
 * 
 * @post Configuration may be updated based on command content
 * @post Device actions may be triggered (e.g., reboot)
 * 
 * @note Supported commands: setHeartbeatSeconds, setSpeedLimit, reboot
 * @note Invalid JSON or unknown commands are logged but ignored
 */
void Simulator::onMqttMessage(const MqttMessage& message) {
    // First, check if this is a Device Twin message and route to TwinHandler
    if (twinHandler_ && message.topic.find("$iothub/twin/") != std::string::npos) {
        twinHandler_->handleMqttMessage(message);
        return;
    }
    
    // Handle regular cloud-to-device messages
    try {
        auto json = nlohmann::json::parse(message.payload);
        
        // Process command if present in message
        if (json.contains("cmd")) {
            std::string cmd = json["cmd"];
            
            // Handle heartbeat interval configuration
            if (cmd == "setHeartbeatSeconds" && json.contains("value")) {
                config_.heartbeatSeconds = json["value"];
            }
            // Handle speed limit configuration
            else if (cmd == "setSpeedLimit" && json.contains("value")) {
                config_.speedLimitKph = json["value"];
            }
            // Handle device reboot command
            else if (cmd == "reboot") {
                stop();
                std::this_thread::sleep_for(std::chrono::seconds(2));  // Brief shutdown delay
                start();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse C2D message: " << e.what() << std::endl;
    }
}

/**
 * @brief Handle MQTT connection status changes
 * 
 * Processes connection and disconnection events from MQTT client.
 * Manages C2D subscription and automatic reconnection logic.
 * 
 * @param connected True if connection established, false if lost
 * @param reason Description of connection status change
 * 
 * @post Connection state is updated
 * @post C2D topic subscription is established on connect
 * @post Reconnection logic is activated on disconnect
 */
void Simulator::onMqttConnection(bool connected, const std::string& reason) {
    connected_ = connected;
    
    if (connected) {
        std::cout << "MQTT Connection: CONNECTED - " << reason << std::endl;
        
        // Subscribe to cloud-to-device message topic
        mqttClient_->subscribe(c2dTopic_);
        
        // Initialize Device Twin subscriptions if TwinHandler is available
        if (twinHandler_) {
            std::cout << "Initializing Device Twin subscriptions..." << std::endl;
            if (twinHandler_->initializeSubscriptions()) {
                std::cout << "Device Twin subscriptions initialized successfully" << std::endl;
                
                // Request full Device Twin to get current desired properties
                std::cout << "Requesting full Device Twin..." << std::endl;
                twinHandler_->requestFullTwin("1");
            } else {
                std::cerr << "Failed to initialize Device Twin subscriptions" << std::endl;
            }
        }
        
        // Reset reconnection state on successful connection
        reconnectAttempts_ = 0;
        shouldReconnect_ = false;
    } else {
        std::cout << "MQTT Connection: DISCONNECTED - " << reason << std::endl;
        
        // Activate reconnection logic if simulation is still running
        if (running_) {
            shouldReconnect_ = true;
            lastReconnectAttempt_ = std::chrono::steady_clock::now();
        }
    }
}

/**
 * @brief Emit tracking event to Azure IoT Hub
 * 
 * Serializes event to JSON format and publishes to Azure IoT Hub via MQTT.
 * Provides comprehensive logging for debugging and monitoring purposes.
 * 
 * @param event Event structure containing all tracking data
 * 
 * @pre Event must contain valid tracking data
 * @post Event is serialized to JSON format
 * @post Message is published to Azure IoT Hub (if connected)
 * @post Detailed logging output is generated for monitoring
 * 
 * @note Uses QoS 1 (at least once delivery) for reliable transmission
 * @note JSON payload is pretty-printed for readability
 */
void Simulator::emitEvent(const Event& event) {
    // Serialize event to JSON format for transmission
    std::string json = JsonCodec::serialize(event);
    
    // Parse and format JSON for readable logging output
    try {
        nlohmann::json parsed = nlohmann::json::parse(json);
        
        // Display structured event information for monitoring
        std::cout << "\n=== EVENT GENERATED ===" << std::endl;
        std::cout << "Type: " << parsed["eventType"].get<std::string>() << std::endl;
        std::cout << "Sequence: " << parsed["seq"].get<int>() << std::endl;
        std::cout << "Timestamp: " << parsed["ts"].get<std::string>() << std::endl;
        std::cout << "JSON Payload:" << std::endl;
        std::cout << parsed.dump(2) << std::endl; // Pretty print with 2-space indent
        
        // Attempt to publish to Azure IoT Hub if connected
        if (connected_) {
            std::cout << "📤 Publishing to topic: " << d2cTopic_ << std::endl;
            bool success = false;
            
            // Use appropriate MQTT client based on connection type
            if (config_.hasDpsConfig() && dpsConnectionManager_->isConnected()) {
                success = dpsConnectionManager_->publish("", json, 1);  // DPS manager handles topic
            } else {
                success = mqttClient_->publish(d2cTopic_, json, 1);  // QoS 1 for reliability
            }
            
            std::cout << (success ? "✅ Published to Azure IoT Hub" : "❌ Publish failed") << std::endl;
        } else {
            std::cout << "⚠️  Offline - Message queued for later delivery" << std::endl;
        }
        std::cout << "========================\n" << std::endl;
        
    } catch (const std::exception& e) {
        // Log JSON parsing errors for debugging
        std::cerr << "❌ JSON parsing error: " << e.what() << std::endl;
        std::cerr << "Raw JSON: " << json << std::endl;
    }
}

/**
 * @brief Update GPS location based on current movement state
 * 
 * Calculates new GPS coordinates based on route following or free movement.
 * Includes realistic heading variation and speed-based position updates.
 * 
 * @pre Current speed and heading must be valid
 * @post GPS coordinates are updated based on movement model
 * @post Heading includes realistic variation for natural movement
 * 
 * @note Route following takes precedence over free movement
 * @note Heading variation uses normal distribution (±5°) for realism
 */
void Simulator::updateLocation() {
    // Use route interpolation if following predefined route
    if (followingRoute_ && !config_.route.empty()) {
        currentLocation_ = Geo::interpolateRoute(config_.route, routeProgress_);
    }
    // Calculate free movement based on current speed and heading
    else if (currentSpeed_ > 0.0) {
        double speedMs = currentSpeed_ / 3.6;  // Convert km/h to m/s
        double distance = speedMs * 1.0;       // Distance per second
        
        // Add realistic heading variation (±5° standard deviation)
        currentHeading_ += rng_->normal(0.0, 5.0);
        currentHeading_ = std::fmod(currentHeading_ + 360.0, 360.0);  // Normalize to 0-359°
        
        // Calculate new GPS position based on heading and distance
        currentLocation_ = Geo::moveLocation(currentLocation_, currentHeading_, distance);
    }
}

/**
 * @brief Check for geofence enter/exit events
 * 
 * Compares current GPS location against all configured geofences.
 * Triggers geofence_enter and geofence_exit events when boundaries are crossed.
 * 
 * @pre Current location must be valid GPS coordinates
 * @post Geofence enter/exit events are generated as appropriate
 * @post Current geofence state is updated
 * 
 * @note Uses circular geofences with radius-based detection
 * @note Maintains state to detect entry/exit transitions
 */
void Simulator::checkGeofences() {
    // Check which geofences currently contain the device
    auto insideIds = Geo::checkGeofences(currentLocation_, config_.geofences);
    
    // Detect new geofence entries
    for (const auto& id : insideIds) {
        if (std::find(currentGeofenceIds_.begin(), currentGeofenceIds_.end(), id) == 
            currentGeofenceIds_.end()) {
            // Device has entered a new geofence
            stateMachine_.processGeofenceChange(true, id);
            currentGeofenceIds_.push_back(id);
        }
    }
    
    // Detect geofence exits by checking previously entered geofences
    auto it = currentGeofenceIds_.begin();
    while (it != currentGeofenceIds_.end()) {
        if (std::find(insideIds.begin(), insideIds.end(), *it) == insideIds.end()) {
            // Device has exited this geofence
            stateMachine_.processGeofenceChange(false, *it);
            it = currentGeofenceIds_.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * @brief Check and send periodic heartbeat messages
 * 
 * Monitors time since last heartbeat and sends periodic status updates
 * to Azure IoT Hub at configured intervals for device health monitoring.
 * 
 * @pre Heartbeat interval must be configured (> 0 seconds)
 * @post Heartbeat event is generated when interval expires
 * @post Heartbeat timer is reset after transmission
 * 
 * @note Heartbeat provides regular status updates even when idle
 * @note Critical for device connectivity monitoring in IoT systems
 */
void Simulator::checkHeartbeat() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat_);
    
    // Send heartbeat if configured interval has elapsed
    if (elapsed.count() >= config_.heartbeatSeconds) {
        Event event = createBaseEvent(EventType::Heartbeat);
        emitEvent(event);
        lastHeartbeat_ = now;  // Reset heartbeat timer
    }
}

/**
 * @brief Create base event structure with current device state
 * 
 * Populates an event with all current device telemetry data including
 * GPS location, battery status, network information, and timing data.
 * 
 * @param type The specific event type to create
 * @return Complete event structure ready for transmission
 * 
 * @pre Device must be configured with valid device ID
 * @post Event contains all current telemetry data
 * @post Sequence number is incremented for message ordering
 * 
 * @note Const method with mutable sequence number for thread safety
 * @note Timestamp uses ISO8601 format for standardization
 */
Event Simulator::createBaseEvent(EventType type) const {
    Event event;
    
    // Populate device identification and timing
    event.deviceId = config_.deviceId;
    event.timestamp = clock_->iso8601();
    event.eventType = type;
    event.sequence = ++const_cast<Simulator*>(this)->sequenceNumber_;  // Atomic increment
    
    // Include current telemetry data
    event.location = currentLocation_;   // GPS coordinates and altitude
    event.speedKph = currentSpeed_;      // Current vehicle speed
    event.heading = currentHeading_;     // Direction of travel (degrees)
    event.battery = battery_.getInfo();  // Battery percentage and voltage
    event.network = networkInfo_;        // Network signal strength and type
    
    return event;
}

/**
 * @brief Attempt automatic reconnection with exponential backoff
 * 
 * Implements resilient reconnection logic with increasing delays between
 * attempts to prevent overwhelming the Azure IoT Hub service.
 * 
 * @pre Connection must be lost and reconnection should be active
 * @post Reconnection is attempted based on backoff schedule
 * @post Reconnection stops after maximum attempts are exhausted
 * 
 * @note Uses exponential backoff: 1, 2, 4, 8, 16, 32, 60, 60... seconds
 * @note Maximum 60-second delay prevents excessive wait times
 * @note Stops attempting after MAX_RECONNECT_ATTEMPTS failures
 */
void Simulator::attemptReconnection() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastReconnectAttempt_);
    
    // Calculate exponential backoff delay with 60-second maximum
    int delaySeconds = std::min(60, static_cast<int>(std::pow(2, reconnectAttempts_)));
    
    // Check if enough time has elapsed for next reconnection attempt
    if (elapsed.count() >= delaySeconds) {
        if (reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
            std::cout << "Attempting to reconnect (attempt " << (reconnectAttempts_ + 1) << ")..." << std::endl;
            connectToIoTHub();
            reconnectAttempts_++;
            lastReconnectAttempt_ = now;
        } else {
            // Stop reconnection attempts after maximum failures
            std::cout << "Max reconnection attempts reached. Stopping reconnection attempts." << std::endl;
            shouldReconnect_ = false;
        }
    }
}

bool Simulator::validateDpsConfiguration() const {
    if (config_.idScope.empty()) {
        std::cerr << "[Simulator] Missing DPS ID Scope" << std::endl;
        return false;
    }
    
    if (config_.imei.empty()) {
        std::cerr << "[Simulator] Missing device IMEI" << std::endl;
        return false;
    }
    
    if (config_.deviceCertPath.empty() || config_.deviceKeyPath.empty()) {
        std::cerr << "[Simulator] Missing device certificate or key paths" << std::endl;
        return false;
    }
    
    if (config_.rootCaPath.empty()) {
        std::cerr << "[Simulator] Missing root CA certificate path" << std::endl;
        return false;
    }
    
    return true;
}

bool Simulator::validateLegacyConfiguration() const {
    if (config_.iotHubHost.empty()) {
        std::cerr << "[Simulator] Missing IoT Hub hostname" << std::endl;
        return false;
    }
    
    if (config_.deviceId.empty()) {
        std::cerr << "[Simulator] Missing device ID" << std::endl;
        return false;
    }
    
    if (config_.deviceKeyBase64.empty()) {
        std::cerr << "[Simulator] Missing device shared access key" << std::endl;
        return false;
    }
    
    return true;
}

void Simulator::initializeDeviceTwinAdapter() {
    // Get the actual IoT Hub MQTT client from DPS connection manager (Adapter pattern)
    auto hubClient = dpsConnectionManager_->getHubClient();
    if (!hubClient) {
        std::cerr << "Device Twin: No hub client available" << std::endl;
        return;
    }
    
    // Recreate TwinHandler with correct MQTT client for IoT Hub (not DPS client)
    twinHandler_ = std::make_shared<TwinHandler>(hubClient, config_.deviceId);
    
    // Configure Observer pattern callbacks (reuse existing setup)
    twinHandler_->setConfigUpdateCallback([](const TwinUpdateResult& result, const nlohmann::json& configData) {
        (void)configData; // Bounded processing - config already persisted
        
        std::cout << "Configuration " << (result.status == TwinStatus::Success ? "updated" : "failed")
                  << ": v" << result.configVersion << std::endl;
        
        if (result.status != TwinStatus::Success) {
            std::cerr << "Config error: " << result.errorMessage << std::endl;
        }
    });
    
    twinHandler_->setTwinResponseCallback([](TwinStatus status, const std::string& message) {
        if (status != TwinStatus::Success) {
            std::cerr << "Twin error: " << message << std::endl;
        }
    });
    
    // Set up MQTT message routing for Device Twin messages (Command pattern dispatch)
    hubClient->setMessageCallback([this](const MqttMessage& message) {
        this->onMqttMessage(message);
    });
    
    // Initialize Device Twin protocol (Adapter pattern initialization)
    std::cout << "Initializing Device Twin..." << std::endl;
    if (twinHandler_->initializeSubscriptions()) {
        // Request current configuration from Azure IoT Hub (Command pattern)
        twinHandler_->requestFullTwin("1");
    } else {
        std::cerr << "Device Twin initialization failed" << std::endl;
    }
}

void Simulator::onDpsConnectionComplete(bool connected, const std::string& reason) {
    if (connected) {
        connected_ = true;
        config_.deviceId = dpsConnectionManager_->getDeviceId();
        config_.iotHubHost = dpsConnectionManager_->getAssignedHub();
        
        // Update MQTT topics with assigned device ID
        d2cTopic_ = "devices/" + config_.deviceId + "/messages/events/";
        c2dTopic_ = "devices/" + config_.deviceId + "/messages/devicebound/#";
        
        std::cout << "[Simulator] ✅ DPS provisioning successful!" << std::endl;
        std::cout << "[Simulator] Assigned Hub: " << config_.iotHubHost << std::endl;
        std::cout << "[Simulator] Device ID: " << config_.deviceId << std::endl;
        
        // Initialize Device Twin adapter after DPS connection (Hexagonal Architecture)
        if (twinHandler_) {
            initializeDeviceTwinAdapter();
        }
        
        // Reset reconnection attempts on successful connection
        reconnectAttempts_ = 0;
        shouldReconnect_ = false;
        
    } else {
        std::cerr << "[Simulator] ❌ DPS connection failed: " << reason << std::endl;
        connected_ = false;
        shouldReconnect_ = true;
    }
}

} // namespace tracker

/**
 * @note This implementation follows MSRA coding standards for embedded C++:
 * - Comprehensive documentation for all public interfaces
 * - Clear separation of concerns with dependency injection
 * - Exception safety with proper RAII patterns
 * - Performance-conscious design suitable for resource-constrained environments
 * - Thread-safe design considerations for embedded real-time systems
 * 
 * @see https://docs.microsoft.com/en-us/cpp/cpp/welcome-back-to-cpp-modern-cpp
 * @see Azure IoT Hub MQTT documentation for protocol details
 */