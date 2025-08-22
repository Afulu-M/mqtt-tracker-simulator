#include "PahoMqttClient.hpp"
#include <iostream>
#include <cstring>
#include <fstream>

namespace tracker {

PahoMqttClient::PahoMqttClient() : client_(nullptr) {}

PahoMqttClient::~PahoMqttClient() {
    disconnect();
    if (client_) {
        MQTTAsync_destroy(&client_);
    }
}

bool PahoMqttClient::connect(const std::string& host, std::uint16_t port, 
                            const std::string& clientId,
                            const std::string& username, 
                            const std::string& password) {
    
    std::string serverURI = "ssl://" + host + ":" + std::to_string(port);
    
    int rc = MQTTAsync_create(&client_, serverURI.c_str(), clientId.c_str(), 
                             MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTASYNC_SUCCESS) {
        return false;
    }
    
    MQTTAsync_setCallbacks(client_, this, connectionLost, messageArrived, nullptr);
    
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    
    conn_opts.keepAliveInterval = kKeepAliveIntervalSeconds;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = kConnectionTimeoutSeconds;
    conn_opts.retryInterval = 5;        // 5 seconds between retries
    conn_opts.onSuccess = onConnected;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = this;
    conn_opts.username = username.c_str();
    conn_opts.password = password.c_str();
    conn_opts.ssl = &ssl_opts;
    
    ssl_opts.enableServerCertAuth = 0;  // Disable cert verification for testing
    ssl_opts.verify = 0;
    
    rc = MQTTAsync_connect(client_, &conn_opts);
    return rc == MQTTASYNC_SUCCESS;
}

bool PahoMqttClient::connectWithTls(const std::string& host, std::uint16_t port,
                                   const std::string& clientId,
                                   const std::string& username,
                                   const TlsConfig& tlsConfig) {
    
    std::cout << "[MQTT] Connecting with TLS to " << host << ":" << port << std::endl;
    std::cout << "[MQTT] Client ID: " << clientId << std::endl;
    std::cout << "[MQTT] Username: " << username << std::endl;
    std::cout << "[MQTT] Cert: " << tlsConfig.certPath << std::endl;
    std::cout << "[MQTT] Key: " << tlsConfig.keyPath << std::endl;
    std::cout << "[MQTT] CA: " << tlsConfig.caPath << std::endl;
    
    // Validate certificate files before attempting connection
    if (!validateCertificateFiles(tlsConfig)) {
        return false;
    }
    
    std::string serverURI = "ssl://" + host + ":" + std::to_string(port);
    
    int rc = MQTTAsync_create(&client_, serverURI.c_str(), clientId.c_str(), 
                             MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cout << "[MQTT] Failed to create client, error code: " << rc << std::endl;
        return false;
    }
    
    MQTTAsync_setCallbacks(client_, this, connectionLost, messageArrived, nullptr);
    
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    
    conn_opts.keepAliveInterval = kKeepAliveIntervalSeconds;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = kConnectionTimeoutSeconds;
    conn_opts.retryInterval = 5;        // 5 seconds between retries
    conn_opts.onSuccess = onConnected;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = this;
    conn_opts.username = username.c_str();
    conn_opts.ssl = &ssl_opts;
    
    // Configure X.509 client certificate authentication
    ssl_opts.keyStore = tlsConfig.certPath.c_str();        // Client certificate (.pem)
    ssl_opts.privateKey = tlsConfig.keyPath.c_str();       // Private key (.pem) 
    ssl_opts.trustStore = tlsConfig.caPath.c_str();        // Root CA certificate (.pem)
    ssl_opts.enableServerCertAuth = tlsConfig.verifyServer ? 1 : 0;
    ssl_opts.verify = tlsConfig.verifyServer ? 1 : 0;
    
    // Additional SSL settings for Azure DPS
    ssl_opts.enabledCipherSuites = nullptr;  // Use default cipher suites
    ssl_opts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;  // Force TLS 1.2
    
    std::cout << "[MQTT] Attempting connection..." << std::endl;
    rc = MQTTAsync_connect(client_, &conn_opts);
    
    if (rc == MQTTASYNC_SUCCESS) {
        std::cout << "[MQTT] Connection attempt initiated successfully" << std::endl;
        return true;
    } else {
        std::cout << "[MQTT] Connection attempt failed, error code: " << rc << std::endl;
        return false;
    }
}

void PahoMqttClient::disconnect() {
    if (client_ && connected_) {
        MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
        disc_opts.onSuccess = onDisconnected;
        disc_opts.context = this;
        
        MQTTAsync_disconnect(client_, &disc_opts);
    }
}

bool PahoMqttClient::isConnected() const {
    return connected_;
}

bool PahoMqttClient::publish(const std::string& topic, const std::string& payload, 
                           int qos, bool retained) {
    if (!connected_) {
        queueMessage(topic, payload, qos, retained);
        return false;
    }
    
    // Azure IoT Hub requires specific topic format for device-to-cloud messages
    std::string iotHubTopic = topic;
    if (topic.find("messages/events") != std::string::npos) {
        // Add content-type and encoding properties for Azure IoT Hub
        iotHubTopic = topic + "$.ct=application%2Fjson&$.ce=utf-8";
    }
    
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    
    pubmsg.payload = const_cast<void*>(static_cast<const void*>(payload.c_str()));
    pubmsg.payloadlen = static_cast<int>(payload.length());
    pubmsg.qos = qos;
    pubmsg.retained = retained ? 1 : 0;
    
    int rc = MQTTAsync_sendMessage(client_, iotHubTopic.c_str(), &pubmsg, &opts);
    return rc == MQTTASYNC_SUCCESS;
}

bool PahoMqttClient::subscribe(const std::string& topic, int qos) {
    if (!connected_) {
        return false;
    }
    
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    
    int rc = MQTTAsync_subscribe(client_, topic.c_str(), qos, &opts);
    return rc == MQTTASYNC_SUCCESS;
}

bool PahoMqttClient::unsubscribe(const std::string& topic) {
    if (!connected_) {
        return false;
    }
    
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    
    int rc = MQTTAsync_unsubscribe(client_, topic.c_str(), &opts);
    return rc == MQTTASYNC_SUCCESS;
}

void PahoMqttClient::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
}

