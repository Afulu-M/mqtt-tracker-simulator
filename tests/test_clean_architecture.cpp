#include <gtest/gtest.h>
#include "../core/domain/DeviceStateMachine.hpp"
#include "../core/domain/EventBus.hpp"
#include "../core/domain/TelemetryPipeline.hpp"
#include "../core/adapters/DefaultPolicies.hpp"
#include "../core/sim/MockTransport.hpp"
#include "../core/sim/SimulatedClock.hpp"
#include <memory>

using namespace tracker;

class CleanArchitectureTest : public ::testing::Test {
protected:
    void SetUp() override {
        clock_ = std::make_shared<sim::SimulatedClock>();
        eventBus_ = std::make_shared<domain::EventBus>();
        transport_ = std::make_shared<sim::MockTransport>();
        policyEngine_ = std::make_shared<adapters::DefaultPolicyEngine>();
        
        stateMachine_ = std::make_unique<domain::DeviceStateMachine>(eventBus_, clock_);
        telemetryPipeline_ = std::make_unique<domain::TelemetryPipeline>(
            transport_, eventBus_, policyEngine_);
    }

    std::shared_ptr<sim::SimulatedClock> clock_;
    std::shared_ptr<domain::EventBus> eventBus_;
    std::shared_ptr<sim::MockTransport> transport_;
    std::shared_ptr<adapters::DefaultPolicyEngine> policyEngine_;
    std::unique_ptr<domain::DeviceStateMachine> stateMachine_;
    std::unique_ptr<domain::TelemetryPipeline> telemetryPipeline_;
};

TEST_F(CleanArchitectureTest, StateMachineTransitions) {
    // Test deterministic state transitions
    EXPECT_EQ(stateMachine_->getCurrentState(), domain::DeviceState::Idle);
    
    // Ignition on should transition to Driving
    stateMachine_->setIgnition(true);
    eventBus_->processEvents();
    EXPECT_EQ(stateMachine_->getCurrentState(), domain::DeviceState::Driving);
    
    // Motion stop should transition to Parked
    stateMachine_->setMotion(false);
    eventBus_->processEvents();
    EXPECT_EQ(stateMachine_->getCurrentState(), domain::DeviceState::Parked);
    
    // Low battery should transition to LowBattery state
    stateMachine_->setBatteryLevel(10.0);
    eventBus_->processEvents();
    EXPECT_EQ(stateMachine_->getCurrentState(), domain::DeviceState::LowBattery);
}

TEST_F(CleanArchitectureTest, EventBusIsolation) {
    int heartbeatCount = 0;
    int motionCount = 0;
    
    // Subscribe to specific events
    eventBus_->subscribe(EventType::Heartbeat, [&](const Event&) {
        heartbeatCount++;
    });
    
    eventBus_->subscribe(EventType::MotionStart, [&](const Event&) {
        motionCount++;
    });
    
    // Publish events
    Event heartbeat;
    heartbeat.eventType = EventType::Heartbeat;
    eventBus_->publish(heartbeat);
    
    Event motion;
    motion.eventType = EventType::MotionStart;
    eventBus_->publish(motion);
    
    // Process events
    eventBus_->processEvents();
    
    EXPECT_EQ(heartbeatCount, 1);
    EXPECT_EQ(motionCount, 1);
}

TEST_F(CleanArchitectureTest, TransportAbstraction) {
    transport_->setConnected(true);
    EXPECT_TRUE(transport_->isConnected());
    
    // Test publish
    bool success = transport_->publish("test/topic", "test payload", 1);
    EXPECT_TRUE(success);
    
    const auto& messages = transport_->getPublishedMessages();
    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].topic, "test/topic");
    EXPECT_EQ(messages[0].payload, "test payload");
    EXPECT_EQ(messages[0].qos, 1);
}

TEST_F(CleanArchitectureTest, PolicyBasedBehavior) {
    const auto& retryPolicy = policyEngine_->getRetryPolicy();
    const auto& reportingPolicy = policyEngine_->getReportingPolicy();
    const auto& powerPolicy = policyEngine_->getPowerPolicy();
    
    // Test retry policy
    EXPECT_TRUE(retryPolicy.shouldRetry(1));
    EXPECT_FALSE(retryPolicy.shouldRetry(10)); // Beyond max attempts
    
    auto delay1 = retryPolicy.getBackoffDelay(1);
    auto delay2 = retryPolicy.getBackoffDelay(2);
    EXPECT_LT(delay1, delay2); // Exponential backoff
    
    // Test reporting policy  
    auto stationaryInterval = reportingPolicy.getHeartbeatInterval(false);
    auto movingInterval = reportingPolicy.getHeartbeatInterval(true);
    EXPECT_GT(stationaryInterval, movingInterval); // Less frequent when stationary
    
    // Test power policy
    double stationaryDrain = powerPolicy.getBatteryDrainRate(false, false);
    double movingDrain = powerPolicy.getBatteryDrainRate(true, false);
    EXPECT_GT(movingDrain, stationaryDrain); // Higher drain when moving
}

TEST_F(CleanArchitectureTest, DeterministicTimeSimulation) {
    auto startTime = std::chrono::steady_clock::now();
    clock_->setCurrentTime(startTime);
    clock_->freezeTime();
    
    EXPECT_EQ(clock_->now(), startTime);
    
    // Time shouldn't advance when frozen
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(clock_->now(), startTime);
    
    // Manual advance
    clock_->advance(std::chrono::hours(1));
    auto expectedTime = startTime + std::chrono::hours(1);
    EXPECT_EQ(clock_->now(), expectedTime);
}

TEST_F(CleanArchitectureTest, IntegrationTestWithMockTransport) {
    // Setup telemetry pipeline
    ports::Credentials creds;
    creds.host = "test.iot.hub";
    creds.port = 8883;
    creds.clientId = "test-device";
    
    transport_->connect(creds);
    telemetryPipeline_->start("test-device");
    
    // Generate an event through the state machine
    stateMachine_->setIgnition(true);
    eventBus_->processEvents();
    telemetryPipeline_->processEvents();
    
    // Verify telemetry was sent
    const auto& messages = transport_->getPublishedMessages();
    EXPECT_GT(messages.size(), 0);
    
    // Check message format
    bool foundIgnitionEvent = false;
    for (const auto& msg : messages) {
        if (msg.topic.find("test-device") != std::string::npos &&
            msg.payload.find("ignition_on") != std::string::npos) {
            foundIgnitionEvent = true;
            break;
        }
    }
    EXPECT_TRUE(foundIgnitionEvent);
}