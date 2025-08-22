/**
 * @file TwinHandler.cpp
 * @brief Azure IoT Hub Device Twin adapter implementation
 * 
 * Implements the Device Twin protocol adapter following Hexagonal Architecture
 * principles. Provides Command pattern for configuration updates and Observer
 * pattern for change notifications while maintaining clean separation from
 * domain logic.
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note RAII resource management with bounded memory usage
 * @note Thread-safe with minimal locking for embedded compatibility
 * @note Deterministic error handling following fail-fast/fail-safe principles
 */

#include "TwinHandler.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <regex>

namespace tracker {

TwinHandler::TwinHandler(std::shared_ptr<IMqttClient> mqttClient, const std::string& deviceId)
    : mqttClient_(std::move(mqttClient))
    , deviceId_(deviceId)
{
    if (!mqttClient_) {
        throw std::invalid_argument("TwinHandler: MQTT client cannot be null");
    }
    
    if (deviceId_.empty()) {
        throw std::invalid_argument("TwinHandler: Device ID cannot be empty");
    }
    
    std::cout << "TwinHandler: Initialized for device " << deviceId_ << std::endl;
}

TwinHandler::~TwinHandler() {
    std::cout << "TwinHandler: Shutting down for device " << deviceId_ << std::endl;
}

bool TwinHandler::initializeSubscriptions() {
    if (!mqttClient_->isConnected()) {
        std::cerr << "TwinHandler: Cannot initialize subscriptions - MQTT client not connected" << std::endl;
        return false;
    }
    
    // Subscribe to Device Twin response topics (all status codes)
    const std::string responseTopic = std::string(kTwinResponseTopic) + "#";
    if (!mqttClient_->subscribe(responseTopic, 0)) {
        std::cerr << "TwinHandler: Failed to subscribe to twin response topic: " << responseTopic << std::endl;
        return false;
    }
    
    // Subscribe to desired properties PATCH updates
    const std::string patchTopic = std::string(kTwinPatchTopic) + "#";
    if (!mqttClient_->subscribe(patchTopic, 0)) {
        std::cerr << "TwinHandler: Failed to subscribe to desired patch topic: " << patchTopic << std::endl;
        return false;
    }
    
    std::cout << "TwinHandler: Successfully subscribed to Device Twin topics:" << std::endl;
    std::cout << "  Response: " << responseTopic << std::endl;
    std::cout << "  Desired:  " << patchTopic << std::endl;
    
    initialized_ = true;
    return true;
}

bool TwinHandler::requestFullTwin(const std::string& requestId) {
    if (!initialized_) {
        std::cerr << "TwinHandler: Cannot request twin - subscriptions not initialized" << std::endl;
        return false;
    }
    
    if (!mqttClient_->isConnected()) {
        std::cerr << "TwinHandler: Cannot request twin - MQTT client not connected" << std::endl;
        return false;
    }
    
    // Construct Device Twin GET topic with request ID
    const std::string getTopic = std::string(kTwinGetTopic) + "?$rid=" + requestId;
    
    // Send empty payload for GET request per Azure IoT Hub specification
    if (!mqttClient_->publish(getTopic, "", 0, false)) {
        std::cerr << "TwinHandler: Failed to publish twin GET request to: " << getTopic << std::endl;
        return false;
    }
    
    std::cout << "TwinHandler: Requested full Device Twin with RID=" << requestId << std::endl;
    return true;
}

bool TwinHandler::sendReportedAck(const std::string& requestId, const nlohmann::json& reportedProperties) {
    if (!mqttClient_->isConnected()) {
        std::cerr << "TwinHandler: Cannot send reported ack - MQTT client not connected" << std::endl;
        return false;
    }
    
    try {
        // Construct reported properties PATCH topic with request ID
        const std::string reportedTopic = std::string(kTwinReportedTopic) + "?$rid=" + requestId;
        
        // Serialize JSON payload
        const std::string payload = reportedProperties.dump();
        
        if (!mqttClient_->publish(reportedTopic, payload, 0, false)) {
            std::cerr << "TwinHandler: Failed to publish reported properties to: " << reportedTopic << std::endl;
            return false;
        }
        
        std::cout << "TwinHandler: Sent reported properties acknowledgment with RID=" << requestId << std::endl;
        std::cout << "  Payload: " << payload << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "TwinHandler: Failed to serialize reported properties: " << e.what() << std::endl;
        return false;
    }
}

void TwinHandler::setConfigUpdateCallback(ConfigUpdateCallback callback) {
    configUpdateCallback_ = std::move(callback);
}

void TwinHandler::setTwinResponseCallback(TwinResponseCallback callback) {
    twinResponseCallback_ = std::move(callback);
}

void TwinHandler::handleMqttMessage(const MqttMessage& message) {
    const std::string& topic = message.topic;
    const std::string& payload = message.payload;
    
    // Route message based on topic prefix (Command pattern dispatch)
    if (topic.find(kTwinResponseTopic) == 0) {
        processTwinResponse(topic, payload);
    } else if (topic.find(kTwinPatchTopic) == 0) {
        processDesiredPatch(topic, payload);
    }
    // Ignore non-Device Twin messages (bounded processing)
}

std::string TwinHandler::getConfigVersion() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return currentConfigVersion_;
}

