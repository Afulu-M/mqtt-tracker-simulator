#pragma once

#include <chrono>
#include <cstdint>

namespace tracker {

class IClock {
public:
    virtual ~IClock() = default;
    
    virtual std::chrono::system_clock::time_point now() const = 0;
    virtual uint64_t epochSeconds() const = 0;
    virtual std::string iso8601() const = 0;
};

class SystemClock : public IClock {
public:
    std::chrono::system_clock::time_point now() const override {
        return std::chrono::system_clock::now();
    }
    
    uint64_t epochSeconds() const override {
        return std::chrono::duration_cast<std::chrono::seconds>(
            now().time_since_epoch()).count();
    }
    
    std::string iso8601() const override;
};

} // namespace tracker