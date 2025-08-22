#pragma once

#include "../ports/ITransport.hpp"
#include <queue>
#include <vector>
#include <string>
#include <chrono>

namespace tracker::sim {

struct MockMessage {
    std::string topic;
    std::string payload;
    int qos;
    std::chrono::steady_clock::time_point timestamp;
};

class MockTransport : public ports::ITransport {
public:
    MockTransport();
    ~MockTransport() override = default;

    // ITransport interface
    bool connect(const ports::Credentials& credentials) override;
    void disconnect() override;
    bool isConnected() const override;

    bool publish(std::string_view topic, std::string_view payload, int qos = 0) override;
    bool subscribe(std::string_view topic, int qos = 0) override;

    void setMessageHandler(MessageHandler handler) override;
    void setConnectionHandler(ConnectionHandler handler) override;

    void processEvents() override;

    // Mock-specific methods for testing
    void setConnected(bool connected);
    void simulateConnectionLoss();
    void simulateConnectionRestore();
    void injectMessage(std::string_view topic, std::string_view payload);
    
    const std::vector<MockMessage>& getPublishedMessages() const { return publishedMessages_; }
    void clearPublishedMessages() { publishedMessages_.clear(); }
    
    bool shouldFailPublish() const { return failPublish_; }
    void setFailPublish(bool fail) { failPublish_ = fail; }

private:
    bool connected_ = false;
    bool failPublish_ = false;
    
    MessageHandler messageHandler_;
    ConnectionHandler connectionHandler_;
    
    std::vector<MockMessage> publishedMessages_;
    std::queue<MockMessage> incomingMessages_;
    std::vector<std::string> subscriptions_;
    
    ports::Credentials lastCredentials_;
};

} // namespace tracker::sim