void TwinHandler::processTwinResponse(const std::string& topic, const std::string& payload) {
    // Extract protocol metadata from topic (Strategy pattern for status handling)
    const int statusCode = extractStatusCode(topic);
    const std::string requestId = extractRequestId(topic);
    
    // Handle different status codes with explicit success/error semantics
    if (statusCode == 200) {
        // Success response with content (for GET requests) - process payload
        std::cout << "TwinHandler: Processing Device Twin configuration (RID=" << requestId << ")" << std::endl;
    } else if (statusCode == 204) {
        // Success response without content (for PATCH acknowledgment) - complete
        if (twinResponseCallback_) {
            twinResponseCallback_(TwinStatus::Success, "Configuration acknowledged");
        }
        return; // No payload to process
    } else {
        // Error response - fail fast with explicit error message
        const std::string errorMsg = "Device Twin operation failed: HTTP " + std::to_string(statusCode);
        if (twinResponseCallback_) {
            twinResponseCallback_(TwinStatus::InvalidResponse, errorMsg);
        }
        return;
    }
    
    try {
        // Parse Device Twin JSON with deterministic structure handling
        const nlohmann::json twinJson = nlohmann::json::parse(payload);
        
        // Extract desired properties using Strategy pattern for different response formats
        nlohmann::json desired;
        if (twinJson.contains("desired")) {
            desired = twinJson["desired"];  // Direct GET response format
        } else if (twinJson.contains("properties") && twinJson["properties"].contains("desired")) {
            desired = twinJson["properties"]["desired"];  // Full twin format
        } else {
            // Fail fast on unexpected format
            const std::string errorMsg = "Device Twin missing desired properties structure";
            if (twinResponseCallback_) {
                twinResponseCallback_(TwinStatus::InvalidResponse, errorMsg);
            }
            return;
        }
        
        if (!desired.empty()) {
            // Apply configuration using Command pattern
            const TwinUpdateResult result = applyDesiredAndWriteFile(desired);
            
            if (result.status == TwinStatus::Success) {
                // Send acknowledgment back to Azure (completed Command)
                const nlohmann::json reportedAck = createDefaultReportedAck(desired, result);
                sendReportedAck("2", reportedAck);
            }
            
            // Notify observers of configuration change (Observer pattern)
            if (configUpdateCallback_) {
                configUpdateCallback_(result, desired);
            }
            
            if (twinResponseCallback_) {
                twinResponseCallback_(result.status, result.errorMessage);
            }
        }
        
    } catch (const nlohmann::json::parse_error& e) {
        const std::string errorMsg = "Failed to parse twin response JSON: " + std::string(e.what());
        std::cerr << "TwinHandler: " << errorMsg << std::endl;
        
        writeErrorFile(payload, errorMsg);
        
        if (twinResponseCallback_) {
            twinResponseCallback_(TwinStatus::JsonParseError, errorMsg);
        }
    } catch (const std::exception& e) {
        const std::string errorMsg = "Unexpected error processing twin response: " + std::string(e.what());
        std::cerr << "TwinHandler: " << errorMsg << std::endl;
        
        if (twinResponseCallback_) {
            twinResponseCallback_(TwinStatus::InvalidResponse, errorMsg);
        }
    }
}

