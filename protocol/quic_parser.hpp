#ifndef quic_parser_hpp
#define quic_parser_hpp

#include <cstdint>
#include <cstddef>

namespace proto {

/**
 * QUIC - QUIC Protocol 解析器
 *
 * 功能:
 * - isPacket() - 检测 QUIC 包
 */
class QUIC {
public:
    QUIC() = default;
    ~QUIC() = default;

    /**
     * 检测是否为 QUIC 包
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 QUIC 包
     */
    bool isPacket(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 QUIC 标准端口
     * @param port 端口号
     * @return true 如果端口是 443 或 8443
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为 QUIC Long Header
     * 高2位 = 1, 版本号非0
     */
    bool isLongHeader(const uint8_t* data, size_t length);

    /**
     * 检查是否为 QUIC Short Header
     * 高2位 = 0
     */
    bool isShortHeader(const uint8_t* data, size_t length);
};

} // namespace proto

#endif // quic_parser_hpp
