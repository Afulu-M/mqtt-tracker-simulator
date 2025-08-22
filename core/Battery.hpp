#pragma once

#include "Event.hpp"
#include "IRng.hpp"
#include <memory>

namespace tracker {

class Battery {
public:
    explicit Battery(std::shared_ptr<IRng> rng);
    
    void tick(double deltaSeconds, bool isDriving);
    
    BatteryInfo getInfo() const;
    double getPercentage() const { return percentage_; }
    
    void setPercentage(double pct) { percentage_ = std::clamp(pct, 0.0, 100.0); }
    
private:
    std::shared_ptr<IRng> rng_;
    double percentage_ = 100.0;
    
    static constexpr double IDLE_DRAIN_PER_HOUR = 0.5;
    static constexpr double DRIVING_DRAIN_PER_HOUR = 2.0;
    static constexpr double MIN_VOLTAGE = 3.2;
    static constexpr double MAX_VOLTAGE = 4.2;
};

} // namespace tracker