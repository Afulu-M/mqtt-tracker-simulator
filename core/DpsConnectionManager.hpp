/**
 * @file DpsConnectionManager.hpp
 * @brief High-level connection manager for Azure DPS and IoT Hub integration
 * 
 * Provides unified interface for connecting to Azure IoT Hub through Device
 * Provisioning Service (DPS). Manages complete workflow from device provisioning
 * to IoT Hub connectivity with automatic certificate-based authentication.
 * 
 * Features:
 * - Complete DPS provisioning workflow
 * - Automatic IoT Hub connection after provisioning
 * - Certificate validation and error handling
 * - Message routing and topic management
 * - Connection state management
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Follows MSRA C++ coding standards for embedded development
 * @note Uses dependency injection for platform independence
 * @note Designed for production Azure IoT Hub deployments
 */

#pragma once

#include "IMqttClient.hpp"
#include "DpsProvisioning.hpp"
#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace tracker {

/**
 * @brief Device configuration for DPS provisioning and IoT Hub connection
 * 
 * Contains all parameters required for device authentication and connection
 * through Azure DPS. Supports X.509 certificate-based authentication with
 * configurable certificate paths and validation settings.
 * 
 * @note All certificate paths must be absolute paths to PEM files
 * @note IMEI is used as device registration ID in DPS
 * @note Certificate chain should include device cert + intermediate CA
 */
struct DeviceConfig {
    std::string imei;                    ///< Device IMEI (used as registration ID)
    std::string idScope;                 ///< Azure DPS ID Scope
    std::string deviceCertPath;          ///< Path to device certificate (.pem)
    std::string deviceKeyPath;           ///< Path to device private key (.pem)
    std::string deviceChainPath;         ///< Path to certificate chain (.pem)
    std::string rootCaPath;              ///< Path to root CA certificate (.pem)
    bool verifyServerCert = true;        ///< Enable server certificate validation
    std::chrono::seconds timeout{120};   ///< Timeout for provisioning process
    
    /**
     * @brief Validate that all required fields are present
     * @return true if configuration is complete, false otherwise
     */
    bool isValid() const {
        return !imei.empty() && !idScope.empty() && 
               !deviceCertPath.empty() && !deviceKeyPath.empty() && !rootCaPath.empty();
    }
};

/**
 * @brief High-level connection manager for Azure DPS and IoT Hub
 * 
 * Orchestrates complete connection workflow from device provisioning through DPS
 * to active IoT Hub connectivity. Manages multiple MQTT clients, handles connection
 * state transitions, and provides simplified interface for telemetry publishing.
 * 
 * Connection Flow:
 * 1. Validate device configuration and certificates
 * 2. Connect to DPS and perform device provisioning
 * 3. Receive assigned IoT Hub from DPS
 * 4. Connect to assigned IoT Hub with same certificates
 * 5. Set up telemetry and command topics
 * 
 * @note Thread-safe design suitable for embedded real-time systems
 * @note Automatic topic management for Azure IoT Hub conventions
 * @note Comprehensive error handling and logging
 */
class DpsConnectionManager {
public:
    /// Callback function type for connection state changes
    using ConnectionCallback = std::function<void(bool connected, const std::string& reason)>;
    
    /// Callback function type for incoming IoT Hub messages
    using MessageCallback = std::function<void(const MqttMessage&)>;
    
    /**
     * @brief Construct DPS connection manager
     * @param provisioningClient MQTT client for DPS communication
     * @note Creates separate MQTT client for IoT Hub communication
     */
    explicit DpsConnectionManager(std::shared_ptr<IMqttClient> provisioningClient);
    
    /// Destructor - ensures clean disconnection and resource cleanup
    ~DpsConnectionManager() = default;
    
    // Disable copy and assignment to prevent resource management issues
    DpsConnectionManager(const DpsConnectionManager&) = delete;
    DpsConnectionManager& operator=(const DpsConnectionManager&) = delete;
    DpsConnectionManager(DpsConnectionManager&&) = default;
    DpsConnectionManager& operator=(DpsConnectionManager&&) = default;
    
    /**
     * @brief Connect to IoT Hub through DPS provisioning
     * @param config Device configuration with certificates and DPS settings
     * @param callback Function to call when connection process completes
     * @note Connection is asynchronous - use processEvents() to advance
     */
    void connectToIotHub(const DeviceConfig& config, ConnectionCallback callback);
    
    /**
     * @brief Disconnect from all services (DPS and IoT Hub)
     * @note Safe to call multiple times - cleans up all resources
     */
    void disconnect();
    
    /**
     * @brief Check if currently connected to IoT Hub
     * @return true if connected and ready for telemetry, false otherwise
     */
    bool isConnected() const;
    
