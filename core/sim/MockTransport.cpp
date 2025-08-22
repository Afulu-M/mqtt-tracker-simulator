#include "MockTransport.hpp"
#include <algorithm>

namespace tracker::sim {

MockTransport::MockTransport() = default;

bool MockTransport::connect(const ports::Credentials& credentials) {
    lastCredentials_ = credentials;
    connected_ = true;
    
    if (connectionHandler_) {
        connectionHandler_(true, "Mock connection established");
    }
    return true;
}

void MockTransport::disconnect() {
    if (connected_) {
        connected_ = false;
        if (connectionHandler_) {
            connectionHandler_(false, "Disconnected");
        }
    }
}

bool MockTransport::isConnected() const {
    return connected_;
}

bool MockTransport::publish(std::string_view topic, std::string_view payload, int qos) {
    if (!connected_ || failPublish_) {
        return false;
    }
    
    MockMessage msg;
    msg.topic = std::string(topic);
    msg.payload = std::string(payload);
    msg.qos = qos;
    msg.timestamp = std::chrono::steady_clock::now();
    
    publishedMessages_.push_back(msg);
    return true;
}

bool MockTransport::subscribe(std::string_view topic, int qos) {
    if (!connected_) return false;
    
    auto topicStr = std::string(topic);
    if (std::find(subscriptions_.begin(), subscriptions_.end(), topicStr) == subscriptions_.end()) {
        subscriptions_.push_back(topicStr);
    }
    return true;
}

void MockTransport::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void MockTransport::setConnectionHandler(ConnectionHandler handler) {
    connectionHandler_ = std::move(handler);
}

void MockTransport::processEvents() {
    // Process incoming messages
    while (!incomingMessages_.empty()) {
        if (messageHandler_) {
            const auto& msg = incomingMessages_.front();
            messageHandler_(msg.topic, msg.payload);
        }
        incomingMessages_.pop();
    }
}

void MockTransport::setConnected(bool connected) {
    bool wasConnected = connected_;
    connected_ = connected;
    
    if (connectionHandler_ && wasConnected != connected) {
        connectionHandler_(connected, connected ? "Connected" : "Disconnected");
    }
}

void MockTransport::simulateConnectionLoss() {
    setConnected(false);
}

void MockTransport::simulateConnectionRestore() {
    setConnected(true);
}

void MockTransport::injectMessage(std::string_view topic, std::string_view payload) {
    MockMessage msg;
    msg.topic = std::string(topic);
    msg.payload = std::string(payload);
    msg.qos = 0;
    msg.timestamp = std::chrono::steady_clock::now();
    
    incomingMessages_.push(msg);
}

} // namespace tracker::sim