#include "DeviceStateMachine.hpp"
#include <iostream>

namespace tracker::domain {

DeviceStateMachine::DeviceStateMachine(std::shared_ptr<ports::IEventBus> eventBus, 
                                     std::shared_ptr<IClock> clock)
    : eventBus_(eventBus), clock_(clock) {
}

void DeviceStateMachine::processEvent(DeviceEvent event) {
    DeviceState newState = currentState_;
    
    switch (currentState_) {
        case DeviceState::Idle:
            if (event == DeviceEvent::IgnitionOn) {
                newState = DeviceState::Driving;
            } else if (event == DeviceEvent::BatteryLow) {
                newState = DeviceState::LowBattery;
            } else if (event == DeviceEvent::ConnectionLost) {
                newState = DeviceState::Offline;
            }
            break;
            
        case DeviceState::Driving:
            if (event == DeviceEvent::IgnitionOff || event == DeviceEvent::MotionStopped) {
                startParkingTimer();
                newState = DeviceState::Parked;
            } else if (event == DeviceEvent::BatteryLow) {
                newState = DeviceState::LowBattery;
            } else if (event == DeviceEvent::ConnectionLost) {
                newState = DeviceState::Offline;
            }
            break;
            
        case DeviceState::Parked:
            if (event == DeviceEvent::IgnitionOn || event == DeviceEvent::MotionDetected) {
                stopParkingTimer();
                newState = DeviceState::Driving;
            } else if (event == DeviceEvent::ParkingTimerExpired) {
                newState = DeviceState::Idle;
            } else if (event == DeviceEvent::BatteryLow) {
                newState = DeviceState::LowBattery;
            } else if (event == DeviceEvent::ConnectionLost) {
                newState = DeviceState::Offline;
            }
            break;
            
        case DeviceState::LowBattery:
            if (event == DeviceEvent::BatteryNormal) {
                newState = ignitionOn_ ? DeviceState::Driving : DeviceState::Idle;
            } else if (event == DeviceEvent::ConnectionLost) {
                newState = DeviceState::Offline;
            }
            break;
            
        case DeviceState::Offline:
            if (event == DeviceEvent::ConnectionRestored) {
                if (batteryPercentage_ < LOW_BATTERY_THRESHOLD) {
                    newState = DeviceState::LowBattery;
                } else if (ignitionOn_ && inMotion_) {
                    newState = DeviceState::Driving;
                } else if (ignitionOn_ || inMotion_) {
                    newState = DeviceState::Parked;
                } else {
                    newState = DeviceState::Idle;
                }
            }
            break;
    }
    
    if (newState != currentState_) {
        transitionTo(newState);
    }
}

void DeviceStateMachine::setIgnition(bool on) {
    ignitionOn_ = on;
    processEvent(on ? DeviceEvent::IgnitionOn : DeviceEvent::IgnitionOff);
}

void DeviceStateMachine::setMotion(bool inMotion) {
    inMotion_ = inMotion;
    if (inMotion) {
        processEvent(DeviceEvent::MotionDetected);
    } else {
        motionStoppedTime_ = clock_->now();
        processEvent(DeviceEvent::MotionStopped);
    }
}

void DeviceStateMachine::setBatteryLevel(double percentage) {
    bool wasLow = batteryPercentage_ < LOW_BATTERY_THRESHOLD;
    batteryPercentage_ = percentage;
    bool isLow = percentage < LOW_BATTERY_THRESHOLD;
    
    if (!wasLow && isLow) {
        processEvent(DeviceEvent::BatteryLow);
    } else if (wasLow && !isLow) {
        processEvent(DeviceEvent::BatteryNormal);
    }
}

void DeviceStateMachine::setConnectionStatus(bool connected) {
    bool wasConnected = connected_;
    connected_ = connected;
    
    if (wasConnected && !connected) {
        processEvent(DeviceEvent::ConnectionLost);
    } else if (!wasConnected && connected) {
        processEvent(DeviceEvent::ConnectionRestored);
    }
}

void DeviceStateMachine::transitionTo(DeviceState newState) {
    DeviceState oldState = currentState_;
    currentState_ = newState;
    
    std::cout << "State transition: " << stateToString(oldState) 
              << " -> " << stateToString(newState) << std::endl;
    
    // Emit appropriate events based on state transitions
    switch (newState) {
        case DeviceState::Driving:
            if (oldState != DeviceState::Driving) {
                emitTrackerEvent(ignitionOn_ ? EventType::IgnitionOn : EventType::MotionStart);
            }
            break;
        case DeviceState::Parked:
            if (oldState == DeviceState::Driving) {
                emitTrackerEvent(ignitionOn_ ? EventType::MotionStop : EventType::IgnitionOff);
            }
            break;
        case DeviceState::Idle:
            if (oldState != DeviceState::Idle) {
                emitTrackerEvent(EventType::MotionStop);
            }
            break;
        case DeviceState::LowBattery:
            emitTrackerEvent(EventType::LowBattery);
            break;
        default:
            break;
    }
}

void DeviceStateMachine::emitTrackerEvent(EventType eventType, 
                                        const std::unordered_map<std::string, std::string>& extras) {
    Event event;
    event.eventType = eventType;
    event.timestamp = clock_->getIsoTimestamp();
    event.extras = extras;
    eventBus_->publish(event);
}

void DeviceStateMachine::startParkingTimer() {
    motionStoppedTime_ = clock_->now();
}

void DeviceStateMachine::stopParkingTimer() {
    motionStoppedTime_ = {};
}

bool DeviceStateMachine::isParkingTimerExpired() const {
    if (motionStoppedTime_ == std::chrono::steady_clock::time_point{}) {
        return false;
    }
    auto elapsed = clock_->now() - motionStoppedTime_;
    return elapsed >= PARKING_TIMEOUT;
}

std::string stateToString(DeviceState state) {
    switch (state) {
        case DeviceState::Idle: return "Idle";
        case DeviceState::Driving: return "Driving";
        case DeviceState::Parked: return "Parked";
        case DeviceState::LowBattery: return "LowBattery";
        case DeviceState::Offline: return "Offline";
        default: return "Unknown";
    }
}

} // namespace tracker::domain