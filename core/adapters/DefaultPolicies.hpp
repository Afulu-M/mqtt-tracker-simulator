#pragma once

#include "../ports/IPolicyEngine.hpp"
#include <algorithm>
#include <cmath>

namespace tracker::adapters {

class ExponentialBackoffRetryPolicy : public ports::RetryPolicy {
public:
    ExponentialBackoffRetryPolicy(std::chrono::milliseconds baseDelay = std::chrono::milliseconds(1000),
                                double multiplier = 2.0,
                                std::chrono::milliseconds maxDelay = std::chrono::minutes(5),
                                int maxAttempts = 5)
        : baseDelay_(baseDelay), multiplier_(multiplier), maxDelay_(maxDelay), maxAttempts_(maxAttempts) {}

    std::chrono::milliseconds getBackoffDelay(int attemptCount) const override {
        auto delay = std::chrono::milliseconds(static_cast<long long>(
            baseDelay_.count() * std::pow(multiplier_, attemptCount - 1)));
        return std::min(delay, maxDelay_);
    }

    bool shouldRetry(int attemptCount) const override {
        return attemptCount < maxAttempts_;
    }

private:
    std::chrono::milliseconds baseDelay_;
    double multiplier_;
    std::chrono::milliseconds maxDelay_;
    int maxAttempts_;
};

class AdaptiveReportingPolicy : public ports::ReportingPolicy {
public:
    AdaptiveReportingPolicy(std::chrono::seconds stationaryInterval = std::chrono::minutes(5),
                          std::chrono::seconds movingInterval = std::chrono::minutes(1))
        : stationaryInterval_(stationaryInterval), movingInterval_(movingInterval) {}

    std::chrono::seconds getHeartbeatInterval(bool inMotion) const override {
        return inMotion ? movingInterval_ : stationaryInterval_;
    }

    bool shouldReportMotionChange() const override {
        return true; // Always report motion state changes
    }

    bool shouldReportBatteryLevel(double currentPct, double lastReportedPct) const override {
        return std::abs(currentPct - lastReportedPct) >= 5.0; // 5% threshold
    }

private:
    std::chrono::seconds stationaryInterval_;
    std::chrono::seconds movingInterval_;
};

class ConservativePowerPolicy : public ports::PowerPolicy {
public:
    ConservativePowerPolicy(double stationaryDrainRate = 0.1, // %/hour
                          double movingDrainRate = 0.5,     // %/hour  
                          double connectedDrainMultiplier = 1.2,
                          double lowBatteryThreshold = 15.0)
        : stationaryDrainRate_(stationaryDrainRate),
          movingDrainRate_(movingDrainRate),
          connectedDrainMultiplier_(connectedDrainMultiplier),
          lowBatteryThreshold_(lowBatteryThreshold) {}

    double getBatteryDrainRate(bool inMotion, bool connected) const override {
        double baseRate = inMotion ? movingDrainRate_ : stationaryDrainRate_;
        return connected ? baseRate * connectedDrainMultiplier_ : baseRate;
    }

    bool shouldEnterLowPowerMode(double batteryPct) const override {
        return batteryPct <= lowBatteryThreshold_;
    }

private:
    double stationaryDrainRate_;
    double movingDrainRate_;
    double connectedDrainMultiplier_;
    double lowBatteryThreshold_;
};

class DefaultPolicyEngine : public ports::IPolicyEngine {
public:
    DefaultPolicyEngine() = default;

    const ports::RetryPolicy& getRetryPolicy() const override {
        return retryPolicy_;
    }

    const ports::ReportingPolicy& getReportingPolicy() const override {
        return reportingPolicy_;
    }

    const ports::PowerPolicy& getPowerPolicy() const override {
        return powerPolicy_;
    }

private:
    ExponentialBackoffRetryPolicy retryPolicy_;
    AdaptiveReportingPolicy reportingPolicy_;
    ConservativePowerPolicy powerPolicy_;
};

} // namespace tracker::adapters