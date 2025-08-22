#include "Event.hpp"
#include <unordered_map>

namespace tracker {

std::string eventTypeToString(EventType type) {
    static const std::unordered_map<EventType, std::string> typeMap = {
        {EventType::Heartbeat, "heartbeat"},
        {EventType::IgnitionOn, "ignition_on"},
        {EventType::IgnitionOff, "ignition_off"},
        {EventType::MotionStart, "motion_start"},
        {EventType::MotionStop, "motion_stop"},
        {EventType::GeofenceEnter, "geofence_enter"},
        {EventType::GeofenceExit, "geofence_exit"},
        {EventType::SpeedOverLimit, "speed_over_limit"},
        {EventType::LowBattery, "low_battery"}
    };
    
    auto it = typeMap.find(type);
    return (it != typeMap.end()) ? it->second : "unknown";
}

EventType stringToEventType(const std::string& str) {
    static const std::unordered_map<std::string, EventType> stringMap = {
        {"heartbeat", EventType::Heartbeat},
        {"ignition_on", EventType::IgnitionOn},
        {"ignition_off", EventType::IgnitionOff},
        {"motion_start", EventType::MotionStart},
        {"motion_stop", EventType::MotionStop},
        {"geofence_enter", EventType::GeofenceEnter},
        {"geofence_exit", EventType::GeofenceExit},
        {"speed_over_limit", EventType::SpeedOverLimit},
        {"low_battery", EventType::LowBattery}
    };
    
    auto it = stringMap.find(str);
    return (it != stringMap.end()) ? it->second : EventType::Heartbeat;
}

} // namespace tracker