void TwinHandler::processDesiredPatch(const std::string& topic, const std::string& payload) {
    (void)topic; // Topic parsing not required for PATCH processing
    try {
        // Parse incremental desired properties update (Command pattern)
        const nlohmann::json desiredPatch = nlohmann::json::parse(payload);
        
        // Apply incremental configuration update
        const TwinUpdateResult result = applyDesiredAndWriteFile(desiredPatch);
        
        if (result.status == TwinStatus::Success) {
            // Acknowledge successful PATCH application
            const nlohmann::json reportedAck = createDefaultReportedAck(desiredPatch, result);
            sendReportedAck("3", reportedAck);
        }
        
        // Notify observers of configuration change (Observer pattern)
        if (configUpdateCallback_) {
            configUpdateCallback_(result, desiredPatch);
        }
        
    } catch (const nlohmann::json::parse_error& e) {
        // Fail fast on invalid JSON (embedded safety)
        const std::string errorMsg = "Invalid JSON in desired properties PATCH: " + std::string(e.what());
        writeErrorFile(payload, errorMsg);
    } catch (const std::exception& e) {
        // Fail safe on unexpected errors
        const std::string errorMsg = "Error processing desired PATCH: " + std::string(e.what());
        writeErrorFile(payload, errorMsg);
    }
}

TwinUpdateResult TwinHandler::applyDesiredAndWriteFile(const nlohmann::json& desired) {
    TwinUpdateResult result;
    result.status = TwinStatus::Success;
    result.appliedAt = getCurrentTimestamp();
    
    try {
        // Validate desired properties structure
        if (!validateDesiredStructure(desired)) {
            std::cout << "TwinHandler: Warning - Desired properties have non-standard structure, applying anyway" << std::endl;
        }
        
        // Extract configuration version if present
        if (desired.contains("$version")) {
            result.configVersion = std::to_string(desired["$version"].get<int>());
        } else if (desired.contains("config") && desired["config"].contains("config_version")) {
            result.configVersion = std::to_string(desired["config"]["config_version"].get<int>());
        } else {
            result.configVersion = "unknown";
        }
        
        // Check if configuration has actually changed
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            if (currentConfigVersion_ != result.configVersion) {
                result.hasChanges = true;
                currentConfigVersion_ = result.configVersion;
            }
        }
        
        // Create clean configuration JSON (remove Azure metadata)
        nlohmann::json cleanConfig = desired;
        cleanConfig.erase("$version");
        cleanConfig.erase("$metadata");
        
        // Write configuration to local file
        std::ofstream configFile(kConfigFilePath, std::ios::out | std::ios::trunc);
        if (!configFile.is_open()) {
            result.status = TwinStatus::FileWriteError;
            result.errorMessage = "Failed to open configuration file: " + std::string(kConfigFilePath);
            return result;
        }
        
        configFile << cleanConfig.dump(2); // Pretty-print with 2-space indentation
        configFile.close();
        
        std::cout << "Configuration applied: version=" << result.configVersion 
                  << ", changed=" << (result.hasChanges ? "yes" : "no") << std::endl;
        
    } catch (const nlohmann::json::type_error& e) {
        result.status = TwinStatus::JsonParseError;
        result.errorMessage = "JSON type error in desired properties: " + std::string(e.what());
        std::cerr << "TwinHandler: " << result.errorMessage << std::endl;
    } catch (const std::ios_base::failure& e) {
        result.status = TwinStatus::FileWriteError;
        result.errorMessage = "File I/O error writing configuration: " + std::string(e.what());
        std::cerr << "TwinHandler: " << result.errorMessage << std::endl;
    } catch (const std::exception& e) {
        result.status = TwinStatus::InvalidResponse;
        result.errorMessage = "Unexpected error applying desired properties: " + std::string(e.what());
        std::cerr << "TwinHandler: " << result.errorMessage << std::endl;
    }
    
    return result;
}

