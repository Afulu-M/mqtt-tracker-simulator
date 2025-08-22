#pragma once

#include "Event.hpp"
#include <functional>
#include <vector>

namespace tracker {

enum class DeviceState {
    Idle,
    Driving,
    Parked,
    LowBattery
};

class StateMachine {
public:
    using EventEmitter = std::function<void(const Event&)>;
    
    StateMachine();
    
    void setEventEmitter(EventEmitter emitter) { eventEmitter_ = emitter; }
    
    DeviceState getCurrentState() const { return currentState_; }
    
    void processIgnition(bool on);
    void processMotion(bool moving);
    void processBatteryLevel(double percentage);
    void processGeofenceChange(bool entered, const std::string& geofenceId);
    void processSpeedLimit(double currentSpeed, double limit);
    
private:
    void transitionTo(DeviceState newState);
    void emitEvent(EventType type, const std::unordered_map<std::string, std::string>& extras = {});
    
    DeviceState currentState_;
    EventEmitter eventEmitter_;
    
    bool ignitionOn_ = false;
    bool inMotion_ = false;
    double batteryPercentage_ = 100.0;
    std::vector<std::string> currentGeofences_;
};

std::string stateToString(DeviceState state);

} // namespace tracker