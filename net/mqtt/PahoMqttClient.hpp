/**
 * @file PahoMqttClient.hpp
 * @brief Paho MQTT C library implementation for desktop platforms
 * 
 * Provides MQTT client implementation using Eclipse Paho MQTT C library.
 * Supports both SAS token and X.509 certificate authentication for Azure IoT Hub.
 * Includes offline message queuing and automatic reconnection capabilities.
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note For embedded platforms, replace with coreMQTT or Paho Embedded C
 * @note Thread-safe implementation with proper callback handling
 * @note Follows MSRA C++ coding standards for production use
 */

#pragma once

#include "IMqttClient.hpp"
#include <MQTTAsync.h>
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <cstdint>

namespace tracker {

/**
 * @brief Paho MQTT C library implementation for desktop platforms
 * 
 * Concrete implementation of IMqttClient using Eclipse Paho MQTT C library.
 * Provides asynchronous MQTT operations with callback-based event handling.
 * Supports both username/password and X.509 certificate authentication.
 * 
 * Features:
 * - Offline message queuing with configurable limits
 * - Automatic reconnection with exponential backoff
 * - Thread-safe callback handling
 * - Azure IoT Hub specific optimizations
 * 
 * @note This implementation is optimized for desktop/server environments
 * @note For embedded targets, consider using coreMQTT or Paho Embedded C
 */
class PahoMqttClient : public IMqttClient {
public:
    /**
     * @brief Construct new Paho MQTT client instance
     * @note Client is not connected after construction - call connect() method
     */
    PahoMqttClient();
    
    /**
     * @brief Destructor - ensures clean disconnection and resource cleanup
     * @note Automatically disconnects if still connected
     */
    ~PahoMqttClient() override;
    
    // Disable copy and assignment to prevent resource management issues
    PahoMqttClient(const PahoMqttClient&) = delete;
    PahoMqttClient& operator=(const PahoMqttClient&) = delete;
    PahoMqttClient(PahoMqttClient&&) = delete;
    PahoMqttClient& operator=(PahoMqttClient&&) = delete;
    
    bool connect(const std::string& host, std::uint16_t port, 
                const std::string& clientId,
                const std::string& username, 
                const std::string& password) override;
    
    bool connectWithTls(const std::string& host, std::uint16_t port,
                       const std::string& clientId,
                       const std::string& username,
                       const TlsConfig& tlsConfig) override;
    
    void disconnect() override;
    bool isConnected() const override;
    
    bool publish(const std::string& topic, const std::string& payload, 
                int qos = 0, bool retained = false) override;
    
    bool subscribe(const std::string& topic, int qos = 0) override;
    bool unsubscribe(const std::string& topic) override;
    
    void setMessageCallback(MessageCallback callback) override;
    void setConnectionCallback(ConnectionCallback callback) override;
    
    void processEvents() override;
    
private:
    /// Maximum number of messages to queue when offline
    static constexpr std::size_t kMaxOfflineQueueSize = 100;
    
    /// Azure IoT Hub recommended keep-alive interval (seconds)
    static constexpr int kKeepAliveIntervalSeconds = 240;
    
    /// Connection timeout for Azure services (seconds)
    static constexpr int kConnectionTimeoutSeconds = 30;
    
    MQTTAsync client_;                    ///< Paho MQTT client handle
    bool connected_ = false;              ///< Current connection state
    
    MessageCallback messageCallback_;     ///< User callback for incoming messages
    ConnectionCallback connectionCallback_; ///< User callback for connection events
    
    std::queue<MqttMessage> offlineQueue_; ///< Queue for messages when offline
    std::mutex queueMutex_;               ///< Mutex protecting offline queue
    
    /**
     * @brief Static callback for incoming MQTT messages
     * @param context Pointer to PahoMqttClient instance
     * @param topicName MQTT topic name (null-terminated)
     * @param topicLen Length of topic name (unused - topic is null-terminated)
     * @param message Paho message structure with payload and metadata
     * @return 1 to indicate successful message processing
     */
    static int messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);
    
    /**
     * @brief Static callback for successful MQTT connection
     * @param context Pointer to PahoMqttClient instance
     * @param response Success response data (may contain session info)
     */
    static void onConnected(void* context, MQTTAsync_successData* response);
    
    /**
     * @brief Static callback for failed MQTT connection
     * @param context Pointer to PahoMqttClient instance
     * @param response Failure response with error code and message
     */
    static void onConnectFailure(void* context, MQTTAsync_failureData* response);
    
    /**
     * @brief Static callback for lost MQTT connection
     * @param context Pointer to PahoMqttClient instance
     * @param cause Reason for connection loss (may be null)
     */
    static void connectionLost(void* context, char* cause);
    
    /**
     * @brief Static callback for successful disconnection
     * @param context Pointer to PahoMqttClient instance
     * @param response Success response data
     */
    static void onDisconnected(void* context, MQTTAsync_successData* response);
    
    /**
     * @brief Send all queued messages when connection is restored
     * @note Called automatically when connection is established
     */
    void flushOfflineQueue();
    
    /**
     * @brief Add message to offline queue when not connected
     * @param topic MQTT topic for message
     * @param payload Message payload
     * @param qos Quality of Service level
     * @param retained Whether message should be retained
     * @note Queue has maximum size limit to prevent memory exhaustion
     */
    void queueMessage(const std::string& topic, const std::string& payload, 
                     int qos, bool retained);
    
    /**
     * @brief Validate certificate files exist and are readable
     * @param tlsConfig TLS configuration with certificate paths
     * @return true if all certificate files are accessible, false otherwise
     */
    bool validateCertificateFiles(const TlsConfig& tlsConfig) const;
};

} // namespace tracker