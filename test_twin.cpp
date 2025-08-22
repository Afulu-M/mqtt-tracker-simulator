/**
 * @file test_twin.cpp
 * @brief Simple test program to verify Device Twin functionality
 * 
 * Tests TwinHandler creation, initialization, and basic operations without
 * requiring actual Azure IoT Hub connectivity. Used to validate implementation.
 */

#include "TwinHandler.hpp"
#include "PahoMqttClient.hpp"
#include <iostream>
#include <memory>

using namespace tracker;

int main() {
    std::cout << "=== Device Twin Handler Test ===" << std::endl;
    
    try {
        // Create mock MQTT client (won't actually connect)
        auto mqttClient = std::make_shared<PahoMqttClient>();
        
        // Create TwinHandler
        std::cout << "Creating TwinHandler..." << std::endl;
        TwinHandler twinHandler(mqttClient, "test-device-123");
        
        std::cout << "TwinHandler created successfully" << std::endl;
        std::cout << "Initialized: " << (twinHandler.isInitialized() ? "YES" : "NO") << std::endl;
        std::cout << "Config Version: " << twinHandler.getConfigVersion() << std::endl;
        
        // Test callback setup
        std::cout << "\nSetting up callbacks..." << std::endl;
        twinHandler.setConfigUpdateCallback([](const TwinUpdateResult& result, const nlohmann::json& config) {
            std::cout << "Config update callback triggered" << std::endl;
            std::cout << "  Status: " << static_cast<int>(result.status) << std::endl;
            std::cout << "  Version: " << result.configVersion << std::endl;
        });
        
        twinHandler.setTwinResponseCallback([](TwinStatus status, const std::string& message) {
            std::cout << "Twin response callback triggered" << std::endl;
            std::cout << "  Status: " << static_cast<int>(status) << std::endl;
            std::cout << "  Message: " << message << std::endl;
        });
        
        std::cout << "Callbacks set successfully" << std::endl;
        
        // Test JSON creation for reported properties
        std::cout << "\nTesting JSON operations..." << std::endl;
        nlohmann::json testReported = {
            {"config", {
                {"applied_at", "2025-08-21T14:30:15Z"},
                {"status", "ok"},
                {"config_version", "1"}
            }}
        };
        
        std::cout << "Test reported properties JSON:" << std::endl;
        std::cout << testReported.dump(2) << std::endl;
        
        std::cout << "\n=== Test Completed Successfully ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}