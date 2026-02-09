#ifndef smb_parser_hpp
#define smb_parser_hpp

#include <cstdint>
#include <cstddef>

namespace proto {

/**
 * SMB - Server Message Block 解析器
 *
 * 功能:
 * - isMessage() - 检测 SMB 消息
 */
class SMB {
public:
    SMB() = default;
    ~SMB() = default;

    /**
     * 检测是否为 SMB 消息
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 SMB 消息
     */
    bool isMessage(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 SMB 标准端口
     * @param port 端口号
     * @return true 如果端口是 445 或 139
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为 NetBIOS Session Service header
     * 格式: 0x00 0x00 0x00 xx
     */
    bool isNetBIOSHeader(const uint8_t* data, size_t length);

    /**
     * 检查是否为 SMB 魔数
     * 格式: 0xFF 'S' 'M' 'B'
     */
    bool isSMBMagic(const uint8_t* data, size_t length);
};

} // namespace proto

#endif // smb_parser_hpp
