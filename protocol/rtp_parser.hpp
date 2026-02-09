#ifndef rtp_parser_hpp
#define rtp_parser_hpp

#include <cstdint>
#include <cstddef>

namespace proto {

/**
 * RTP/RTCP - Real-time Transport Protocol 解析器
 *
 * 功能:
 * - isRTPPacket() - 检测 RTP 包
 * - isRTCPPacket() - 检测 RTCP 包
 */
class RTP {
public:
    RTP() = default;
    ~RTP() = default;

    /**
     * 检测是否为 RTP 包
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 RTP 包
     */
    bool isRTPPacket(const uint8_t* data, size_t length);

    /**
     * 检测是否为 RTCP 包
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 RTCP 包
     */
    bool isRTCPPacket(const uint8_t* data, size_t length);

private:
    /**
     * 获取 RTP 版本号
     * 版本号在第一个字节的高2位
     */
    uint8_t getVersion(const uint8_t* data);

    /**
     * 获取 Payload Type
     * 去掉 marker bit
     */
    uint8_t getPayloadType(const uint8_t* data);

    /**
     * 检查是否为 RTCP payload type
     * RTCP payload type 范围: 192-207 (0xC0-0xCF)
     */
    bool isRTCPPayloadType(uint8_t payloadType);
};

} // namespace proto

#endif // rtp_parser_hpp
