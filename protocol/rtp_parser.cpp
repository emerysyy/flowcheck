#include "rtp_parser.hpp"

namespace proto {

bool RTP::isRTPPacket(const uint8_t* data, size_t length) {
    if (length < 12) return false;

    // RTP 头部至少12字节
    // 第一个字节: V(2) P(1) X(1) CC(4)
    // V = Version (必须为2)

    uint8_t version = getVersion(data);

    // RTP 版本必须是 2
    if (version != 2) {
        return false;
    }

    // 检查是否不是 RTCP
    // RTCP 的 payload type 范围是 192-207
    uint8_t payloadType = getPayloadType(data);

    if (isRTCPPayloadType(payloadType)) {
        return false;  // 这是 RTCP 包
    }

    return true;
}

bool RTP::isRTCPPacket(const uint8_t* data, size_t length) {
    if (length < 8) return false;

    // RTCP 与 RTP 共享头部格式
    // RTCP 版本必须是 2

    uint8_t version = getVersion(data);

    if (version != 2) {
        return false;
    }

    // 检查 payload type 是否在 RTCP 范围内
    uint8_t payloadType = getPayloadType(data);

    if (isRTCPPayloadType(payloadType)) {
        return true;
    }

    return false;
}

uint8_t RTP::getVersion(const uint8_t* data) {
    // 版本号在第一个字节的高2位
    return data[0] >> 6;
}

uint8_t RTP::getPayloadType(const uint8_t* data) {
    // Payload Type 在第二个字节
    // 去掉 marker bit (bit 7)
    return data[1] & 0x7F;
}

bool RTP::isRTCPPayloadType(uint8_t payloadType) {
    // RTCP payload type 范围: 192-207 (0xC0-0xCF)
    // RFC 3550 定义:
    // 192 - SR (Sender Report)
    // 193 - RR (Receiver Report)
    // 194 - SDES (Source Description)
    // 195 - BYE (Goodbye)
    // 196 - APP (Application-specific)
    // 200-207 - 保留
    return (payloadType >= 192 && payloadType <= 207);
}

} // namespace proto
