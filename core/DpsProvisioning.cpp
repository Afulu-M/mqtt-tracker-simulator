#include "DpsProvisioning.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>

namespace tracker {

DpsProvisioning::DpsProvisioning(std::shared_ptr<IMqttClient> mqttClient)
    : mqttClient_(mqttClient) {
    
    mqttClient_->setConnectionCallback(
        [this](bool connected, const std::string& reason) {
            onDpsConnected(connected, reason);
        }
    );
    
    mqttClient_->setMessageCallback(
        [this](const MqttMessage& message) {
            onDpsMessage(message);
        }
    );
}

void DpsProvisioning::startProvisioning(const DpsConfig& config, ProvisioningCallback callback) {
    config_ = config;
    callback_ = callback;
    state_ = State::ConnectingToDps;
    startTime_ = std::chrono::steady_clock::now();
    lastPoll_ = startTime_;
    
    // Build DPS username with proper API version
    std::string username = config_.idScope + "/registrations/" + config_.registrationId + 
                          "/api-version=" + kDpsApiVersion;
    
    std::cout << "[DPS] Starting provisioning for device: " << config_.registrationId << std::endl;
    std::cout << "[DPS] ID Scope: " << config_.idScope << std::endl;
    std::cout << "[DPS] Endpoint: " << config_.globalEndpoint << ":" << config_.port << std::endl;
    
    bool connected = mqttClient_->connectWithTls(
        config_.globalEndpoint,
        config_.port,
        config_.registrationId,
        username,
        config_.tlsConfig
    );
    
    if (!connected) {
        ProvisioningResult result;
        result.success = false;
        result.errorMessage = "Failed to initiate connection to DPS";
        completeProvisioning(result);
    }
}

void DpsProvisioning::processEvents() {
    mqttClient_->processEvents();
    
    // Check for timeout
    if (isTimedOut()) {
        ProvisioningResult result;
        result.success = false;
        result.errorMessage = "Provisioning timeout";
        completeProvisioning(result);
        return;
    }
    
    // Handle polling in WaitingForAssignment state
    if (state_ == State::WaitingForAssignment) {
        auto now = std::chrono::steady_clock::now();
        if (now - lastPoll_ >= kPollingInterval) {
            pollAssignmentStatus();
            lastPoll_ = now;
        }
    }
}

void DpsProvisioning::cancel() {
    if (state_ != State::Idle && state_ != State::Completed && state_ != State::Failed) {
        mqttClient_->disconnect();
        state_ = State::Failed;
    }
}

void DpsProvisioning::onDpsConnected(bool connected, const std::string& reason) {
    if (state_ != State::ConnectingToDps) {
        return;
    }
    
    if (connected) {
        mqttClient_->subscribe("$dps/registrations/res/#", 1);
        
        std::ostringstream registrationPayload;
        registrationPayload << "{\"registrationId\":\"" << config_.registrationId << "\"}";
        
        std::string topic = buildRegistrationTopic();
        bool published = mqttClient_->publish(topic, registrationPayload.str(), 1);
        
        if (published) {
            state_ = State::SendingRegistration;
            std::cout << "[DPS] Sent registration request for device: " << config_.registrationId << std::endl;
        } else {
            ProvisioningResult result;
            result.success = false;
            result.errorMessage = "Failed to send registration request";
            completeProvisioning(result);
        }
    } else {
        ProvisioningResult result;
        result.success = false;
        result.errorMessage = "Failed to connect to DPS: " + reason;
        completeProvisioning(result);
    }
}

void DpsProvisioning::onDpsMessage(const MqttMessage& message) {
    std::cout << "[DPS] Received message on topic: " << message.topic << std::endl;
    std::cout << "[DPS] Payload: " << message.payload << std::endl;
    
    if (message.topic.find("$dps/registrations/res/") == 0) {
        if (state_ == State::SendingRegistration || state_ == State::WaitingForAssignment) {
            handleRegistrationResponse(message.payload);
        }
    }
}

void DpsProvisioning::handleRegistrationResponse(const std::string& payload) {
    std::string status = extractJsonValue(payload, "status");
    
    if (status == "assigning") {
        operationId_ = extractJsonValue(payload, "operationId");
        state_ = State::WaitingForAssignment;
        std::cout << "[DPS] Device assignment in progress, operation ID: " << operationId_ << std::endl;
    } else if (status == "assigned") {
        handleAssignmentResponse(payload);
    } else {
        ProvisioningResult result;
        result.success = false;
        result.errorMessage = "Registration failed with status: " + status;
        completeProvisioning(result);
    }
}

void DpsProvisioning::handleAssignmentResponse(const std::string& payload) {
    std::string status = extractJsonValue(payload, "status");
    
    if (status == "assigned") {
        std::string assignedHub = extractJsonValue(payload, "assignedHub");
        std::string deviceId = extractJsonValue(payload, "deviceId");
        
        if (!assignedHub.empty() && !deviceId.empty()) {
            ProvisioningResult result;
            result.success = true;
            result.assignedHub = assignedHub;
            result.deviceId = deviceId;
            std::cout << "[DPS] Successfully provisioned device " << deviceId << " to hub " << assignedHub << std::endl;
            completeProvisioning(result);
        } else {
            ProvisioningResult result;
            result.success = false;
            result.errorMessage = "Assignment response missing required fields";
            completeProvisioning(result);
        }
    } else if (status == "assigning") {
        // Continue polling - will be handled in processEvents()
    } else {
        ProvisioningResult result;
        result.success = false;
        result.errorMessage = "Assignment failed with status: " + status;
        completeProvisioning(result);
    }
}

void DpsProvisioning::pollAssignmentStatus() {
    if (state_ != State::WaitingForAssignment || operationId_.empty()) {
        return;
    }
    
    std::string topic = buildPollingTopic();
    mqttClient_->publish(topic, "", 1);
}

void DpsProvisioning::completeProvisioning(const ProvisioningResult& result) {
    state_ = result.success ? State::Completed : State::Failed;
    mqttClient_->disconnect();
    
    if (callback_) {
        callback_(result);
    }
}

std::string DpsProvisioning::extractJsonValue(const std::string& json, const std::string& key) const {
    std::string searchKey = "\"" + key + "\":\"";
    size_t start = json.find(searchKey);
    if (start == std::string::npos) {
        return "";
    }
    
    start += searchKey.length();
    size_t end = json.find("\"", start);
    if (end == std::string::npos) {
        return "";
    }
    
    return json.substr(start, end - start);
}

bool DpsProvisioning::isTimedOut() const {
    if (state_ == State::Idle || state_ == State::Completed || state_ == State::Failed) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    return (now - startTime_) > config_.timeout;
}

std::string DpsProvisioning::buildRegistrationTopic() const {
    return "$dps/registrations/PUT/iotdps-register/?$rid=1";
}

std::string DpsProvisioning::buildPollingTopic() const {
    return "$dps/registrations/GET/iotdps-get-operationstatus/?$rid=2&operationId=" + operationId_;
}

} // namespace tracker