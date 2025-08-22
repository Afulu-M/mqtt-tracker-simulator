#include "SimulatedClock.hpp"
#include <iomanip>
#include <sstream>

namespace tracker::sim {

SimulatedClock::SimulatedClock(std::chrono::steady_clock::time_point startTime)
    : simulatedTime_(startTime), realStartTime_(startTime) {
}

std::chrono::steady_clock::time_point SimulatedClock::now() const {
    if (frozen_) {
        return simulatedTime_;
    }
    
    // Return simulated time + elapsed real time since creation
    auto realElapsed = std::chrono::steady_clock::now() - realStartTime_;
    return simulatedTime_ + realElapsed;
}

std::string SimulatedClock::getIsoTimestamp() const {
    return formatTime(now());
}

void SimulatedClock::advance(std::chrono::milliseconds duration) {
    simulatedTime_ += duration;
    realStartTime_ = std::chrono::steady_clock::now(); // Reset real time reference
}

void SimulatedClock::setCurrentTime(std::chrono::steady_clock::time_point time) {
    simulatedTime_ = time;
    realStartTime_ = std::chrono::steady_clock::now(); // Reset real time reference
}

std::string SimulatedClock::formatTime(std::chrono::steady_clock::time_point time) const {
    // Convert to system_clock for formatting (approximation)
    auto duration = time.time_since_epoch();
    auto system_time = std::chrono::system_clock::time_point(duration);
    auto time_t = std::chrono::system_clock::to_time_t(system_time);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    
    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    
    return ss.str();
}

} // namespace tracker::sim