#pragma once

#include "../Event.hpp"
#include <functional>
#include <typeinfo>
#include <memory>

namespace tracker::ports {

class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    template<typename EventType>
    using EventHandler = std::function<void(const EventType&)>;
    
    virtual void publish(const Event& event) = 0;
    virtual void subscribe(EventType eventType, std::function<void(const Event&)> handler) = 0;
    virtual void unsubscribe(EventType eventType) = 0;
    virtual void processEvents() = 0;
};

} // namespace tracker::ports