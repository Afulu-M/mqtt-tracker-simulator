#pragma once

#include "../ports/ITransport.hpp"
#include "../ports/IEventBus.hpp"
#include "../ports/IPolicyEngine.hpp"
#include "../JsonCodec.hpp"
#include "../Event.hpp"
#include <memory>
#include <queue>
#include <chrono>

namespace tracker::domain {

class TelemetryPipeline {
public:
    TelemetryPipeline(std::shared_ptr<ports::ITransport> transport,
                     std::shared_ptr<ports::IEventBus> eventBus,
                     std::shared_ptr<ports::IPolicyEngine> policyEngine);

    void start(const std::string& deviceId);
    void stop();
    
    void processEvents();
    
private:
    void onEvent(const Event& event);
    void sendTelemetry(const Event& event);
    void retryFailedMessages();
    
    bool shouldPublish(const Event& event) const;
    std::string buildTopic(const std::string& deviceId, const Event& event) const;
    
    struct PendingMessage {
        Event event;
        std::string topic;
        std::string payload;
        int attempts = 0;
        std::chrono::steady_clock::time_point nextRetry;
    };
    
    std::shared_ptr<ports::ITransport> transport_;
    std::shared_ptr<ports::IEventBus> eventBus_;
    std::shared_ptr<ports::IPolicyEngine> policyEngine_;
    
    JsonCodec codec_;
    std::string deviceId_;
    bool running_ = false;
    
    std::queue<PendingMessage> retryQueue_;
    std::chrono::steady_clock::time_point lastHeartbeat_;
    
    // State for reporting policies
    double lastReportedBatteryPct_ = 100.0;
    bool lastMotionState_ = false;
};

} // namespace tracker::domain