#pragma once

#include "../IClock.hpp"
#include <chrono>
#include <string>

namespace tracker::sim {

class SimulatedClock : public IClock {
public:
    SimulatedClock(std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now());
    ~SimulatedClock() override = default;

    // IClock interface
    std::chrono::steady_clock::time_point now() const override;
    std::string getIsoTimestamp() const override;

    // Simulation controls
    void advance(std::chrono::milliseconds duration);
    void setCurrentTime(std::chrono::steady_clock::time_point time);
    
    // Time manipulation for testing
    void freezeTime() { frozen_ = true; }
    void unfreezeTime() { frozen_ = false; }
    bool isFrozen() const { return frozen_; }

private:
    std::chrono::steady_clock::time_point simulatedTime_;
    std::chrono::steady_clock::time_point realStartTime_;
    bool frozen_ = false;
    
    std::string formatTime(std::chrono::steady_clock::time_point time) const;
};

} // namespace tracker::sim