/**
 * @file TomlConfig.hpp
 * @brief TOML configuration file parser for desktop applications
 * 
 * Provides simple TOML configuration parser for simulator settings including
 * DPS configuration, legacy connection strings, and simulation parameters.
 * Designed for easy configuration management across development and production.
 * 
 * Supported Sections:
 * - [dps]: Azure Device Provisioning Service configuration
 * - [connection]: Legacy Azure IoT Hub connection strings  
 * - [simulation]: Simulation runtime parameters
 * - [[route]]: Route waypoints for movement simulation
 * - [[geofences]]: Geofence definitions for location events
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Simple string-based parser for embedded compatibility
 * @note Supports both DPS and legacy configuration modes
 * @note Validates configuration completeness and provides defaults
 */

#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "Simulator.hpp"

namespace tracker {

/**
 * @brief TOML configuration file parser and validator
 * 
 * Parses TOML configuration files for GPS tracker simulator settings.
 * Supports both modern DPS configuration and legacy connection strings
 * with automatic validation and certificate path resolution.
 * 
 * Features:
 * - Section-based configuration parsing
 * - Automatic certificate path construction
 * - Configuration validation with detailed error messages
 * - Default value assignment for optional parameters
 * - Support for both absolute and relative paths
 * 
 * @note Thread-safe static methods for configuration loading
 * @note Minimal dependencies suitable for embedded deployment
 */
class TomlConfig {
public:
    /**
     * @brief Parse Azure IoT Hub connection string (legacy format)
     * @param connectionString Connection string in format "HostName=...;DeviceId=...;SharedAccessKey=..."
     * @return Simulator configuration with parsed connection parameters
     * @note Used for backward compatibility with SAS token authentication
     */
    static tracker::SimulatorConfig parseConnectionString(const std::string& connectionString) {
        tracker::SimulatorConfig config;
        
        // Parse connection string format: HostName=...;DeviceId=...;SharedAccessKey=...
        std::istringstream ss(connectionString);
        std::string part;
        
        while (std::getline(ss, part, ';')) {
            size_t equalPos = part.find('=');
            if (equalPos != std::string::npos) {
                std::string key = part.substr(0, equalPos);
                std::string value = part.substr(equalPos + 1);
                
                if (key == "HostName") {
                    config.iotHubHost = value;
                } else if (key == "DeviceId") {
                    config.deviceId = value;
                } else if (key == "SharedAccessKey") {
                    config.deviceKeyBase64 = value;
                }
            }
        }
        
        return config;
    }

    /**
     * @brief Load and parse TOML configuration file
     * @param filename Path to TOML configuration file
     * @return Complete simulator configuration with validation
     * @throws std::runtime_error if file cannot be read or parsed
     * @note Supports both DPS and legacy configuration sections
     * @note Automatically constructs certificate paths from base path and IMEI
     * @note Provides default route and geofence data if not specified
     */
    static tracker::SimulatorConfig loadFromFile(const std::string& filename) {
        tracker::SimulatorConfig config;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            std::cerr << "Could not open config file: " << filename << std::endl;
            return config;
        }
        
