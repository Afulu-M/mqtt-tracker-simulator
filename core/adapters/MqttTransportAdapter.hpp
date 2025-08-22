#pragma once

#include "../ports/ITransport.hpp"
#include "../IMqttClient.hpp"
#include <memory>

namespace tracker::adapters {

class MqttTransportAdapter : public ports::ITransport {
public:
    explicit MqttTransportAdapter(std::shared_ptr<IMqttClient> mqttClient);
    ~MqttTransportAdapter() override = default;

    bool connect(const ports::Credentials& credentials) override;
    void disconnect() override;
    bool isConnected() const override;

    bool publish(std::string_view topic, std::string_view payload, int qos = 0) override;
    bool subscribe(std::string_view topic, int qos = 0) override;

    void setMessageHandler(MessageHandler handler) override;
    void setConnectionHandler(ConnectionHandler handler) override;

    void processEvents() override;

private:
    void onMqttMessage(const MqttMessage& message);
    void onMqttConnection(bool connected, const std::string& reason);

    std::shared_ptr<IMqttClient> mqttClient_;
    MessageHandler messageHandler_;
    ConnectionHandler connectionHandler_;
};

} // namespace tracker::adapters