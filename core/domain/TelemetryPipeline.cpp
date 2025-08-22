#include "TelemetryPipeline.hpp"
#include <iostream>

namespace tracker::domain {

TelemetryPipeline::TelemetryPipeline(std::shared_ptr<ports::ITransport> transport,
                                   std::shared_ptr<ports::IEventBus> eventBus,
                                   std::shared_ptr<ports::IPolicyEngine> policyEngine)
    : transport_(transport), eventBus_(eventBus), policyEngine_(policyEngine) {
}

void TelemetryPipeline::start(const std::string& deviceId) {
    deviceId_ = deviceId;
    running_ = true;
    lastHeartbeat_ = std::chrono::steady_clock::now();
    
    // Subscribe to all events
    for (auto eventType : {EventType::Heartbeat, EventType::IgnitionOn, EventType::IgnitionOff,
                          EventType::MotionStart, EventType::MotionStop, EventType::GeofenceEnter,
                          EventType::GeofenceExit, EventType::SpeedOverLimit, EventType::LowBattery}) {
        eventBus_->subscribe(eventType, [this](const Event& event) {
            onEvent(event);
        });
    }
}

void TelemetryPipeline::stop() {
    running_ = false;
    // Unsubscribe from events
    for (auto eventType : {EventType::Heartbeat, EventType::IgnitionOn, EventType::IgnitionOff,
                          EventType::MotionStart, EventType::MotionStop, EventType::GeofenceEnter,
                          EventType::GeofenceExit, EventType::SpeedOverLimit, EventType::LowBattery}) {
        eventBus_->unsubscribe(eventType);
    }
}

void TelemetryPipeline::processEvents() {
    if (!running_) return;
    
    retryFailedMessages();
    
    // Check if heartbeat is due
    auto now = std::chrono::steady_clock::now();
    bool inMotion = lastMotionState_;
    auto heartbeatInterval = policyEngine_->getReportingPolicy().getHeartbeatInterval(inMotion);
    
    if (now - lastHeartbeat_ >= heartbeatInterval) {
        Event heartbeat;
        heartbeat.eventType = EventType::Heartbeat;
        heartbeat.deviceId = deviceId_;
        eventBus_->publish(heartbeat);
        lastHeartbeat_ = now;
    }
}

void TelemetryPipeline::onEvent(const Event& event) {
    if (!running_ || !shouldPublish(event)) return;
    
    sendTelemetry(event);
    
    // Update state for policy decisions
    if (event.eventType == EventType::MotionStart) {
        lastMotionState_ = true;
    } else if (event.eventType == EventType::MotionStop) {
        lastMotionState_ = false;
    }
}

void TelemetryPipeline::sendTelemetry(const Event& event) {
    if (!transport_->isConnected()) {
        // Queue for retry when connection restored
        PendingMessage msg;
        msg.event = event;
        msg.topic = buildTopic(deviceId_, event);
        msg.payload = codec_.encode(event);
        msg.nextRetry = std::chrono::steady_clock::now();
        retryQueue_.push(msg);
        return;
    }
    
    std::string topic = buildTopic(deviceId_, event);
    std::string payload = codec_.encode(event);
    
    if (!transport_->publish(topic, payload, 1)) {
        // Failed to publish - add to retry queue
        PendingMessage msg;
        msg.event = event;
        msg.topic = topic;
        msg.payload = payload;
        msg.attempts = 1;
        msg.nextRetry = std::chrono::steady_clock::now() + 
                       policyEngine_->getRetryPolicy().getBackoffDelay(1);
        retryQueue_.push(msg);
    }
}

void TelemetryPipeline::retryFailedMessages() {
    if (retryQueue_.empty() || !transport_->isConnected()) return;
    
    auto now = std::chrono::steady_clock::now();
    
    while (!retryQueue_.empty()) {
        auto& msg = retryQueue_.front();
        
        if (msg.nextRetry > now) break;
        
        if (!policyEngine_->getRetryPolicy().shouldRetry(msg.attempts)) {
            std::cout << "Dropping message after " << msg.attempts << " attempts" << std::endl;
            retryQueue_.pop();
            continue;
        }
        
        if (transport_->publish(msg.topic, msg.payload, 1)) {
            retryQueue_.pop();
        } else {
            msg.attempts++;
            msg.nextRetry = now + policyEngine_->getRetryPolicy().getBackoffDelay(msg.attempts);
            break; // Try this message again later, keep others in queue
        }
    }
}

bool TelemetryPipeline::shouldPublish(const Event& event) const {
    const auto& policy = policyEngine_->getReportingPolicy();
    
    switch (event.eventType) {
        case EventType::Heartbeat:
            return true; // Heartbeats are always sent when scheduled
        case EventType::MotionStart:
        case EventType::MotionStop:
            return policy.shouldReportMotionChange();
        case EventType::LowBattery:
            return policy.shouldReportBatteryLevel(event.battery.percentage, lastReportedBatteryPct_);
        default:
            return true; // All other events are important
    }
}

std::string TelemetryPipeline::buildTopic(const std::string& deviceId, const Event& event) const {
    return "devices/" + deviceId + "/messages/events/";
}

} // namespace tracker::domain