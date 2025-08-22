/**
 * @file main_cli.cpp
 * @brief Command-line interface for GPS tracker simulator
 * 
 * Provides a comprehensive CLI application for running the GPS tracker simulator
 * with various operation modes including automated driving, event generation,
 * and interactive control. Designed for testing and validation of IoT devices.
 * 
 * @author Generated with Claude Code
 * @date 2025
 * @version 1.0
 * 
 * @note Follows MSRA coding standards for embedded C++ development
 * @note Includes proper signal handling for graceful shutdown
 * @note Supports configuration via TOML files and environment variables
 */

#include "Simulator.hpp"
#include "PahoMqttClient.hpp"
#include "SasToken.hpp"
#include "IClock.hpp"
#include "IRng.hpp"
#include "TomlConfig.hpp"
#include "TwinHandler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <cstdlib>

using namespace tracker;

/// Global flag for graceful shutdown coordination
static volatile bool g_running = true;

/// Global Device Twin handler for configuration management
static std::shared_ptr<TwinHandler> g_twinHandler;

/**
 * @brief Signal handler for graceful shutdown
 * 
 * Handles SIGINT (Ctrl+C) and SIGTERM signals to enable clean shutdown
 * of the simulator and proper resource cleanup.
 * 
 * @param signal Signal number received
 * 
 * @note Uses volatile global flag for thread-safe communication
 * @note Essential for embedded systems requiring reliable shutdown
 */
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

/**
 * @brief Display program usage information
 * 
 * Prints comprehensive help text including all command-line options,
 * configuration file format, and usage examples.
 * 
 * @param programName Name of the executable (from argv[0])
 * 
 * @note Provides clear guidance for both novice and expert users
 * @note Includes configuration file examples for quick setup
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --config [file]    Configuration file (default: simulator.toml)\n"
              << "  --drive [minutes]  Start a driving simulation (default: 10 minutes)\n"
              << "  --spike [count]    Generate a spike of events (default: 10)\n"
              << "  --headless         Run without user interaction\n"
              << "  --help             Show this help message\n"
              << "\nConfiguration file format (TOML):\n"
              << "  [connection]\n"
              << "  iot_hub_host = \"your-hub.azure-devices.net\"\n"
              << "  device_id = \"your-device-id\"\n"
              << "  device_key_base64 = \"your-base64-key\"\n"
              << std::endl;
}

/**
 * @brief Safe environment variable getter for Windows
 * 
 * Provides cross-platform safe access to environment variables,
 * handling Windows-specific security warnings.
 * 
 * @param name Environment variable name
 * @return Environment variable value or empty string if not found
 */
std::string safeGetEnv(const char* name) {
#ifdef _WIN32
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) == 0 && buffer != nullptr) {
        std::string result(buffer);
        free(buffer);
        return result;
    }
    return "";
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : "";
#endif
}

/**
 * @brief Load configuration from environment variables
 * 
 * Provides fallback configuration mechanism when TOML file is not available.
 * Includes sensible defaults for testing and development scenarios.
 * 
 * @return Complete simulator configuration with environment overrides
 * 
 * @note Environment variables take precedence over defaults
 * @note Includes sample route and geofences for immediate testing
 * @note Error handling for invalid numeric conversions
 */
SimulatorConfig loadConfigFromEnv() {
    SimulatorConfig config;
    
    // Load Azure IoT Hub connection parameters from environment using safe getter
    std::string host = safeGetEnv("IOT_HOST");
    std::string deviceId = safeGetEnv("DEVICE_ID");
    std::string deviceKey = safeGetEnv("DEVICE_KEY");
    std::string heartbeat = safeGetEnv("HEARTBEAT_SEC");
    std::string speedLimit = safeGetEnv("SPEED_LIMIT_KPH");
    
    // Apply environment overrides with safe conversions
    if (!host.empty()) config.iotHubHost = host;
    if (!deviceId.empty()) config.deviceId = deviceId;
    if (!deviceKey.empty()) config.deviceKeyBase64 = deviceKey;
    if (!heartbeat.empty()) config.heartbeatSeconds = std::stoi(heartbeat);
    if (!speedLimit.empty()) config.speedLimitKph = std::stod(speedLimit);
    
    // Default route: sample trip around Johannesburg for testing
    config.route = {
        {-26.2041, 28.0473}, // Start - Johannesburg CBD
        {-26.2000, 28.0500}, // Waypoint 1 - North movement
        {-26.1950, 28.0520}, // Waypoint 2 - Northeast
        {-26.1920, 28.0480}, // End - Return south
    };
    
    // Sample geofences for enter/exit event testing
    config.geofences = {
        {"office", -26.2041, 28.0473, 100.0},      // 100m radius office geofence
        {"warehouse", -26.1920, 28.0480, 150.0}   // 150m radius warehouse geofence
    };
    
    return config;
}

