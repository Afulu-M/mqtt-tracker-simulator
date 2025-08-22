/**
 * @file TwinHandler.hpp
 * @brief Azure IoT Hub Device Twin adapter following Hexagonal Architecture
 * 
 * This adapter isolates Device Twin configuration management from the domain core,
 * implementing the Command pattern for configuration updates and Observer pattern
 * for configuration change notifications. Maintains clean separation between
 * Azure IoT Hub specifics and domain configuration logic.
 * 
 * Architecture:
 * - Port: Configuration management interface
 * - Adapter: Azure IoT Hub Device Twin protocol implementation
 * - Strategy: Configurable acknowledgment patterns
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Follows Hexagonal Architecture principles for testability
 * @note Thread-safe with bounded resource usage for embedded compatibility
 * @note Deterministic behavior with explicit error handling
 */

#pragma once

#include "IMqttClient.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>

namespace tracker {

// ============================================================================
// Domain Types - Status and Results (Strategy Pattern Support)
// ============================================================================

/**
 * @brief Device Twin operation status codes following fail-fast principle
 * 
 * Explicit error types enable deterministic error handling and support
 * different retry strategies in production vs simulation environments.
 */
enum class TwinStatus {
    Success = 0,           ///< Operation completed successfully
    JsonParseError,        ///< Failed to parse JSON payload (fail fast)
    FileWriteError,        ///< Failed to write configuration file (fail safe)
    MqttError,             ///< MQTT communication failure (retry candidate)
    InvalidResponse        ///< Received unexpected response format (fail fast)
};

/**
 * @brief Configuration update result with metadata
 * 
 * Immutable result object containing all information needed for
 * configuration tracking, metrics, and acknowledgment generation.
 * Supports Command pattern for configuration application.
 */
struct TwinUpdateResult {
    TwinStatus status;              ///< Operation outcome (for Strategy pattern)
    std::string errorMessage;       ///< Detailed error description (empty on success)
    std::string configVersion;      ///< Applied configuration version (for tracking)
    std::string appliedAt;          ///< ISO8601 timestamp when applied (for audit)
    bool hasChanges = false;        ///< Whether configuration changed (for optimization)
};

// ============================================================================
// Adapter Implementation - Azure IoT Hub Device Twin Protocol
// ============================================================================

/**
 * @brief Azure IoT Hub Device Twin protocol adapter
 * 
 * Implements configuration management port using Azure IoT Hub Device Twin
 * protocol over MQTT. Follows Hexagonal Architecture by isolating Azure-specific
 * protocol details from domain configuration logic.
 * 
 * Design Patterns:
 * - Adapter: Wraps Azure IoT Hub Device Twin protocol
 * - Command: Configuration updates as command objects
 * - Observer: Event-driven configuration change notifications
 * - Strategy: Pluggable acknowledgment and error handling policies
 * 
 * Embedded Considerations:
 * - Bounded resource usage (pre-sized buffers, no unbounded queues)
 * - Deterministic behavior with explicit error handling
 * - Thread-safe with minimal locking (single mutex for state)
 * - RAII resource management with automatic cleanup
 * 
 * @invariant Connected MQTT client required for all operations
 * @invariant All configuration changes result in atomic file updates
 * @invariant Thread-safe concurrent access to configuration state
 */
class TwinHandler {
public:
    // ========================================================================
    // Observer Pattern - Event Callbacks (Ports)
    // ========================================================================
    
    /// Configuration update event callback (Observer pattern)
    using ConfigUpdateCallback = std::function<void(const TwinUpdateResult&, const nlohmann::json&)>;
    
    /// Twin operation completion callback (Command pattern result)
    using TwinResponseCallback = std::function<void(TwinStatus status, const std::string& message)>;
    
    /**
     * @brief Construct Device Twin handler with MQTT client
     * @param mqttClient Shared MQTT client for Device Twin communication
     * @param deviceId Device identifier for topic construction
     * @pre mqttClient must be valid and connected to Azure IoT Hub
     */
    explicit TwinHandler(std::shared_ptr<IMqttClient> mqttClient, const std::string& deviceId);
    
    /**
     * @brief Destructor - ensures clean shutdown and resource cleanup
     */
    ~TwinHandler();
    
    // Disable copy and assignment to prevent resource management issues
    TwinHandler(const TwinHandler&) = delete;
    TwinHandler& operator=(const TwinHandler&) = delete;
    TwinHandler(TwinHandler&&) = delete;
    TwinHandler& operator=(TwinHandler&&) = delete;
    
    /**
     * @brief Initialize Device Twin subscriptions
     * @return true if subscriptions were successful, false otherwise
     * @note Must be called before requesting twin or processing updates
     * @note Subscribes to both response and desired property PATCH topics
     */
    bool initializeSubscriptions();
    
    /**
     * @brief Request full Device Twin from Azure IoT Hub
     * @param requestId Correlation ID for tracking the request (default: "1")
     * @return true if request was sent successfully, false otherwise
     * @note Response will be delivered via MQTT callback asynchronously
     */
    bool requestFullTwin(const std::string& requestId = "1");
    
