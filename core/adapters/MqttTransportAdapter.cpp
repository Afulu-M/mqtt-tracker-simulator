#include "MqttTransportAdapter.hpp"

namespace tracker::adapters {

MqttTransportAdapter::MqttTransportAdapter(std::shared_ptr<IMqttClient> mqttClient)
    : mqttClient_(mqttClient) {
    
    mqttClient_->setMessageCallback([this](const MqttMessage& msg) {
        onMqttMessage(msg);
    });
    
    mqttClient_->setConnectionCallback([this](bool connected, const std::string& reason) {
        onMqttConnection(connected, reason);
    });
}

bool MqttTransportAdapter::connect(const ports::Credentials& credentials) {
    return mqttClient_->connect(credentials.host, credentials.port,
                              credentials.clientId, credentials.username, credentials.password);
}

void MqttTransportAdapter::disconnect() {
    mqttClient_->disconnect();
}

bool MqttTransportAdapter::isConnected() const {
    return mqttClient_->isConnected();
}

bool MqttTransportAdapter::publish(std::string_view topic, std::string_view payload, int qos) {
    return mqttClient_->publish(std::string(topic), std::string(payload), qos, false);
}

bool MqttTransportAdapter::subscribe(std::string_view topic, int qos) {
    return mqttClient_->subscribe(std::string(topic), qos);
}

void MqttTransportAdapter::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void MqttTransportAdapter::setConnectionHandler(ConnectionHandler handler) {
    connectionHandler_ = std::move(handler);
}

void MqttTransportAdapter::processEvents() {
    mqttClient_->processEvents();
}

void MqttTransportAdapter::onMqttMessage(const MqttMessage& message) {
    if (messageHandler_) {
        messageHandler_(message.topic, message.payload);
    }
}

void MqttTransportAdapter::onMqttConnection(bool connected, const std::string& reason) {
    if (connectionHandler_) {
        connectionHandler_(connected, reason);
    }
}

} // namespace tracker::adapters