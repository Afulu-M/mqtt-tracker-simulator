#pragma once

#include "../Event.hpp"
#include "../ports/IEventBus.hpp"
#include "../ports/IClock.hpp"
#include <memory>
#include <chrono>
#include <functional>

namespace tracker::domain {

enum class DeviceState {
    Idle,
    Driving,
    Parked,
    LowBattery,
    Offline
};

enum class DeviceEvent {
    IgnitionOn,
    IgnitionOff,
    MotionDetected,
    MotionStopped,
    BatteryLow,
    BatteryNormal,
    ConnectionLost,
    ConnectionRestored,
    ParkingTimerExpired
};

class DeviceStateMachine {
public:
    DeviceStateMachine(std::shared_ptr<ports::IEventBus> eventBus, 
                      std::shared_ptr<IClock> clock);

    void processEvent(DeviceEvent event);
    DeviceState getCurrentState() const { return currentState_; }
    
    // External state changes
    void setIgnition(bool on);
    void setMotion(bool inMotion);
    void setBatteryLevel(double percentage);
    void setConnectionStatus(bool connected);

private:
    void transitionTo(DeviceState newState);
    void emitTrackerEvent(EventType eventType, const std::unordered_map<std::string, std::string>& extras = {});
    void startParkingTimer();
    void stopParkingTimer();
    bool isParkingTimerExpired() const;

    std::shared_ptr<ports::IEventBus> eventBus_;
    std::shared_ptr<IClock> clock_;
    
    DeviceState currentState_ = DeviceState::Idle;
    
    // State tracking
    bool ignitionOn_ = false;
    bool inMotion_ = false;
    bool connected_ = true;
    double batteryPercentage_ = 100.0;
    
    // Timing
    std::chrono::steady_clock::time_point motionStoppedTime_;
    static constexpr std::chrono::minutes PARKING_TIMEOUT{2};
    static constexpr double LOW_BATTERY_THRESHOLD = 15.0;
};

std::string stateToString(DeviceState state);

} // namespace tracker::domain