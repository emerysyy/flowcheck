#ifndef tls_parser_hpp
#define tls_parser_hpp

#include <string>
#include <cstdint>
#include <optional>

namespace proto {

enum class TLSContentType : uint8_t {
    CHANGE_CIPHER_SPEC = 20,
    ALERT = 21,
    HANDSHAKE = 22,
    APPLICATION_DATA = 23
};

enum class TLSHandshakeType : uint8_t {
    CLIENT_HELLO = 1,
    SERVER_HELLO = 2,
    CERTIFICATE = 11
};

struct TLSResult {
    bool success;
    std::string sni;
};

class TLS {
public:
    /**
     * Extract SNI from TLS ClientHello message
     */
    TLSResult parseSNI(const uint8_t* data, size_t length);

    /**
     * Check if this is a TLS record
     */
    bool isRecord(const uint8_t* data, size_t length);

    /**
     * Check if this is a ClientHello handshake
     */
    bool isClientHello(const uint8_t* data, size_t length);

    /**
     * Check if port matches TLS standard ports
     */
    static bool matchesStandardPort(uint16_t port);

private:
    bool parseRecordHeader(const uint8_t* data, size_t length,
                          TLSContentType& type, uint16_t& version,
                          uint16_t& recordLength);

    bool parseHandshakeHeader(const uint8_t* data, size_t length,
                             TLSHandshakeType& msgType,
                             uint32_t& msgLength);

    std::optional<std::string> parseSNIExtension(
        const uint8_t* extensions,
        size_t extensionsLength
    );

    uint16_t read16(const uint8_t* p) const;
    uint32_t read24(const uint8_t* p) const;
};

} // namespace proto

#endif // tls_parser_hpp
