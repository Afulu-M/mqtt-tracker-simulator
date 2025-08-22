#include "JsonCodec.hpp"

namespace tracker {

std::string JsonCodec::serialize(const Event& event) {
    return eventToJson(event).dump();
}

Event JsonCodec::deserialize(const std::string& json) {
    return jsonToEvent(nlohmann::json::parse(json));
}

nlohmann::json JsonCodec::eventToJson(const Event& event) {
    nlohmann::json j;
    
    j["deviceId"] = event.deviceId;
    j["ts"] = event.timestamp;
    j["eventType"] = eventTypeToString(event.eventType);
    j["seq"] = event.sequence;
    
    j["loc"] = locationToJson(event.location);
    j["speedKph"] = event.speedKph;
    j["heading"] = event.heading;
    j["battery"] = batteryToJson(event.battery);
    j["network"] = networkToJson(event.network);
    
    if (!event.extras.empty()) {
        nlohmann::json extras;
        for (const auto& [key, value] : event.extras) {
            if (value.empty()) {
                extras[key] = nullptr;
            } else {
                extras[key] = value;
            }
        }
        j["extras"] = extras;
    }
    
    return j;
}

Event JsonCodec::jsonToEvent(const nlohmann::json& json) {
    Event event;
    
    event.deviceId = json.value("deviceId", "");
    event.timestamp = json.value("ts", "");
    event.eventType = stringToEventType(json.value("eventType", "heartbeat"));
    event.sequence = json.value("seq", 0ULL);
    
    if (json.contains("loc")) {
        event.location = jsonToLocation(json["loc"]);
    }
    
    event.speedKph = json.value("speedKph", 0.0);
    event.heading = json.value("heading", 0.0);
    
    if (json.contains("battery")) {
        event.battery = jsonToBattery(json["battery"]);
    }
    
    if (json.contains("network")) {
        event.network = jsonToNetwork(json["network"]);
    }
    
    if (json.contains("extras") && json["extras"].is_object()) {
        for (const auto& [key, value] : json["extras"].items()) {
            if (value.is_null()) {
                event.extras[key] = "";
            } else if (value.is_string()) {
                event.extras[key] = value;
            } else {
                event.extras[key] = value.dump();
            }
        }
    }
    
    return event;
}

nlohmann::json JsonCodec::locationToJson(const Location& location) {
    nlohmann::json j;
    j["lat"] = location.lat;
    j["lon"] = location.lon;
    j["alt"] = location.alt;
    j["acc"] = location.accuracy;
    return j;
}

Location JsonCodec::jsonToLocation(const nlohmann::json& json) {
    Location location;
    location.lat = json.value("lat", 0.0);
    location.lon = json.value("lon", 0.0);
    location.alt = json.value("alt", 0.0);
    location.accuracy = json.value("acc", 0.0);
    return location;
}

nlohmann::json JsonCodec::batteryToJson(const BatteryInfo& battery) {
    nlohmann::json j;
    j["pct"] = static_cast<int>(battery.percentage);
    j["voltage"] = battery.voltage;
    return j;
}

BatteryInfo JsonCodec::jsonToBattery(const nlohmann::json& json) {
    BatteryInfo battery;
    battery.percentage = json.value("pct", 100.0);
    battery.voltage = json.value("voltage", 4.0);
    return battery;
}

nlohmann::json JsonCodec::networkToJson(const NetworkInfo& network) {
    nlohmann::json j;
    j["rssi"] = network.rssi;
    j["rat"] = network.rat;
    return j;
}

NetworkInfo JsonCodec::jsonToNetwork(const nlohmann::json& json) {
    NetworkInfo network;
    network.rssi = json.value("rssi", -70);
    network.rat = json.value("rat", "LTE");
    return network;
}

} // namespace tracker