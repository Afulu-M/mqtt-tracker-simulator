#pragma once

#include <string>
#include <cstdint>

namespace tracker {

class SasToken {
public:
    struct Config {
        std::string host;
        std::string deviceId;
        std::string deviceKeyBase64;
        uint64_t expirySeconds = 3600;
    };
    
    static std::string generate(const Config& config);
    static std::string generate(const std::string& host, 
                               const std::string& deviceId,
                               const std::string& deviceKeyBase64,
                               uint64_t expiryEpochSeconds);
    
    static std::string urlEncode(const std::string& value);
    static std::string base64Decode(const std::string& encoded);
    static std::string base64Encode(const std::string& data);
    
private:
    static std::string hmacSha256(const std::string& key, const std::string& message);
    static std::string createStringToSign(const std::string& resourceUri, uint64_t expiry);
};

} // namespace tracker