#ifndef imap_parser_hpp
#define imap_parser_hpp

#include <cstdint>
#include <cstring>
#include <cstddef>

namespace proto {

/**
 * IMAP - Internet Message Access Protocol 解析器
 *
 * 功能:
 * - isMessage() - 检测 IMAP 消息（响应或命令）
 */
class IMAP {
public:
    IMAP() = default;
    ~IMAP() = default;

    /**
     * 检测是否为 IMAP 消息
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 IMAP 响应或命令
     */
    bool isMessage(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 IMAP 标准端口
     * @param port 端口号
     * @return true 如果端口是 143 或 993
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为 IMAP 响应
     * IMAP 响应以 '*' 开头
     */
    bool isResponse(const char* str, size_t length);

    /**
     * 检查是否为 IMAP 命令
     * IMAP 命令格式: TAG COMMAND ...
     * TAG 可以是字母数字，COMMAND 是关键字
     */
    bool isCommand(const char* str, size_t length);

    /**
     * 检查是否为已知 IMAP 命令
     */
    bool isKnownCommand(const char* str, size_t length);
};

} // namespace proto

#endif // imap_parser_hpp
