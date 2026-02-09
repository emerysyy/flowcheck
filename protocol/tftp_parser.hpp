#ifndef tftp_parser_hpp
#define tftp_parser_hpp

#include <cstdint>
#include <cstddef>

namespace proto {

/**
 * TFTP - Trivial File Transfer Protocol 解析器
 *
 * 功能:
 * - isMessage() - 检测 TFTP 消息
 */
class TFTP {
public:
    TFTP() = default;
    ~TFTP() = default;

    /**
     * 检测是否为 TFTP 消息
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 TFTP 消息
     */
    bool isMessage(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 TFTP 标准端口
     * @param port 端口号
     * @return true 如果端口是 69
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为有效的 TFTP 操作码
     * 操作码范围: 1-6
     * 1=RRQ, 2=WRQ, 3=DATA, 4=ACK, 5=ERROR, 6=OACK
     */
    bool isValidOpcode(uint16_t opcode);
};

} // namespace proto

#endif // tftp_parser_hpp
