#include "StateMachine.hpp"
#include <algorithm>

namespace tracker {

StateMachine::StateMachine() : currentState_(DeviceState::Idle) {}

void StateMachine::processIgnition(bool on) {
    if (ignitionOn_ == on) return;
    
    ignitionOn_ = on;
    emitEvent(on ? EventType::IgnitionOn : EventType::IgnitionOff);
    
    if (batteryPercentage_ <= 20.0) {
        transitionTo(DeviceState::LowBattery);
    } else if (on && inMotion_) {
        transitionTo(DeviceState::Driving);
    } else if (on) {
        transitionTo(DeviceState::Parked);
    } else {
        transitionTo(DeviceState::Idle);
    }
}

void StateMachine::processMotion(bool moving) {
    if (inMotion_ == moving) return;
    
    inMotion_ = moving;
    emitEvent(moving ? EventType::MotionStart : EventType::MotionStop);
    
    if (batteryPercentage_ <= 20.0) {
        transitionTo(DeviceState::LowBattery);
    } else if (ignitionOn_ && moving) {
        transitionTo(DeviceState::Driving);
    } else if (ignitionOn_) {
        transitionTo(DeviceState::Parked);
    } else {
        transitionTo(DeviceState::Idle);
    }
}

void StateMachine::processBatteryLevel(double percentage) {
    bool wasLowBattery = (batteryPercentage_ <= 20.0);
    batteryPercentage_ = percentage;
    bool isLowBattery = (percentage <= 20.0);
    
    if (!wasLowBattery && isLowBattery) {
        emitEvent(EventType::LowBattery);
        transitionTo(DeviceState::LowBattery);
    } else if (wasLowBattery && !isLowBattery) {
        if (ignitionOn_ && inMotion_) {
            transitionTo(DeviceState::Driving);
        } else if (ignitionOn_) {
            transitionTo(DeviceState::Parked);
        } else {
            transitionTo(DeviceState::Idle);
        }
    }
}

void StateMachine::processGeofenceChange(bool entered, const std::string& geofenceId) {
    if (entered) {
        currentGeofences_.push_back(geofenceId);
        emitEvent(EventType::GeofenceEnter, {{"geofenceId", geofenceId}});
    } else {
        auto it = std::find(currentGeofences_.begin(), currentGeofences_.end(), geofenceId);
        if (it != currentGeofences_.end()) {
            currentGeofences_.erase(it);
            emitEvent(EventType::GeofenceExit, {{"geofenceId", geofenceId}});
        }
    }
}

void StateMachine::processSpeedLimit(double currentSpeed, double limit) {
    if (currentSpeed > limit) {
        emitEvent(EventType::SpeedOverLimit, {
            {"limit", std::to_string(static_cast<int>(limit))},
            {"measured", std::to_string(static_cast<int>(currentSpeed))}
        });
    }
}

void StateMachine::transitionTo(DeviceState newState) {
    if (currentState_ != newState) {
        currentState_ = newState;
    }
}

void StateMachine::emitEvent(EventType type, const std::unordered_map<std::string, std::string>& extras) {
    if (eventEmitter_) {
        Event event;
        event.eventType = type;
        event.extras = extras;
        eventEmitter_(event);
    }
}

std::string stateToString(DeviceState state) {
    switch (state) {
        case DeviceState::Idle: return "Idle";
        case DeviceState::Driving: return "Driving";
        case DeviceState::Parked: return "Parked";
        case DeviceState::LowBattery: return "LowBattery";
        default: return "Unknown";
    }
}

} // namespace tracker