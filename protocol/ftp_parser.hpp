#ifndef ftp_parser_hpp
#define ftp_parser_hpp

#include <cstdint>
#include <cstring>

namespace proto {

/**
 * FTP - File Transfer Protocol 解析器
 *
 * 功能:
 * - isMessage() - 检测 FTP 消息（响应或命令）
 * - 支持控制连接检测
 */
class FTP {
public:
    FTP() = default;
    ~FTP() = default;

    /**
     * 检测是否为 FTP 消息
     * @param data 数据包数据
     * @param length 数据包长度
     * @return true 如果是 FTP 响应或命令
     */
    bool isMessage(const uint8_t* data, size_t length);

    /**
     * 检查端口是否为 FTP 标准端口
     * @param port 端口号
     * @return true 如果端口是 21
     */
    static bool matchesStandardPort(uint16_t port);

private:
    /**
     * 检查是否为 FTP 响应代码
     * 格式: 3位数字 + 空格或连字符
     */
    bool isResponse(const char* str, size_t length);

    /**
     * 检查是否为 FTP 命令
     * 格式: 4字母命令 + 空格
     */
    bool isCommand(const char* str, size_t length);

    /**
     * 检查是否为已知 FTP 命令
     */
    bool isKnownCommand(const char* str, size_t length);
};

} // namespace proto

#endif // ftp_parser_hpp
