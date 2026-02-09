#include "tls_parser.hpp"
#include <cstring>

namespace proto {

// Extension type for SNI
static constexpr uint16_t kExtensionServerName = 0x0000;

uint16_t TLS::read16(const uint8_t* p) const {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

uint32_t TLS::read24(const uint8_t* p) const {
    return (static_cast<uint32_t>(p[0]) << 16) |
           (static_cast<uint32_t>(p[1]) << 8) |
           p[2];
}

bool TLS::isRecord(const uint8_t* data, size_t length) {
    if (length < 5) return false;

    // TLS Record Layer 结构非常明确
    uint8_t content_type = data[0];
    uint8_t major = data[1];
    uint8_t minor = data[2];
    uint16_t record_len = read16(data + 3);

    // Content Type: 20-23, 24(heartbeat), 25(alert)
    // RFC 8446: 20(change_cipher_spec), 21(alert), 22(handshake), 23(application_data)
    // 历史版本支持: 24(heartbeat - RFC 6520)
    if (content_type < 20 || content_type > 25) {
        return false;
    }

    // Version: SSLv3(3,0), TLS1.0-1.2(3,1-3,3), TLS1.3(3,4)
    if (major != 3 || minor > 4) {
        return false;
    }

    // Length 检查：不能超过 2^14 (16384)
    if (record_len > 16384 || record_len == 0) {
        return false;
    }

    // 确保数据包包含完整的 record
    if (length < 5 + record_len) {
        return false;
    }

    return true;
}

bool TLS::parseRecordHeader(const uint8_t* data, size_t length,
                           TLSContentType& type, uint16_t& version,
                           uint16_t& recordLength) {
    if (length < 5) return false;

    type = static_cast<TLSContentType>(data[0]);
    version = read16(data + 1);
    recordLength = read16(data + 3);

    // 使用增强的验证
    uint8_t major = data[1];
    uint8_t minor = data[2];

    // Content Type: 20-25
    if (data[0] < 20 || data[0] > 25) return false;

    // Version: SSLv3(3,0), TLS1.0-1.3(3,1-3,4)
    if (major != 3 || minor > 4) return false;

    // Length 检查
    if (recordLength > 16384 || recordLength == 0) return false;
    if (length < 5 + recordLength) return false;

    return true;
}

bool TLS::isClientHello(const uint8_t* data, size_t length) {
    if (length < 5) return false;

    TLSContentType type;
    uint16_t version, recordLength;

    if (!parseRecordHeader(data, length, type, version, recordLength)) {
        return false;
    }

    if (type != TLSContentType::HANDSHAKE) {
        return false;
    }

    const uint8_t* handshakeData = data + 5;
    size_t handshakeLength = recordLength;

    if (handshakeLength < 4) return false;

    TLSHandshakeType msgType;
    uint32_t msgLength;

    if (!parseHandshakeHeader(handshakeData, handshakeLength, msgType, msgLength)) {
        return false;
    }

    return msgType == TLSHandshakeType::CLIENT_HELLO;
}

bool TLS::parseHandshakeHeader(const uint8_t* data, size_t length,
                              TLSHandshakeType& msgType,
                              uint32_t& msgLength) {
    if (length < 4) return false;

    msgType = static_cast<TLSHandshakeType>(data[0]);
    msgLength = read24(data + 1);

    return (4 + msgLength) <= length;
}

TLSResult TLS::parseSNI(const uint8_t* data, size_t length) {
    TLSResult result{false, ""};

    if (!isClientHello(data, length)) {
        return result;
    }

    const uint8_t* recordData = data + 5;
    size_t recordLength = read16(data + 3);

    if (recordLength < 4) return result;

    TLSHandshakeType msgType = static_cast<TLSHandshakeType>(recordData[0]);
    uint32_t handshakeLength = read24(recordData + 1);

    if (msgType != TLSHandshakeType::CLIENT_HELLO) {
        return result;
    }

    if (recordLength < 4 + handshakeLength) {
        return result;
    }

    const uint8_t* handshakeBody = recordData + 4;
    size_t bodyLength = handshakeLength;

    // Skip: version (2) + random (32)
    if (bodyLength < 34) return result;
    size_t offset = 34;

    // Skip session ID
    if (offset + 1 > bodyLength) return result;
    uint8_t sessionIdLength = handshakeBody[offset];
    offset += 1 + sessionIdLength;
    if (offset > bodyLength) return result;

    // Skip cipher suites
    if (offset + 2 > bodyLength) return result;
    uint16_t cipherSuitesLength = read16(handshakeBody + offset);
    offset += 2 + cipherSuitesLength;
    if (offset > bodyLength) return result;

    // Skip compression methods
    if (offset + 1 > bodyLength) return result;
    uint8_t compressionMethodsLength = handshakeBody[offset];
    offset += 1 + compressionMethodsLength;
    if (offset > bodyLength) return result;

    // Check if extensions are present
    if (offset >= bodyLength) return result;

    // Parse extensions
    if (offset + 2 > bodyLength) return result;
    uint16_t extensionsLength = read16(handshakeBody + offset);
    offset += 2;

    if (offset + extensionsLength > bodyLength) return result;

    const uint8_t* extensions = handshakeBody + offset;
    size_t remaining = extensionsLength;

    // Iterate through extensions
    while (remaining >= 4) {
        uint16_t extType = read16(extensions);
        uint16_t extLength = read16(extensions + 2);

        extensions += 4;
        remaining -= 4;

        if (extLength > remaining) break;

        if (extType == kExtensionServerName) {
            std::optional<std::string> sni = parseSNIExtension(extensions, extLength);
            if (sni.has_value() && !sni->empty()) {
                result.success = true;
                result.sni = *sni;
                return result;
            }
        }

        extensions += extLength;
        remaining -= extLength;
    }

    return result;
}

std::optional<std::string> TLS::parseSNIExtension(
    const uint8_t* extensions,
    size_t extensionsLength
) {
    if (extensionsLength < 2) return std::nullopt;

    uint16_t listLength = read16(extensions);
    size_t offset = 2;

    if (offset + listLength > extensionsLength) return std::nullopt;

    while (offset + 3 <= extensionsLength && offset < 2 + listLength) {
        uint8_t nameType = extensions[offset];
        uint16_t nameLength = read16(extensions + offset + 1);

        offset += 3;

        if (nameType == 0) { // Hostname
            if (offset + nameLength > extensionsLength) return std::nullopt;

            std::string name(reinterpret_cast<const char*>(extensions + offset), nameLength);

            bool valid = true;
            for (char c : name) {
                if (!(std::isalnum(static_cast<unsigned char>(c)) ||
                      c == '-' || c == '.')) {
                    valid = false;
                    break;
                }
            }

            if (valid && !name.empty()) {
                for (char& c : name) {
                    c = std::tolower(static_cast<unsigned char>(c));
                }
                return name;
            }
        }

        offset += nameLength;
    }

    return std::nullopt;
}

bool TLS::matchesStandardPort(uint16_t port) {
    // TLS/HTTPS 标准端口
    return port == 443 || port == 8443;
}

} // namespace proto
