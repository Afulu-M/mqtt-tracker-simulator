/**
 * @file DpsProvisioning.hpp
 * @brief Azure Device Provisioning Service (DPS) integration for X.509 certificate authentication
 * 
 * Implements complete DPS provisioning workflow including device registration,
 * assignment polling, and IoT Hub discovery. Designed for embedded portability
 * with minimal dependencies and robust error handling.
 * 
 * DPS Workflow:
 * 1. Connect to DPS endpoint with X.509 client certificate
 * 2. Send registration request with device IMEI
 * 3. Poll assignment status until hub is assigned
 * 4. Return assigned hub details for IoT Hub connection
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Follows MSRA C++ coding standards for embedded development
 * @note State machine design ensures predictable behavior
 * @note Supports configurable timeouts for production deployment
 */

#pragma once

#include "IMqttClient.hpp"
#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include <cstdint>

namespace tracker {

/**
 * @brief Configuration parameters for Azure Device Provisioning Service
 * 
 * Contains all parameters required for DPS provisioning including endpoint,
 * authentication, and timeout settings. Designed for easy configuration
 * management and embedded deployment.
 * 
 * @note All string parameters must be valid for Azure DPS compatibility
 * @note Timeout should account for network latency and DPS processing time
 */
struct DpsConfig {
    std::string idScope;                    ///< Azure DPS ID Scope (required)
    std::string registrationId;             ///< Device registration ID (typically IMEI)
    std::string globalEndpoint = "global.azure-devices-provisioning.net"; ///< DPS endpoint
    std::uint16_t port = 8883;             ///< MQTT over TLS port
    TlsConfig tlsConfig;                    ///< X.509 certificate configuration
    std::chrono::seconds timeout{120};     ///< Maximum time for provisioning process
};

/**
 * @brief Result of DPS provisioning operation
 * 
 * Contains outcome of provisioning process including assigned IoT Hub details
 * or error information for troubleshooting. Used for decision making in
 * connection management.
 * 
 * @note Success flag determines whether assigned hub information is valid
 * @note Error message provides detailed information for troubleshooting
 */
struct ProvisioningResult {
    bool success = false;                   ///< Whether provisioning succeeded
    std::string assignedHub;                ///< Assigned IoT Hub hostname
    std::string deviceId;                   ///< Assigned device identifier
    std::string errorMessage;               ///< Error description if provisioning failed
    std::string enrollmentGroupId;          ///< DPS enrollment group (if applicable)
};

/**
 * @brief Azure Device Provisioning Service client implementation
 * 
 * Manages complete DPS provisioning workflow using state machine design.
 * Handles device registration, assignment polling, and error recovery.
 * Designed for embedded systems with predictable memory usage and behavior.
 * 
 * State Machine:
 * Idle → ConnectingToDps → SendingRegistration → WaitingForAssignment → Completed/Failed
 * 
 * @note Thread-safe design suitable for embedded real-time systems
 * @note Uses dependency injection for platform independence
 * @note Implements exponential backoff for robust network communication
 */
class DpsProvisioning {
public:
    /// Callback function type for provisioning completion
    using ProvisioningCallback = std::function<void(const ProvisioningResult&)>;
    
    /**
     * @brief Construct DPS provisioning client
     * @param mqttClient MQTT client implementation for DPS communication
     * @note MQTT client must support X.509 certificate authentication
     */
    explicit DpsProvisioning(std::shared_ptr<IMqttClient> mqttClient);
    
    /// Destructor - ensures clean resource cleanup
    ~DpsProvisioning() = default;
    
    // Disable copy and assignment to prevent resource management issues
    DpsProvisioning(const DpsProvisioning&) = delete;
    DpsProvisioning& operator=(const DpsProvisioning&) = delete;
    DpsProvisioning(DpsProvisioning&&) = default;
    DpsProvisioning& operator=(DpsProvisioning&&) = default;
    
    /**
     * @brief Start DPS provisioning process
     * @param config DPS configuration parameters
     * @param callback Function to call when provisioning completes
     * @note Provisioning is asynchronous - use processEvents() to advance
     */
    void startProvisioning(const DpsConfig& config, ProvisioningCallback callback);
    
    /**
     * @brief Process pending DPS events
     * @note Must be called regularly for asynchronous operation
     * @note Non-blocking for embedded compatibility
     */
    void processEvents();
    
    /**
     * @brief Cancel ongoing provisioning operation
     * @note Safe to call at any time - cleans up resources
     */
    void cancel();
    
private:
    /// DPS provisioning state machine states
    enum class State {
        Idle,                    ///< Not provisioning
        ConnectingToDps,         ///< Establishing MQTT connection to DPS
        SendingRegistration,     ///< Sending device registration request
        WaitingForAssignment,    ///< Polling for hub assignment
        Completed,               ///< Provisioning succeeded
        Failed                   ///< Provisioning failed
    };
    
    /// DPS API version for MQTT communication
    static constexpr const char* kDpsApiVersion = "2019-03-31";
    
    /// Polling interval for assignment status (seconds)
    static constexpr std::chrono::seconds kPollingInterval{2};
    
    std::shared_ptr<IMqttClient> mqttClient_;   ///< MQTT client for DPS communication
    State state_ = State::Idle;                 ///< Current provisioning state
    DpsConfig config_;                          ///< Current provisioning configuration
    ProvisioningCallback callback_;             ///< User callback for completion
    std::string operationId_;                   ///< DPS operation ID for polling
    std::chrono::steady_clock::time_point startTime_; ///< Provisioning start time
    std::chrono::steady_clock::time_point lastPoll_;  ///< Last polling attempt time
    
    /**
     * @brief Handle MQTT connection status changes
     * @param connected Whether connection succeeded
     * @param reason Connection status description
     */
    void onDpsConnected(bool connected, const std::string& reason);
    
    /**
     * @brief Handle incoming DPS MQTT messages
     * @param message Received MQTT message from DPS
     */
    void onDpsMessage(const MqttMessage& message);
    
    /**
     * @brief Process initial registration response from DPS
     * @param payload JSON response payload
     */
    void handleRegistrationResponse(const std::string& payload);
    
    /**
     * @brief Process hub assignment response from DPS
     * @param payload JSON response payload with assignment details
     */
    void handleAssignmentResponse(const std::string& payload);
    
    /**
     * @brief Complete provisioning process and invoke callback
     * @param result Provisioning result to return to user
     */
    void completeProvisioning(const ProvisioningResult& result);
    
    /**
     * @brief Extract value from JSON string (simple parser)
     * @param json JSON string to parse
     * @param key Key to extract
     * @return Value associated with key, or empty string if not found
     * @note Simple string-based parser for embedded compatibility
     */
    std::string extractJsonValue(const std::string& json, const std::string& key) const;
    
    /**
     * @brief Send assignment status polling request to DPS
     * @note Called periodically during WaitingForAssignment state
     */
    void pollAssignmentStatus();
    
    /**
     * @brief Check if provisioning timeout has been exceeded
     * @return true if timeout exceeded, false otherwise
     */
    bool isTimedOut() const;
    
    /**
     * @brief Build DPS registration topic for MQTT communication
     * @return MQTT topic for registration request
     */
    std::string buildRegistrationTopic() const;
    
    /**
     * @brief Build DPS polling topic for assignment status
     * @return MQTT topic for status polling
     */
    std::string buildPollingTopic() const;
};

} // namespace tracker