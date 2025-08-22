#pragma once

#include <string>
#include <functional>
#include <chrono>

namespace tracker::ports {

struct Credentials {
    std::string host;
    int port;
    std::string clientId;
    std::string username;
    std::string password;
};

class ITransport {
public:
    virtual ~ITransport() = default;
    
    using MessageHandler = std::function<void(std::string_view topic, std::string_view payload)>;
    using ConnectionHandler = std::function<void(bool connected, std::string_view reason)>;
    
    virtual bool connect(const Credentials& credentials) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    virtual bool publish(std::string_view topic, std::string_view payload, int qos = 0) = 0;
    virtual bool subscribe(std::string_view topic, int qos = 0) = 0;
    
    virtual void setMessageHandler(MessageHandler handler) = 0;
    virtual void setConnectionHandler(ConnectionHandler handler) = 0;
    
    virtual void processEvents() = 0;
};

} // namespace tracker::ports