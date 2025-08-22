# GPS Tracker Simulator - CMake Build Configuration
# Designed for embedded portability with desktop development support
# Follows MSRA coding standards for C++ embedded development

cmake_minimum_required(VERSION 3.20)
project(mqtt_tracker_simulator 
    VERSION 1.0.0 
    DESCRIPTION "GPS Tracker Simulator for Azure IoT Hub" 
    LANGUAGES CXX
)

# C++20 standard required for modern embedded-friendly features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable compiler-specific extensions for portability

# Build options for different deployment scenarios
option(BUILD_QT "Build Qt GUI application" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(EMBEDDED_BUILD "Optimize for embedded targets" OFF)

# Compiler-specific optimizations for embedded development
if(EMBEDDED_BUILD)
    # Optimize for size and performance on resource-constrained systems
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()

# Find packages
find_package(OpenSSL REQUIRED)
find_package(eclipse-paho-mqtt-c REQUIRED)

# nlohmann/json
find_package(nlohmann_json REQUIRED)

if(BUILD_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets WebEngineWidgets)
    qt_standard_project_setup()
endif()

# === Core Simulation Library ===
# Platform-independent core logic suitable for embedded porting
# No OS/GUI dependencies for maximum portability
add_library(tracker_core STATIC
    # Event system and data structures
    core/Event.hpp
    core/Event.cpp
    core/StateMachine.hpp  
    core/StateMachine.cpp
    
    # Geographic and sensor simulation
    core/Geo.hpp
    core/Geo.cpp
    core/Battery.hpp
    core/Battery.cpp
    
    # Serialization and communication interfaces
    core/JsonCodec.hpp
    core/JsonCodec.cpp
    core/IMqttClient.hpp
    core/IClock.hpp
    core/IClock.cpp
    core/IRng.hpp
    
    # Device Twin configuration management
    core/TwinHandler.hpp
    core/TwinHandler.cpp
    
    # Main simulation engine with Azure IoT Hub connectivity
    core/Simulator.hpp
    core/Simulator.cpp
    
    # Azure Device Provisioning Service (DPS) integration
    # Enables X.509 certificate-based device authentication and automatic hub assignment
    # Designed for embedded portability with minimal dependencies
    core/DpsProvisioning.hpp
    core/DpsProvisioning.cpp
    core/DpsConnectionManager.hpp
    core/DpsConnectionManager.cpp
)

# Public interface for dependent libraries
target_include_directories(tracker_core PUBLIC core)
target_link_libraries(tracker_core PUBLIC nlohmann_json::nlohmann_json)

# Embedded-friendly compiler settings
target_compile_features(tracker_core PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(tracker_core PRIVATE /W4 /WX)  # High warning level, warnings as errors
else()
    target_compile_options(tracker_core PRIVATE -Wall -Wextra -Werror)  # Comprehensive warnings
endif()

# === Cryptographic Library ===
# Azure IoT Hub authentication using SAS tokens
# Uses OpenSSL (desktop) - can be replaced with mbedTLS for embedded targets
add_library(tracker_crypto STATIC
    crypto/SasToken.hpp
    crypto/SasToken.cpp
)

# Crypto library configuration
target_include_directories(tracker_crypto PUBLIC crypto)
target_link_libraries(tracker_crypto PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Embedded-friendly compiler settings for crypto module
target_compile_features(tracker_crypto PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(tracker_crypto PRIVATE /W4 /WX)
else()
    target_compile_options(tracker_crypto PRIVATE -Wall -Wextra -Werror)
endif()

# Link core simulation with cryptographic services
target_link_libraries(tracker_core PRIVATE tracker_crypto)

# === MQTT Networking Library ===
# Desktop implementation using Paho MQTT C library
# For embedded targets, replace with coreMQTT or Paho Embedded C
add_library(tracker_mqtt STATIC
    net/mqtt/PahoMqttClient.hpp
    net/mqtt/PahoMqttClient.cpp
)

# MQTT library configuration
target_include_directories(tracker_mqtt PUBLIC net/mqtt)
target_link_libraries(tracker_mqtt 
    PUBLIC tracker_core
    PRIVATE eclipse-paho-mqtt-c::paho-mqtt3as  # Desktop MQTT implementation
)

# Embedded-friendly compiler settings for networking
target_compile_features(tracker_mqtt PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(tracker_mqtt PRIVATE /W4 /WX)
else()
    target_compile_options(tracker_mqtt PRIVATE -Wall -Wextra -Werror)
endif()

# === Command Line Interface ===
# Primary application for desktop testing and validation
add_executable(sim-cli
    platform/desktop/main_cli.cpp
    platform/desktop/TomlConfig.hpp  # TOML configuration parser with DPS and legacy support
)

# CLI application dependencies
target_link_libraries(sim-cli 
    PRIVATE 
    tracker_core     # Core simulation engine
    tracker_crypto   # Azure IoT Hub authentication
    tracker_mqtt     # MQTT communication
)

# Production-ready compiler settings
target_compile_features(sim-cli PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(sim-cli PRIVATE /W4 /WX)
else()
    target_compile_options(sim-cli PRIVATE -Wall -Wextra -Werror)
endif()

# Qt GUI application (optional)
if(BUILD_QT)
    qt_add_executable(sim-qt
        ui/qt/main.cpp
        ui/qt/MainWindow.hpp
        ui/qt/MainWindow.cpp
    )
    
    target_link_libraries(sim-qt 
        PRIVATE 
        tracker_core 
        tracker_crypto 
        tracker_mqtt
        Qt6::Core 
        Qt6::Widgets 
        Qt6::WebEngineWidgets
    )
endif()

# === Unit Tests ===
# Critical for embedded development - ensures reliability
if(BUILD_TESTS)
    add_executable(sim-tests
        tests/test_sas_token.cpp
    )

    # Test executable configuration
    target_link_libraries(sim-tests 
        PRIVATE 
        tracker_core 
        tracker_crypto
    )
    
    # Enable testing framework
    enable_testing()
    add_test(NAME sas_token_tests COMMAND sim-tests)
    
    # Embedded-friendly test settings
    target_compile_features(sim-tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(sim-tests PRIVATE /W4)
    else()
        target_compile_options(sim-tests PRIVATE -Wall -Wextra)
    endif()
endif()


# === Build Configuration Summary ===
# Features:
# - Core simulation library with Azure DPS X.509 certificate authentication
# - MQTT over TLS connectivity with certificate chain validation
# - Platform-independent design for STM32H/FreeRTOS portability
# - MSRA C++ coding standards compliance with comprehensive error checking
# - Production-ready error handling and logging throughout