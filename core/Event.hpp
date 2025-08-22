#pragma once

#include <string>
#include <optional>
#include <unordered_map>

namespace tracker {

enum class EventType {
    Heartbeat,
    IgnitionOn,
    IgnitionOff,
    MotionStart,
    MotionStop,
    GeofenceEnter,
    GeofenceExit,
    SpeedOverLimit,
    LowBattery
};

struct Location {
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    double accuracy = 0.0;
};

struct BatteryInfo {
    double percentage = 100.0;
    double voltage = 4.0;
};

struct NetworkInfo {
    int rssi = -70;
    std::string rat = "LTE";
};

struct Event {
    std::string deviceId;
    std::string timestamp;
    EventType eventType;
    uint64_t sequence = 0;
    
    Location location;
    double speedKph = 0.0;
    double heading = 0.0;
    BatteryInfo battery;
    NetworkInfo network;
    
    std::unordered_map<std::string, std::string> extras;
};

std::string eventTypeToString(EventType type);
EventType stringToEventType(const std::string& str);

} // namespace tracker