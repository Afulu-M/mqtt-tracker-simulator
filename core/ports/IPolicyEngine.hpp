#pragma once

#include <chrono>
#include <functional>

namespace tracker::ports {

struct RetryPolicy {
    virtual ~RetryPolicy() = default;
    virtual std::chrono::milliseconds getBackoffDelay(int attemptCount) const = 0;
    virtual bool shouldRetry(int attemptCount) const = 0;
};

struct ReportingPolicy {
    virtual ~ReportingPolicy() = default;
    virtual std::chrono::seconds getHeartbeatInterval(bool inMotion) const = 0;
    virtual bool shouldReportMotionChange() const = 0;
    virtual bool shouldReportBatteryLevel(double currentPct, double lastReportedPct) const = 0;
};

struct PowerPolicy {
    virtual ~PowerPolicy() = default;
    virtual double getBatteryDrainRate(bool inMotion, bool connected) const = 0;
    virtual bool shouldEnterLowPowerMode(double batteryPct) const = 0;
};

class IPolicyEngine {
public:
    virtual ~IPolicyEngine() = default;
    
    virtual const RetryPolicy& getRetryPolicy() const = 0;
    virtual const ReportingPolicy& getReportingPolicy() const = 0;
    virtual const PowerPolicy& getPowerPolicy() const = 0;
};

} // namespace tracker::ports