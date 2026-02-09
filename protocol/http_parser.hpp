#ifndef http_parser_hpp
#define http_parser_hpp

#include <string>
#include <cstdint>
#include <optional>

namespace proto {

struct HTTPResult {
    bool success;
    std::string host;
};

class HTTP {
public:
    /**
     * Check if data looks like an HTTP request
     */
    bool isRequest(const uint8_t* data, size_t length);

    /**
     * Check if data looks like an HTTP response
     */
    bool isResponse(const uint8_t* data, size_t length);

    /**
     * Extract Host header from HTTP request
     */
    HTTPResult parseHost(const uint8_t* data, size_t length);

    /**
     * Check if port matches HTTP standard ports
     */
    static bool matchesStandardPort(uint16_t port);

private:
    std::optional<std::string> extractHostHeader(const char* data, size_t length);
    std::string normalizeHost(std::string host);
};

} // namespace proto

#endif // http_parser_hpp