void PahoMqttClient::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
}

void PahoMqttClient::processEvents() {
}

int PahoMqttClient::messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message) {
    (void)topicLen;  // Suppress unused parameter warning - topicLen not needed since topicName is null-terminated
    
    auto* client = static_cast<PahoMqttClient*>(context);
    
    if (client->messageCallback_) {
        MqttMessage msg;
        msg.topic = std::string(topicName);
        msg.payload = std::string(static_cast<char*>(message->payload), message->payloadlen);
        msg.qos = message->qos;
        msg.retained = message->retained != 0;
        
        client->messageCallback_(msg);
    }
    
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void PahoMqttClient::onConnected(void* context, MQTTAsync_successData* response) {
    (void)response;  // Suppress unused parameter warning - response data not needed for success case
    
    auto* client = static_cast<PahoMqttClient*>(context);
    client->connected_ = true;
    
    if (client->connectionCallback_) {
        client->connectionCallback_(true, "Connected successfully");
    }
    
    client->flushOfflineQueue();
}

void PahoMqttClient::onConnectFailure(void* context, MQTTAsync_failureData* response) {
    auto* client = static_cast<PahoMqttClient*>(context);
    client->connected_ = false;
    
    if (client->connectionCallback_) {
        std::string reason = "Connection failed";
        if (response) {
            reason = "CONNACK return code " + std::to_string(response->code);
            if (response->message) {
                reason += " (" + std::string(response->message) + ")";
            }
        }
        client->connectionCallback_(false, reason);
    }
}

void PahoMqttClient::connectionLost(void* context, char* cause) {
    auto* client = static_cast<PahoMqttClient*>(context);
    client->connected_ = false;
    
    if (client->connectionCallback_) {
        std::string reason = cause ? std::string(cause) : "Connection lost";
        client->connectionCallback_(false, reason);
    }
}

void PahoMqttClient::onDisconnected(void* context, MQTTAsync_successData* response) {
    (void)response;  // Suppress unused parameter warning - response data not needed for disconnect
    
    auto* client = static_cast<PahoMqttClient*>(context);
    client->connected_ = false;
}

void PahoMqttClient::flushOfflineQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    while (!offlineQueue_.empty() && connected_) {
        const auto& msg = offlineQueue_.front();
        publish(msg.topic, msg.payload, msg.qos, msg.retained);
        offlineQueue_.pop();
    }
}

void PahoMqttClient::queueMessage(const std::string& topic, const std::string& payload, 
                                int qos, bool retained) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    // Remove oldest message if queue is full (FIFO behavior)
    if (offlineQueue_.size() >= kMaxOfflineQueueSize) {
        offlineQueue_.pop();
    }
    
    MqttMessage msg;
    msg.topic = topic;
    msg.payload = payload;
    msg.qos = qos;
    msg.retained = retained;
    
    offlineQueue_.push(msg);
}

bool PahoMqttClient::validateCertificateFiles(const TlsConfig& tlsConfig) const {
    std::cout << "[MQTT] Validating certificate files..." << std::endl;
    
    std::ifstream certFile(tlsConfig.certPath);
    if (!certFile.good()) {
        std::cout << "[MQTT] ERROR: Certificate file not found: " << tlsConfig.certPath << std::endl;
        return false;
    }
    
    std::ifstream keyFile(tlsConfig.keyPath);
    if (!keyFile.good()) {
        std::cout << "[MQTT] ERROR: Private key file not found: " << tlsConfig.keyPath << std::endl;
        return false;
    }
    
    std::ifstream caFile(tlsConfig.caPath);
    if (!caFile.good()) {
        std::cout << "[MQTT] ERROR: CA file not found: " << tlsConfig.caPath << std::endl;
        return false;
    }
    
    std::cout << "[MQTT] All certificate files validated successfully" << std::endl;
    return true;
}

} // namespace tracker