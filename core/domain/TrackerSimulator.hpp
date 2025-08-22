#pragma once

#include "../ports/ITransport.hpp"
#include "../ports/IEventBus.hpp"
#include "../ports/IPolicyEngine.hpp"
#include "../IClock.hpp"
#include "../IRng.hpp"
#include "DeviceStateMachine.hpp"
#include "TelemetryPipeline.hpp"
#include "../Battery.hpp"
#include "../Geo.hpp"
#include <memory>
#include <string>
#include <vector>

namespace tracker::domain {

struct TrackerConfig {
    std::string deviceId = "SIM-001";
    std::string iotHubHost;
    std::string deviceKeyBase64;
    
    Location startLocation = {-26.2041, 28.0473, 1720.0, 12.5};
    double speedLimitKph = 90.0;
    
    std::vector<RoutePoint> route;
    std::vector<Geofence> geofences;
};

class TrackerSimulator {
public:
    TrackerSimulator(std::shared_ptr<ports::ITransport> transport,
                    std::shared_ptr<ports::IEventBus> eventBus,
                    std::shared_ptr<ports::IPolicyEngine> policyEngine,
                    std::shared_ptr<IClock> clock,
                    std::shared_ptr<IRng> rng);

    void configure(const TrackerConfig& config);
    void start();
    void stop();
    
    bool isRunning() const { return running_; }
    void tick();
    
    // External controls for simulation
    void setIgnition(bool on);
    void setSpeed(double speedKph);
    void setBatteryPercentage(double pct);
    void startDriving(double durationMinutes = 10.0);
    void generateSpike(int eventCount = 10);

private:
    void connectToIoTHub();
    void updateLocation();
    void checkGeofences();
    void updateBattery();
    void onTransportMessage(std::string_view topic, std::string_view payload);
    void onTransportConnection(bool connected, std::string_view reason);
    
    Event createBaseEvent(EventType type) const;

    // Dependencies (Ports)
    std::shared_ptr<ports::ITransport> transport_;
    std::shared_ptr<ports::IEventBus> eventBus_;
    std::shared_ptr<ports::IPolicyEngine> policyEngine_;
    std::shared_ptr<IClock> clock_;
    std::shared_ptr<IRng> rng_;
    
    // Domain Components
    std::unique_ptr<DeviceStateMachine> stateMachine_;
    std::unique_ptr<TelemetryPipeline> telemetryPipeline_;
    Battery battery_;
    
    // Configuration & State
    TrackerConfig config_;
    bool running_ = false;
    bool connected_ = false;
    
    // Current telemetry data
    Location currentLocation_;
    double currentSpeed_ = 0.0;
    double currentHeading_ = 0.0;
    NetworkInfo networkInfo_;
    uint64_t sequenceNumber_ = 0;
    
    // Route simulation
    double routeProgress_ = 0.0;
    bool followingRoute_ = false;
    std::chrono::steady_clock::time_point driveStartTime_;
    double driveDurationSeconds_ = 0.0;
    
    // Geofencing
    std::vector<std::string> currentGeofenceIds_;
    
    // Connection management
    std::chrono::steady_clock::time_point lastReconnectAttempt_;
    int reconnectAttempts_ = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
};

} // namespace tracker::domain