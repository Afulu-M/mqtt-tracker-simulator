/**
 * @file IMqttClient.hpp
 * @brief MQTT client interface for Azure IoT Hub and DPS connectivity
 * 
 * Provides platform-independent MQTT client abstraction supporting both
 * SAS token authentication (legacy) and X.509 certificate authentication (DPS).
 * Designed for embedded portability with minimal dependencies.
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Follows MSRA C++ coding standards for embedded development
 * @note Interface supports both synchronous and asynchronous MQTT operations
 * @note TLS configuration enables secure Azure IoT Hub connections
 */

#pragma once

#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace tracker {

/**
 * @brief MQTT message structure for device-to-cloud and cloud-to-device communication
 * 
 * Encapsulates all MQTT message properties including topic, payload, and QoS settings.
 * Used for both incoming and outgoing messages in Azure IoT Hub communication.
 * 
 * @note All string fields must be valid UTF-8 for Azure IoT Hub compatibility
 * @note QoS levels: 0 (at most once), 1 (at least once), 2 (exactly once)
 */
struct MqttMessage {
    std::string topic;              ///< MQTT topic (e.g., "devices/{deviceId}/messages/events/")
    std::string payload;            ///< Message payload (typically JSON telemetry)
    int qos = 0;                   ///< Quality of Service level (0, 1, or 2)
    bool retained = false;         ///< Retain flag for persistent messages
};

/**
 * @brief TLS configuration for X.509 certificate-based authentication
 * 
 * Contains all parameters required for secure TLS connections to Azure DPS
 * and IoT Hub using client certificates. Supports certificate chain validation
 * and configurable server verification for testing and production environments.
 * 
 * @note Certificate files must be in PEM format for cross-platform compatibility
 * @note Private key must match the public key in the certificate
 * @note CA certificate must be trusted by Azure DPS/IoT Hub
 */
struct TlsConfig {
    std::string certPath;          ///< Path to client certificate file (.pem)
    std::string keyPath;           ///< Path to private key file (.pem)
    std::string caPath;            ///< Path to root CA certificate file (.pem)
    bool verifyServer = true;      ///< Enable server certificate validation
};

/**
 * @brief Platform-independent MQTT client interface
 * 
 * Provides abstract interface for MQTT communication supporting both legacy
 * SAS token authentication and modern X.509 certificate authentication.
 * Designed for Azure IoT Hub and Device Provisioning Service connectivity.
 * 
 * @note Interface supports both blocking and non-blocking operations
 * @note Callback-based design enables event-driven architecture
 * @note Platform implementations can use Paho MQTT (desktop) or coreMQTT (embedded)
 */
class IMqttClient {
public:
    /// Virtual destructor for proper cleanup in derived classes
    virtual ~IMqttClient() = default;
    
    /// Callback function type for incoming MQTT messages
    using MessageCallback = std::function<void(const MqttMessage&)>;
    
    /// Callback function type for connection state changes
    using ConnectionCallback = std::function<void(bool connected, const std::string& reason)>;
    
    /**
     * @brief Connect to MQTT broker using username/password authentication
     * @param host MQTT broker hostname (e.g., "your-hub.azure-devices.net")
     * @param port MQTT broker port (typically 8883 for TLS, 1883 for non-TLS)
     * @param clientId Unique client identifier (device ID for Azure IoT Hub)
     * @param username MQTT username (includes API version for Azure IoT Hub)
     * @param password MQTT password (SAS token for Azure IoT Hub)
     * @return true if connection initiated successfully, false otherwise
     * @note This method initiates asynchronous connection - use callback for status
     */
    virtual bool connect(const std::string& host, std::uint16_t port, 
                        const std::string& clientId,
                        const std::string& username, 
                        const std::string& password) = 0;
    
    /**
     * @brief Connect to MQTT broker using X.509 certificate authentication
     * @param host MQTT broker hostname (e.g., "global.azure-devices-provisioning.net")
     * @param port MQTT broker port (typically 8883 for TLS)
     * @param clientId Unique client identifier (registration ID for DPS)
     * @param username MQTT username (includes DPS scope and API version)
     * @param tlsConfig TLS configuration with certificate paths
     * @return true if connection initiated successfully, false otherwise
     * @note This method initiates asynchronous connection - use callback for status
     */
    virtual bool connectWithTls(const std::string& host, std::uint16_t port,
                               const std::string& clientId,
                               const std::string& username,
                               const TlsConfig& tlsConfig) = 0;
    
    /**
     * @brief Disconnect from MQTT broker
     * @note Closes connection gracefully and cleans up resources
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Check if currently connected to MQTT broker
     * @return true if connected, false otherwise
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Publish message to MQTT topic
     * @param topic MQTT topic to publish to
     * @param payload Message payload (typically JSON for Azure IoT Hub)
     * @param qos Quality of Service level (0, 1, or 2)
     * @param retained Whether message should be retained by broker
     * @return true if publish succeeded, false otherwise
     * @note Message may be queued if not currently connected
     */
    virtual bool publish(const std::string& topic, const std::string& payload, 
                        int qos = 0, bool retained = false) = 0;
    
    /**
     * @brief Subscribe to MQTT topic
     * @param topic MQTT topic to subscribe to (supports wildcards)
     * @param qos Maximum Quality of Service level for received messages
     * @return true if subscription succeeded, false otherwise
     */
    virtual bool subscribe(const std::string& topic, int qos = 0) = 0;
    
    /**
     * @brief Unsubscribe from MQTT topic
     * @param topic MQTT topic to unsubscribe from
     * @return true if unsubscription succeeded, false otherwise
     */
    virtual bool unsubscribe(const std::string& topic) = 0;
    
    /**
     * @brief Set callback for incoming MQTT messages
     * @param callback Function to call when message is received
     * @note Callback is called from MQTT thread - ensure thread safety
     */
    virtual void setMessageCallback(MessageCallback callback) = 0;
    
    /**
     * @brief Set callback for connection state changes
     * @param callback Function to call when connection state changes
     * @note Callback is called from MQTT thread - ensure thread safety
     */
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;
    
    /**
     * @brief Process pending MQTT events
     * @note Must be called regularly for asynchronous message processing
     * @note Implementation should be non-blocking for embedded compatibility
     */
    virtual void processEvents() = 0;

protected:
    // Protected constructors to prevent direct instantiation
    IMqttClient() = default;
    IMqttClient(const IMqttClient&) = default;
    IMqttClient& operator=(const IMqttClient&) = default;
    IMqttClient(IMqttClient&&) = default;
    IMqttClient& operator=(IMqttClient&&) = default;
};

} // namespace tracker