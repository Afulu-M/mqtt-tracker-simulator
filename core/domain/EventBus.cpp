#include "EventBus.hpp"
#include <algorithm>

namespace tracker::domain {

void EventBus::publish(const Event& event) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    eventQueue_.push(event);
}

void EventBus::subscribe(EventType eventType, std::function<void(const Event&)> handler) {
    handlers_[eventType].push_back(std::move(handler));
}

void EventBus::unsubscribe(EventType eventType) {
    handlers_.erase(eventType);
}

void EventBus::processEvents() {
    if (processing_) return; // Prevent recursive processing
    
    processing_ = true;
    
    while (true) {
        Event event;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (eventQueue_.empty()) break;
            
            event = std::move(eventQueue_.front());
            eventQueue_.pop();
        }
        
        // Dispatch to handlers
        auto it = handlers_.find(event.eventType);
        if (it != handlers_.end()) {
            for (const auto& handler : it->second) {
                try {
                    handler(event);
                } catch (...) {
                    // Log error but continue processing other handlers
                }
            }
        }
    }
    
    processing_ = false;
}

} // namespace tracker::domain