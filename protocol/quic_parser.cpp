#include "quic_parser.hpp"

namespace proto {

bool QUIC::isPacket(const uint8_t* data, size_t length) {
    if (length < 1) return false;

    // QUIC 包格式由第一个字节的高2位决定
    // - Long Header: 高2位 = 1 (0b11xxxxxx)
    // - Short Header: 高2位 = 0 (0b00xxxxxx)

    if (isLongHeader(data, length)) {
        return true;
    }

    if (isShortHeader(data, length)) {
        return true;
    }

    return false;
}

bool QUIC::isLongHeader(const uint8_t* data, size_t length) {
    if (length < 5) return false;

    // QUIC Long Header 格式:
    // 第一个字节: 1 Bit (Header Form) = 1, 7 bits (其他标志)
    // 高2位应该是 1 (0b11xxxxxx)

    uint8_t firstByte = data[0];
    uint8_t headerForm = firstByte >> 6;

    if (headerForm == 0x01 && length >= 5) {
        // Long Header 有版本号（4字节）
        // 版本号不能全为0（Reserved）
        if (data[1] != 0 || data[2] != 0 || data[3] != 0 || data[4] != 0) {
            return true;
        }
    }

    return false;
}

bool QUIC::isShortHeader(const uint8_t* data, size_t length) {
    if (length < 1) return false;

    // QUIC Short Header 格式:
    // 第一个字节: 1 Bit (Header Form) = 0, 7 bits (其他标志)
    // 高2位应该是 0 (0b00xxxxxx)

    uint8_t firstByte = data[0];
    uint8_t headerForm = firstByte >> 6;

    if (headerForm == 0x00) {
        return true;
    }

    return false;
}

bool QUIC::matchesStandardPort(uint16_t port) {
    return port == 443 || port == 8443;
}

} // namespace proto
