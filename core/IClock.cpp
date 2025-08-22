#include "IClock.hpp"
#include <iomanip>
#include <sstream>

namespace tracker {

std::string SystemClock::iso8601() const {
    auto time = now();
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    
    // Use thread-safe gmtime_s on Windows, gmtime_r on other platforms
#ifdef _WIN32
    std::tm tm_buf{};
    if (gmtime_s(&tm_buf, &time_t) == 0) {
        ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    }
#else
    std::tm tm_buf{};
    if (gmtime_r(&time_t, &tm_buf)) {
        ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    }
#endif
    
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

} // namespace tracker