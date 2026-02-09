#ifndef ssh_parser_hpp
#define ssh_parser_hpp

#include <cstdint>
#include <cstring>

namespace proto {

/**
 * SSH - Secure Shell 解析器
 *
 * 功能:
 * - isMessage() - 检测 SSH 消息
 * - isSFTPPacket() - 检测 SFTP 包（基于 SSH）
 * - isSCPPacket() - 检测 SCP 包（基于 SSH）
 */
class SSH {
public:
    SSH() = default;
    ~SSH() = default;

    /**
     * 检测是否为 SSH 消息
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 SSH 版本字符串或二进制包
     */
    bool isMessage(const uint8_t* data, size_t length);

    /**
     * 检测是否为 SFTP 包
     * SFTP 是 SSH 的子系统，难以从单个包准确识别
     */
    bool isSFTPPacket(const uint8_t* data, size_t length);

    /**
     * 检测是否为 SCP 包
     * SCP 通过 SSH exec 通道运行
     */
    bool isSCPPacket(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 SSH 标准端口
     * @param port 端口号
     * @return true 如果端口是 22 或 2222
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为 SSH 版本字符串
     * 格式: "SSH-1.99-xxx" 或 "SSH-2.0-xxx"
     */
    bool isVersionString(const char* str, size_t length);

    /**
     * 检查是否为 SSH 二进制包格式
     * 格式: 4字节长度前缀 + 数据
     */
    bool isBinaryPacket(const uint8_t* data, size_t length);
};

} // namespace proto

#endif // ssh_parser_hpp
