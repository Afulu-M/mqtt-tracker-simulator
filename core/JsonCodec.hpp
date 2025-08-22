#pragma once

#include "Event.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace tracker {

class JsonCodec {
public:
    static std::string serialize(const Event& event);
    static Event deserialize(const std::string& json);
    
    static nlohmann::json eventToJson(const Event& event);
    static Event jsonToEvent(const nlohmann::json& json);
    
    static nlohmann::json locationToJson(const Location& location);
    static Location jsonToLocation(const nlohmann::json& json);
    
    static nlohmann::json batteryToJson(const BatteryInfo& battery);
    static BatteryInfo jsonToBattery(const nlohmann::json& json);
    
    static nlohmann::json networkToJson(const NetworkInfo& network);
    static NetworkInfo jsonToNetwork(const nlohmann::json& json);
};

} // namespace tracker