    /**
     * @brief Send reported properties acknowledgment to Azure IoT Hub
     * @param requestId Correlation ID for tracking the response (default: "2")
     * @param reportedProperties JSON object with properties to report
     * @return true if acknowledgment was sent successfully, false otherwise
     * @note Used to confirm successful application of desired properties
     */
    bool sendReportedAck(const std::string& requestId, const nlohmann::json& reportedProperties);
    
    /**
     * @brief Set callback for configuration updates
     * @param callback Function to call when desired properties are received and processed
     * @note Callback is called from MQTT thread - ensure thread safety
     */
    void setConfigUpdateCallback(ConfigUpdateCallback callback);
    
    /**
     * @brief Set callback for twin operation responses
     * @param callback Function to call when twin operations complete
     * @note Callback is called from MQTT thread - ensure thread safety
     */
    void setTwinResponseCallback(TwinResponseCallback callback);
    
    /**
     * @brief Process incoming MQTT message for Device Twin handling
     * @param message MQTT message with topic and payload
     * @note Called internally by MQTT message callback
     * @note Handles both twin responses and desired property patches
     */
    void handleMqttMessage(const MqttMessage& message);
    
    /**
     * @brief Get last known configuration version
     * @return Configuration version string from last successful update
     */
    std::string getConfigVersion() const;
    
    /**
     * @brief Check if Device Twin handler is properly initialized
     * @return true if subscriptions are active, false otherwise
     */
    bool isInitialized() const { return initialized_; }

private:
    /// Configuration file path for applied desired properties
    static constexpr const char* kConfigFilePath = "./config_applied.json";
    
    /// Error file path for JSON parsing failures
    static constexpr const char* kErrorFilePath = "./config_error.json";
    
    /// Azure IoT Hub Device Twin topic prefixes
    static constexpr const char* kTwinResponseTopic = "$iothub/twin/res/";
    static constexpr const char* kTwinPatchTopic = "$iothub/twin/PATCH/properties/desired/";
    static constexpr const char* kTwinGetTopic = "$iothub/twin/GET/";
    static constexpr const char* kTwinReportedTopic = "$iothub/twin/PATCH/properties/reported/";
    
    std::shared_ptr<IMqttClient> mqttClient_;    ///< MQTT client for Device Twin communication
    std::string deviceId_;                       ///< Device identifier for topic construction
    bool initialized_ = false;                   ///< Subscription initialization status
    
    ConfigUpdateCallback configUpdateCallback_; ///< User callback for configuration updates
    TwinResponseCallback twinResponseCallback_;  ///< User callback for twin operation responses
    
    mutable std::mutex configMutex_;            ///< Mutex protecting configuration state
    std::string currentConfigVersion_;          ///< Last successfully applied configuration version
    
    /**
     * @brief Process Device Twin GET response
     * @param topic Complete MQTT topic with status code and request ID
     * @param payload JSON payload containing full Device Twin
     * @note Extracts desired properties and writes to local file
     */
    void processTwinResponse(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Process desired properties PATCH update
     * @param topic Complete MQTT topic with PATCH metadata
     * @param payload JSON payload containing updated desired properties
     * @note Incrementally updates existing configuration
     */
    void processDesiredPatch(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Apply desired properties and write to configuration file
     * @param desired JSON object containing desired properties
     * @return Result of the configuration application process
     * @note Validates JSON structure and writes to local file system
     */
    TwinUpdateResult applyDesiredAndWriteFile(const nlohmann::json& desired);
    
    /**
     * @brief Write error information to error file
     * @param rawPayload Original JSON payload that failed to parse
     * @param errorMessage Detailed error description
     * @note Creates error file for debugging and troubleshooting
     */
    void writeErrorFile(const std::string& rawPayload, const std::string& errorMessage);
    
    /**
     * @brief Extract request ID from Device Twin MQTT topic
     * @param topic Complete MQTT topic string
     * @return Request ID string or empty string if not found
     * @note Used for correlating requests and responses
     */
    std::string extractRequestId(const std::string& topic) const;
    
    /**
     * @brief Extract HTTP status code from twin response topic
     * @param topic Complete MQTT topic string
     * @return HTTP status code (e.g., 200, 400, 404) or 0 if not found
     */
    int extractStatusCode(const std::string& topic) const;
    
    /**
     * @brief Generate ISO8601 timestamp for configuration metadata
     * @return Current UTC time in ISO8601 format (e.g., "2025-08-21T14:30:15Z")
     */
    std::string getCurrentTimestamp() const;
    
    /**
     * @brief Create default reported properties acknowledgment
     * @param appliedConfig Applied configuration JSON object
     * @param result Application result with status and metadata
     * @return JSON object suitable for reported properties PATCH
     */
    nlohmann::json createDefaultReportedAck(const nlohmann::json& appliedConfig, 
                                           const TwinUpdateResult& result) const;
    
    /**
     * @brief Validate that desired properties contain expected structure
     * @param desired JSON object with desired properties
     * @return true if structure is valid, false otherwise
     * @note Checks for presence of known configuration keys
     */
    bool validateDesiredStructure(const nlohmann::json& desired) const;
};

} // namespace tracker