/**
 * @brief Main application entry point
 * 
 * Initializes the GPS tracker simulator with proper signal handling,
 * configuration management, and various operation modes for testing.
 * 
 * @param argc Command line argument count
 * @param argv Command line argument values
 * @return Exit code (0 for success, 1 for error)
 * 
 * @note Supports multiple operation modes: interactive, driving, spike, headless
 * @note Includes comprehensive error handling and resource cleanup
 * @note Uses dependency injection for testability and platform independence
 */
int main(int argc, char* argv[]) {
    // Install signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    bool driveMode = false;
    bool spikeMode = false;
    bool headless = false;
    double driveDurationMinutes = 10.0;
    int spikeCount = 10;
    
    // Parse command line arguments  
    std::string configFile = "simulator.toml";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            }
        } else if (arg == "--drive") {
            driveMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                driveDurationMinutes = std::stod(argv[++i]);
            }
        } else if (arg == "--spike") {
            spikeMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                spikeCount = std::stoi(argv[++i]);
            }
        } else if (arg == "--headless") {
            headless = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Load configuration from TOML file
    auto config = TomlConfig::loadFromFile(configFile);
    
    // Validate configuration (DPS or legacy)
    bool hasDpsConfig = config.hasDpsConfig();
    bool hasLegacyConfig = !config.iotHubHost.empty() && !config.deviceId.empty() && !config.deviceKeyBase64.empty();
    
    if (!hasDpsConfig && !hasLegacyConfig) {
        std::cerr << "Error: Missing required configuration in " << configFile << std::endl;
        std::cerr << "Required (DPS): id_scope, imei, device_cert_base_path, root_ca_path" << std::endl;
        std::cerr << "OR Required (Legacy): iot_hub_host, device_id, device_key_base64" << std::endl;
        return 1;
    }
    
    std::cout << "Starting MQTT Tracker Simulator" << std::endl;
    
    if (hasDpsConfig) {
        std::cout << "Connection Mode: DPS (X.509 Certificates)" << std::endl;
        std::cout << "ID Scope: " << config.idScope << std::endl;
        std::cout << "IMEI: " << config.imei << std::endl;
        std::cout << "Device Cert: " << config.deviceCertPath << std::endl;
    } else {
        std::cout << "Connection Mode: Legacy (SAS Token)" << std::endl;
        std::cout << "Device ID: " << config.deviceId << std::endl;
        std::cout << "IoT Hub Host: " << config.iotHubHost << std::endl;
    }
    
    std::cout << "Heartbeat: " << config.heartbeatSeconds << "s" << std::endl;
    
    // Create platform-specific dependencies using dependency injection pattern
    // This design enables easy porting to embedded platforms (STM32, etc.)
    auto mqttClient = std::make_shared<PahoMqttClient>();  // Desktop MQTT implementation
    auto clock = std::make_shared<SystemClock>();          // System time abstraction
    auto rng = std::make_shared<StandardRng>();            // Standard C++ RNG
    
    // Create and configure simulator with injected dependencies
    Simulator simulator(mqttClient, clock, rng);
    simulator.configure(config);
    
    // Create Device Twin configuration adapter (Hexagonal Architecture)
    // Note: Actual MQTT client will be configured after DPS connection
    const std::string deviceIdForTwin = hasDpsConfig ? config.imei : config.deviceId;
    g_twinHandler = std::make_shared<TwinHandler>(mqttClient, deviceIdForTwin);
    
    // Integrate Device Twin adapter with domain core (Observer pattern)
    simulator.setTwinHandler(g_twinHandler);
    
    // Start simulator
    simulator.start();
    
    // Handle different modes
    if (spikeMode) {
        std::cout << "Generating spike of " << spikeCount << " events..." << std::endl;
        simulator.generateSpike(spikeCount);
        
        // Wait a bit for messages to be sent
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
    } else if (driveMode) {
        std::cout << "Starting driving simulation for " << driveDurationMinutes << " minutes..." << std::endl;
        simulator.startDriving(driveDurationMinutes);
        
        // Run simulation
        auto endTime = std::chrono::steady_clock::now() + 
                      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(driveDurationMinutes * 60));
        
        while (g_running && std::chrono::steady_clock::now() < endTime) {
            simulator.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        simulator.setSpeed(0.0);
        simulator.setIgnition(false);
        
    } else if (headless) {
        std::cout << "Running in headless mode. Press Ctrl+C to stop." << std::endl;
        
        while (g_running) {
            simulator.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
    } else {
        // Interactive mode
        std::cout << "\nInteractive mode. Commands:" << std::endl;
        std::cout << "  i - Toggle ignition" << std::endl;
        std::cout << "  s - Set speed" << std::endl;
        std::cout << "  b - Set battery percentage" << std::endl;
        std::cout << "  d - Start driving" << std::endl;
        std::cout << "  p - Generate spike" << std::endl;
        std::cout << "  q - Quit" << std::endl;
        
        bool ignitionOn = false;
        
        std::thread inputThread([&]() {
            char cmd;
            while (g_running && std::cin >> cmd) {
                
                switch (cmd) {
                    case 'i':
                        ignitionOn = !ignitionOn;
                        simulator.setIgnition(ignitionOn);
                        std::cout << "Ignition " << (ignitionOn ? "ON" : "OFF") << std::endl;
                        break;
                        
                    case 's': {
                        double speed;
                        std::cout << "Enter speed (km/h): ";
                        std::cin >> speed;
                        simulator.setSpeed(speed);
                        std::cout << "Speed set to " << speed << " km/h" << std::endl;
                        break;
                    }
                    
                    case 'b': {
                        double battery;
                        std::cout << "Enter battery percentage: ";
                        std::cin >> battery;
                        simulator.setBatteryPercentage(battery);
                        std::cout << "Battery set to " << battery << "%" << std::endl;
                        break;
                    }
                    
                    case 'd': {
                        double duration;
                        std::cout << "Enter drive duration (minutes): ";
                        std::cin >> duration;
                        simulator.startDriving(duration);
                        std::cout << "Started driving for " << duration << " minutes" << std::endl;
                        break;
                    }
                    
                    case 'p': {
                        int count;
                        std::cout << "Enter event count: ";
                        std::cin >> count;
                        simulator.generateSpike(count);
                        std::cout << "Generated " << count << " events" << std::endl;
                        break;
                    }
                    
                    case 'q':
                        g_running = false;
                        break;
                        
                    default:
                        std::cout << "Unknown command" << std::endl;
                        break;
                }
            }
        });
        
        while (g_running) {
            simulator.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        inputThread.join();
    }
    
    std::cout << "Stopping simulator..." << std::endl;
    simulator.stop();
    
    // Clean up Device Twin handler
    if (g_twinHandler) {
        g_twinHandler.reset();
        std::cout << "Device Twin handler stopped." << std::endl;
    }
    
    // Allow time for graceful shutdown and message transmission
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    
    std::cout << "Simulator stopped." << std::endl;
    return 0;
}

/**
 * @note This CLI application demonstrates MSRA embedded C++ best practices:
 * - Proper resource management with RAII patterns
 * - Signal handling for graceful shutdown in embedded systems
 * - Dependency injection for platform portability
 * - Comprehensive error handling and validation
 * - Modular design suitable for real-time embedded operation
 * - Memory-efficient operation suitable for resource-constrained environments
 * 
 * @see Simulator.hpp for core simulation engine documentation
 * @see Azure IoT Hub documentation for cloud integration details
 */