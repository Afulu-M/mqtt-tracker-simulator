#include "../crypto/SasToken.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace tracker;

void testUrlEncode() {
    std::cout << "Testing URL encoding..." << std::endl;
    
    // Test basic encoding
    assert(SasToken::urlEncode("hello world") == "hello%20world");
    assert(SasToken::urlEncode("test@domain.com") == "test%40domain.com");
    assert(SasToken::urlEncode("safe-chars_123.~") == "safe-chars_123.~");
    
    std::cout << "URL encoding tests passed!" << std::endl;
}

void testBase64() {
    std::cout << "Testing Base64 encoding/decoding..." << std::endl;
    
    std::string original = "Hello, World!";
    std::string encoded = SasToken::base64Encode(original);
    std::string decoded = SasToken::base64Decode(encoded);
    
    assert(decoded == original);
    
    // Test known vector
    std::string knownText = "sure.";
    std::string knownEncoded = "c3VyZS4=";
    assert(SasToken::base64Encode(knownText) == knownEncoded);
    assert(SasToken::base64Decode(knownEncoded) == knownText);
    
    std::cout << "Base64 tests passed!" << std::endl;
}

void testSasTokenGeneration() {
    std::cout << "Testing SAS token generation..." << std::endl;
    
    // Test with known values
    std::string host = "test-hub.azure-devices.net";
    std::string deviceId = "test-device";
    std::string deviceKeyBase64 = "dGVzdGtleQ=="; // "testkey" in base64
    uint64_t expiry = 1234567890;
    
    std::string token = SasToken::generate(host, deviceId, deviceKeyBase64, expiry);
    
    // Verify token format
    assert(token.find("SharedAccessSignature sr=") == 0);
    assert(token.find("&sig=") != std::string::npos);
    assert(token.find("&se=" + std::to_string(expiry)) != std::string::npos);
    
    std::cout << "Generated token: " << token << std::endl;
    std::cout << "SAS token generation tests passed!" << std::endl;
}

void testSasTokenConfig() {
    std::cout << "Testing SAS token config..." << std::endl;
    
    SasToken::Config config;
    config.host = "test-hub.azure-devices.net";
    config.deviceId = "test-device";
    config.deviceKeyBase64 = "dGVzdGtleQ==";
    config.expirySeconds = 3600;
    
    std::string token = SasToken::generate(config);
    
    // Verify token format
    assert(token.find("SharedAccessSignature sr=") == 0);
    assert(token.find("&sig=") != std::string::npos);
    assert(token.find("&se=") != std::string::npos);
    
    std::cout << "SAS token config tests passed!" << std::endl;
}

int main() {
    std::cout << "Running SAS Token Tests..." << std::endl;
    
    try {
        testUrlEncode();
        testBase64();
        testSasTokenGeneration();
        testSasTokenConfig();
        
        std::cout << "\nAll tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}