#ifndef pop3_parser_hpp
#define pop3_parser_hpp

#include <cstdint>
#include <cstring>
#include <cstddef>

namespace proto {

/**
 * POP3 - Post Office Protocol v3 解析器
 *
 * 功能:
 * - isMessage() - 检测 POP3 消息（响应或命令）
 */
class POP3 {
public:
    POP3() = default;
    ~POP3() = default;

    /**
     * 检测是否为 POP3 消息
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 POP3 响应或命令
     */
    bool isMessage(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 POP3 标准端口
     * @param port 端口号
     * @return true 如果端口是 110 或 995
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为 POP3 响应
     * 格式: "+OK" 或 "-ERR"
     */
    bool isResponse(const char* str, size_t length);

    /**
     * 检查是否为 POP3 命令
     * 格式: 4字母命令
     */
    bool isCommand(const char* str, size_t length);

    /**
     * 检查是否为已知 POP3 命令
     */
    bool isKnownCommand(const char* str, size_t length);
};

} // namespace proto

#endif // pop3_parser_hpp
