#pragma once

#include "../ports/IEventBus.hpp"
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>

namespace tracker::domain {

class EventBus : public ports::IEventBus {
public:
    EventBus() = default;
    ~EventBus() override = default;

    void publish(const Event& event) override;
    void subscribe(EventType eventType, std::function<void(const Event&)> handler) override;
    void unsubscribe(EventType eventType) override;
    void processEvents() override;

private:
    std::unordered_map<EventType, std::vector<std::function<void(const Event&)>>> handlers_;
    std::queue<Event> eventQueue_;
    std::mutex queueMutex_;
    bool processing_ = false;
};

} // namespace tracker::domain