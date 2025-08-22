#include "Battery.hpp"
#include <algorithm>

namespace tracker {

Battery::Battery(std::shared_ptr<IRng> rng) : rng_(rng) {}

void Battery::tick(double deltaSeconds, bool isDriving) {
    double drainRate = isDriving ? DRIVING_DRAIN_PER_HOUR : IDLE_DRAIN_PER_HOUR;
    double baseDrain = (drainRate / 3600.0) * deltaSeconds;
    
    double jitter = rng_->uniform(-0.1, 0.1);
    double actualDrain = baseDrain * (1.0 + jitter);
    
    percentage_ = std::clamp(percentage_ - actualDrain, 0.0, 100.0);
}

BatteryInfo Battery::getInfo() const {
    BatteryInfo info;
    info.percentage = percentage_;
    
    double voltageRange = MAX_VOLTAGE - MIN_VOLTAGE;
    info.voltage = MIN_VOLTAGE + (percentage_ / 100.0) * voltageRange;
    
    double jitter = rng_->uniform(-0.05, 0.05);
    info.voltage += jitter;
    info.voltage = std::clamp(info.voltage, MIN_VOLTAGE, MAX_VOLTAGE);
    
    return info;
}

} // namespace tracker