void TwinHandler::writeErrorFile(const std::string& rawPayload, const std::string& errorMessage) {
    try {
        nlohmann::json errorJson = {
            {"timestamp", getCurrentTimestamp()},
            {"deviceId", deviceId_},
            {"error", errorMessage},
            {"rawPayload", rawPayload}
        };
        
        std::ofstream errorFile(kErrorFilePath, std::ios::out | std::ios::trunc);
        if (errorFile.is_open()) {
            errorFile << errorJson.dump(2);
            errorFile.close();
            
            std::cout << "TwinHandler: Wrote error details to file: " << kErrorFilePath << std::endl;
        } else {
            std::cerr << "TwinHandler: Failed to write error file: " << kErrorFilePath << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "TwinHandler: Failed to create error file: " << e.what() << std::endl;
    }
}

std::string TwinHandler::extractRequestId(const std::string& topic) const {
    // Look for $rid parameter in topic using regex
    const std::regex ridRegex(R"(\$rid=([^&/?]+))");
    std::smatch match;
    
    if (std::regex_search(topic, match, ridRegex) && match.size() > 1) {
        return match[1].str();
    }
    
    return "";
}

int TwinHandler::extractStatusCode(const std::string& topic) const {
    // Extract HTTP status code from twin response topic
    // Format: $iothub/twin/res/{status_code}/?$rid={request_id}
    const std::regex statusRegex(R"(\$iothub/twin/res/(\d{3})/)");
    std::smatch match;
    
    if (std::regex_search(topic, match, statusRegex) && match.size() > 1) {
        try {
            return std::stoi(match[1].str());
        } catch (const std::exception&) {
            return 0;
        }
    }
    
    return 0;
}

std::string TwinHandler::getCurrentTimestamp() const {
    // Generate ISO8601 UTC timestamp
    const auto now = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
#ifdef _WIN32
    struct tm utc_tm;
    if (gmtime_s(&utc_tm, &time_t) == 0) {
        oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    } else {
        oss << "1970-01-01T00:00:00Z"; // fallback timestamp
    }
#else
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
#endif
    return oss.str();
}

nlohmann::json TwinHandler::createDefaultReportedAck(const nlohmann::json& appliedConfig, 
                                                    const TwinUpdateResult& result) const {
    nlohmann::json reportedAck;
    
    // Create acknowledgment based on applied configuration structure
    if (appliedConfig.contains("config")) {
        // Standard config object structure
        nlohmann::json configAck = {
            {"applied_at", result.appliedAt},
            {"status", result.status == TwinStatus::Success ? "ok" : "error"}
        };
        
        if (!result.configVersion.empty() && result.configVersion != "unknown") {
            configAck["config_version"] = result.configVersion;
        }
        
        // Copy specific acknowledged fields from original config
        if (appliedConfig["config"].contains("reporting_interval_sec")) {
            configAck["reporting_interval_sec"] = appliedConfig["config"]["reporting_interval_sec"];
        }
        if (appliedConfig["config"].contains("feature_high_rate")) {
            configAck["feature_high_rate"] = appliedConfig["config"]["feature_high_rate"];
        }
        
        reportedAck["config"] = configAck;
        
    } else {
        // Top-level properties or non-standard structure
        reportedAck = {
            {"applied_at", result.appliedAt},
            {"status", result.status == TwinStatus::Success ? "ok" : "error"},
            {"config_version", result.configVersion}
        };
        
        // Add any top-level acknowledgments for common property groups
        if (appliedConfig.contains("reporting")) {
            reportedAck["reporting_ack"] = {
                {"applied_at", result.appliedAt},
                {"status", "ok"}
            };
        }
        
        if (appliedConfig.contains("modes")) {
            reportedAck["modes_ack"] = {
                {"applied_at", result.appliedAt},
                {"status", "ok"}
            };
        }
        
        if (appliedConfig.contains("ota")) {
            reportedAck["ota_ack"] = {
                {"applied_at", result.appliedAt},
                {"status", "ok"}
            };
        }
    }
    
    // Add error details if configuration application failed
    if (result.status != TwinStatus::Success) {
        reportedAck["error"] = result.errorMessage;
    }
    
    return reportedAck;
}

bool TwinHandler::validateDesiredStructure(const nlohmann::json& desired) const {
    // Check for common Azure IoT Hub desired property structures
    if (desired.is_object()) {
        // Look for known property groups
        const std::vector<std::string> knownKeys = {
            "config", "reporting", "modes", "ota", "telemetry", "device"
        };
        
        for (const auto& key : knownKeys) {
            if (desired.contains(key)) {
                return true; // Found at least one known structure
            }
        }
        
        // Also valid if it has any non-metadata properties
        for (const auto& [key, value] : desired.items()) {
            if (key.front() != '$') { // Not Azure metadata
                return true;
            }
        }
    }
    
    return false; // Empty or invalid structure
}

} // namespace tracker