    /**
     * @brief Publish telemetry message to IoT Hub
     * @param topic Relative topic (e.g., "" for default telemetry)
     * @param payload Message payload (typically JSON telemetry data)
     * @param qos Quality of Service level (0, 1, or 2)
     * @param retained Whether message should be retained by broker
     * @return true if publish succeeded, false otherwise
     * @note Topic is automatically prefixed with device-to-cloud path
     */
    bool publish(const std::string& topic, const std::string& payload, 
                int qos = 0, bool retained = false);
    
    /**
     * @brief Subscribe to IoT Hub command topic
     * @param topic Relative topic (empty for default commands)
     * @param qos Maximum Quality of Service level for received messages
     * @return true if subscription succeeded, false otherwise
     * @note Topic is automatically prefixed with cloud-to-device path
     */
    bool subscribe(const std::string& topic, int qos = 0);
    
    /**
     * @brief Unsubscribe from IoT Hub topic
     * @param topic Topic to unsubscribe from
     * @return true if unsubscription succeeded, false otherwise
     */
    bool unsubscribe(const std::string& topic);
    
    /**
     * @brief Set callback for incoming IoT Hub messages
     * @param callback Function to call when message is received
     * @note Only applies to IoT Hub messages, not DPS messages
     */
    void setMessageCallback(MessageCallback callback);
    
    /**
     * @brief Process pending connection and message events
     * @note Must be called regularly for asynchronous operation
     * @note Non-blocking for embedded compatibility
     */
    void processEvents();
    
    /**
     * @brief Get assigned IoT Hub hostname from DPS
     * @return IoT Hub hostname, or empty string if not provisioned
     */
    std::string getAssignedHub() const { return assignedHub_; }
    
    /**
     * @brief Get assigned device ID from DPS
     * @return Device ID for IoT Hub, or empty string if not provisioned
     */
    std::string getDeviceId() const { return deviceId_; }
    
    /**
     * @brief Get IoT Hub MQTT client for protocol adapters (Hexagonal Architecture)
     * @return Shared pointer to hub MQTT client, or nullptr if not connected
     * 
     * Provides access to the actual IoT Hub MQTT client for implementing
     * protocol-specific adapters (e.g., Device Twin, Direct Methods, File Upload).
     * Follows Hexagonal Architecture by enabling clean separation between
     * connection management and protocol-specific functionality.
     * 
     * @note Only available after successful DPS provisioning and hub connection
     * @note Used by adapters requiring direct MQTT protocol access
     * @see TwinHandler for Device Twin protocol adapter implementation
     */
    std::shared_ptr<IMqttClient> getHubClient() const { return hubClient_; }
    
private:
    /// Connection state machine for DPS and IoT Hub workflow
    enum class ConnectionState {
        Disconnected,      ///< Not connected to any service
        Provisioning,      ///< DPS provisioning in progress
        ConnectingToHub,   ///< Connecting to assigned IoT Hub
        Connected,         ///< Connected and ready for telemetry
        Failed            ///< Connection failed - check error message
    };
    
    /// Default timeout for IoT Hub connection (seconds)
    static constexpr std::chrono::seconds kHubConnectionTimeout{30};
    
    std::shared_ptr<IMqttClient> provisioningClient_;  ///< MQTT client for DPS communication
    std::shared_ptr<IMqttClient> hubClient_;           ///< MQTT client for IoT Hub communication
    std::unique_ptr<DpsProvisioning> dpsProvisioning_; ///< DPS provisioning manager
    
    ConnectionState state_ = ConnectionState::Disconnected; ///< Current connection state
    DeviceConfig config_;                               ///< Current device configuration
    ConnectionCallback connectionCallback_;             ///< User callback for connection events
    MessageCallback messageCallback_;                   ///< User callback for IoT Hub messages
    
    std::string assignedHub_;                          ///< IoT Hub hostname from DPS
    std::string deviceId_;                             ///< Device ID from DPS
    
    /**
     * @brief Handle completion of DPS provisioning process
     * @param result Provisioning result with hub assignment or error
     */
    void onProvisioningComplete(const ProvisioningResult& result);
    
    /**
     * @brief Handle IoT Hub connection status changes
     * @param connected Whether IoT Hub connection succeeded
     * @param reason Connection status description
     */
    void onHubConnected(bool connected, const std::string& reason);
    
    /**
     * @brief Handle incoming messages from IoT Hub
     * @param message Received MQTT message from IoT Hub
     */
    void onHubMessage(const MqttMessage& message);
    
    /**
     * @brief Validate certificate files exist and are readable
     * @param config Device configuration with certificate paths
     * @return true if all certificate files are accessible, false otherwise
     */
    bool validateCertificatePaths(const DeviceConfig& config) const;
    
    /**
     * @brief Build device-to-cloud MQTT topic for telemetry
     * @return Full MQTT topic path for publishing telemetry
     */
    std::string buildDeviceTelemetryTopic() const;
    
    /**
     * @brief Build cloud-to-device MQTT topic for commands
     * @return Full MQTT topic path for receiving commands
     */
    std::string buildDeviceCommandTopic() const;
};

} // namespace tracker