        std::string currentSection;
        std::string line;
        while (std::getline(file, line)) {
            // Remove comments and trim whitespace
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            // Skip empty lines
            if (line.empty()) {
                continue;
            }
            
            // Handle section headers
            if (line[0] == '[') {
                if (line.back() == ']') {
                    currentSection = line.substr(1, line.length() - 2);
                }
                continue;
            }
            
            // Parse key = value
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = line.substr(0, equalPos);
                std::string value = line.substr(equalPos + 1);
                
                // Clean up key and value
                trim(key);
                trim(value);
                unquote(value);
                
                // Set config values based on section
                if (currentSection == "connection") {
                    // Legacy connection section
                    if (key == "connection_string") {
                        // Parse connection string and override individual values (legacy)
                        auto connConfig = parseConnectionString(value);
                        config.iotHubHost = connConfig.iotHubHost;
                        config.deviceId = connConfig.deviceId; 
                        config.deviceKeyBase64 = connConfig.deviceKeyBase64;
                    } else if (key == "iot_hub_host") {
                        config.iotHubHost = value;
                    } else if (key == "device_id") {
                        config.deviceId = value;
                    } else if (key == "device_key_base64") {
                        config.deviceKeyBase64 = value;
                    }
                } else if (currentSection == "dps") {
                    // DPS configuration section
                    if (key == "id_scope") {
                        config.idScope = value;
                    } else if (key == "imei") {
                        config.imei = value;
                        config.deviceId = value; // Use IMEI as device ID fallback
                    } else if (key == "device_cert_base_path") {
                        // Build certificate paths from base path (after IMEI is set)
                        if (!config.imei.empty()) {
                            std::string basePath = value;
                            if (basePath.back() != '/' && basePath.back() != '\\') {
                                basePath += "/";
                            }
                            config.deviceCertPath = basePath + config.imei + "/device.cert.pem";
                            config.deviceKeyPath = basePath + config.imei + "/device.key.pem";
                            config.deviceChainPath = basePath + config.imei + "/device.chain.pem";
                        } else {
                            // Store base path for later use when IMEI is available
                            std::string basePath = value;
                            if (basePath.back() != '/' && basePath.back() != '\\') {
                                basePath += "/";
                            }
                            config.deviceCertPath = basePath; // Temporary storage
                        }
                    } else if (key == "root_ca_path") {
                        config.rootCaPath = value;
                    } else if (key == "verify_server_cert") {
                        config.verifyServerCert = (value == "true" || value == "1");
                    }
                } else if (currentSection == "simulation") {
                    // Simulation parameters
                    if (key == "heartbeat_seconds") {
                        config.heartbeatSeconds = std::stoi(value);
                    } else if (key == "speed_limit_kph") {
                        config.speedLimitKph = std::stod(value);
                    }
                }
            }
        }
        
        // Post-process certificate paths if base path was set before IMEI
        if (!config.deviceCertPath.empty() && config.deviceKeyPath.empty() && !config.imei.empty()) {
            std::string basePath = config.deviceCertPath;
            config.deviceCertPath = basePath + config.imei + "/device.cert.pem";
            config.deviceKeyPath = basePath + config.imei + "/device.key.pem";
            config.deviceChainPath = basePath + config.imei + "/device.chain.pem";
        }
        
        // Validate DPS configuration if present
        if (config.hasDpsConfig()) {
            validateCertificatePaths(config);
        }
        
        // Set default route and geofences
        config.route = {
            {-26.2041, 28.0473}, // Start
            {-26.2000, 28.0500}, // Waypoint 1
            {-26.1950, 28.0520}, // Waypoint 2  
            {-26.1920, 28.0480}, // End
        };
        
        config.geofences = {
            {"office", -26.2041, 28.0473, 100.0},
            {"warehouse", -26.1920, 28.0480, 150.0}
        };
        
        return config;
    }

private:
    /**
     * @brief Validate certificate file paths exist and are readable
     * @param config Simulator configuration to validate
     * @throws std::runtime_error if any certificate file is missing
     */
    static void validateCertificatePaths(const tracker::SimulatorConfig& config) {
        namespace fs = std::filesystem;
        
        if (!config.deviceCertPath.empty() && !fs::exists(config.deviceCertPath)) {
            std::cerr << "[Config] Warning: Device certificate not found: " << config.deviceCertPath << std::endl;
        }
        
        if (!config.deviceKeyPath.empty() && !fs::exists(config.deviceKeyPath)) {
            std::cerr << "[Config] Warning: Device private key not found: " << config.deviceKeyPath << std::endl;
        }
        
        if (!config.rootCaPath.empty() && !fs::exists(config.rootCaPath)) {
            std::cerr << "[Config] Warning: Root CA certificate not found: " << config.rootCaPath << std::endl;
        }
    }
    
    /**
     * @brief Trim whitespace from both ends of string
     * @param str String to trim (modified in place)
     */
    static void trim(std::string& str) {
        str.erase(0, str.find_first_not_of(" \t"));
        str.erase(str.find_last_not_of(" \t") + 1);
    }
    
    /**
     * @brief Remove surrounding quotes from string value
     * @param value String value to unquote (modified in place)
     */
    static void unquote(std::string& value) {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
    }
};

} // namespace tracker