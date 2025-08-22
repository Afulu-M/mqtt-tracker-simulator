#include "DpsConnectionManager.hpp"
#include "../net/mqtt/PahoMqttClient.hpp"
#include <iostream>
#include <filesystem>

namespace tracker {

DpsConnectionManager::DpsConnectionManager(std::shared_ptr<IMqttClient> mqttClient) 
    : provisioningClient_(mqttClient) {
    hubClient_ = std::make_shared<PahoMqttClient>();
}

void DpsConnectionManager::connectToIotHub(const DeviceConfig& config, ConnectionCallback callback) {
    if (state_ != ConnectionState::Disconnected) {
        if (callback) {
            callback(false, "Connection already in progress or established");
        }
        return;
    }
    
    if (!validateCertificatePaths(config)) {
        if (callback) {
            callback(false, "Invalid certificate paths");
        }
        return;
    }
    
    config_ = config;
    connectionCallback_ = callback;
    state_ = ConnectionState::Provisioning;
    
    dpsProvisioning_ = std::make_unique<DpsProvisioning>(provisioningClient_);
    
    DpsConfig dpsConfig;
    dpsConfig.idScope = config_.idScope;
    dpsConfig.registrationId = config_.imei;
    dpsConfig.tlsConfig.certPath = config_.deviceCertPath;
    dpsConfig.tlsConfig.keyPath = config_.deviceKeyPath;
    dpsConfig.tlsConfig.caPath = config_.rootCaPath;
    dpsConfig.tlsConfig.verifyServer = config_.verifyServerCert;
    
    std::cout << "[DPS Connection Manager] Starting DPS provisioning for device: " << config_.imei << std::endl;
    
    dpsProvisioning_->startProvisioning(dpsConfig, 
        [this](const ProvisioningResult& result) {
            onProvisioningComplete(result);
        }
    );
}

void DpsConnectionManager::disconnect() {
    if (dpsProvisioning_) {
        dpsProvisioning_->cancel();
    }
    
    if (hubClient_ && hubClient_->isConnected()) {
        hubClient_->disconnect();
    }
    
    state_ = ConnectionState::Disconnected;
    assignedHub_.clear();
    deviceId_.clear();
}

bool DpsConnectionManager::isConnected() const {
    return state_ == ConnectionState::Connected && hubClient_ && hubClient_->isConnected();
}

bool DpsConnectionManager::publish(const std::string& topic, const std::string& payload, int qos, bool retained) {
    if (!isConnected()) {
        return false;
    }
    
    std::string fullTopic = topic;
    if (topic.find("devices/") != 0) {
        fullTopic = buildDeviceTelemetryTopic() + topic;
    }
    
    return hubClient_->publish(fullTopic, payload, qos, retained);
}

bool DpsConnectionManager::subscribe(const std::string& topic, int qos) {
    if (!isConnected()) {
        return false;
    }
    
    std::string fullTopic = topic;
    if (topic.find("devices/") != 0) {
        fullTopic = buildDeviceCommandTopic();
    }
    
    return hubClient_->subscribe(fullTopic, qos);
}

bool DpsConnectionManager::unsubscribe(const std::string& topic) {
    if (!isConnected()) {
        return false;
    }
    
    return hubClient_->unsubscribe(topic);
}

void DpsConnectionManager::setMessageCallback(MessageCallback callback) {
    messageCallback_ = callback;
    if (hubClient_) {
        hubClient_->setMessageCallback([this](const MqttMessage& message) {
            onHubMessage(message);
        });
    }
}

void DpsConnectionManager::processEvents() {
    if (dpsProvisioning_ && state_ == ConnectionState::Provisioning) {
        dpsProvisioning_->processEvents();
    } else if (hubClient_ && (state_ == ConnectionState::ConnectingToHub || state_ == ConnectionState::Connected)) {
        hubClient_->processEvents();
    }
}

void DpsConnectionManager::onProvisioningComplete(const ProvisioningResult& result) {
    if (result.success) {
        assignedHub_ = result.assignedHub;
        deviceId_ = result.deviceId;
        
        std::cout << "[DPS Connection Manager] Provisioning successful. Connecting to IoT Hub: " << assignedHub_ << std::endl;
        
        state_ = ConnectionState::ConnectingToHub;
        
        hubClient_->setConnectionCallback([this](bool connected, const std::string& reason) {
            onHubConnected(connected, reason);
        });
        
        if (messageCallback_) {
            hubClient_->setMessageCallback([this](const MqttMessage& message) {
                onHubMessage(message);
            });
        }
        
        std::string username = assignedHub_ + "/" + deviceId_ + "/?api-version=2021-04-12";
        
        TlsConfig tlsConfig;
        tlsConfig.certPath = config_.deviceCertPath;
        tlsConfig.keyPath = config_.deviceKeyPath;
        tlsConfig.caPath = config_.rootCaPath;
        tlsConfig.verifyServer = config_.verifyServerCert;
        
        bool connectionStarted = hubClient_->connectWithTls(assignedHub_, 8883, deviceId_, username, tlsConfig);
        
        if (!connectionStarted) {
            state_ = ConnectionState::Failed;
            if (connectionCallback_) {
                connectionCallback_(false, "Failed to initiate connection to IoT Hub");
            }
        }
    } else {
        state_ = ConnectionState::Failed;
        if (connectionCallback_) {
            connectionCallback_(false, "DPS provisioning failed: " + result.errorMessage);
        }
    }
    
    dpsProvisioning_.reset();
}

void DpsConnectionManager::onHubConnected(bool connected, const std::string& reason) {
    if (connected) {
        state_ = ConnectionState::Connected;
        std::cout << "[DPS Connection Manager] Successfully connected to IoT Hub: " << assignedHub_ << std::endl;
        
        hubClient_->subscribe(buildDeviceCommandTopic(), 1);
        
        if (connectionCallback_) {
            connectionCallback_(true, "Connected to IoT Hub via DPS");
        }
    } else {
        state_ = ConnectionState::Failed;
        if (connectionCallback_) {
            connectionCallback_(false, "Failed to connect to IoT Hub: " + reason);
        }
    }
}

void DpsConnectionManager::onHubMessage(const MqttMessage& message) {
    if (messageCallback_) {
        messageCallback_(message);
    }
}

bool DpsConnectionManager::validateCertificatePaths(const DeviceConfig& config) const {
    namespace fs = std::filesystem;
    
    if (!fs::exists(config.deviceCertPath)) {
        std::cout << "[Error] Device certificate not found: " << config.deviceCertPath << std::endl;
        return false;
    }
    
    if (!fs::exists(config.deviceKeyPath)) {
        std::cout << "[Error] Device private key not found: " << config.deviceKeyPath << std::endl;
        return false;
    }
    
    if (!fs::exists(config.rootCaPath)) {
        std::cout << "[Error] Root CA certificate not found: " << config.rootCaPath << std::endl;
        return false;
    }
    
    return true;
}

std::string DpsConnectionManager::buildDeviceTelemetryTopic() const {
    return "devices/" + deviceId_ + "/messages/events/";
}

std::string DpsConnectionManager::buildDeviceCommandTopic() const {
    return "devices/" + deviceId_ + "/messages/devicebound/#";
}

} // namespace tracker