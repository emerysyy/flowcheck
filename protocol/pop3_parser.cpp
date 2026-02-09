#include "pop3_parser.hpp"
#include <cctype>
#include <cstring>

namespace proto {

bool POP3::isMessage(const uint8_t* data, size_t length) {
    if (length < 3) return false;

    const char* str = reinterpret_cast<const char*>(data);

    // 检查 POP3 响应
    if (isResponse(str, length)) {
        return true;
    }

    // 检查 POP3 命令
    if (isCommand(str, length)) {
        return true;
    }

    return false;
}

bool POP3::isResponse(const char* str, size_t length) {
    if (length < 3) return false;

    // POP3 响应格式:
    // +OK ... (成功)
    // -ERR ... (错误)

    if (strncmp(str, "+OK", 3) == 0) {
        return true;
    }

    if (length >= 4 && strncmp(str, "-ERR", 4) == 0) {
        return true;
    }

    return false;
}

bool POP3::isCommand(const char* str, size_t length) {
    if (length < 4) return false;

    // POP3 命令以字母开头
    if (!std::isalpha(str[0])) {
        return false;
    }

    // 检查是否为已知 POP3 命令
    return isKnownCommand(str, length);
}

bool POP3::isKnownCommand(const char* str, size_t length) {
    // RFC 1939 定义的 POP3 命令
    static const char* pop3Commands[] = {
        "USER", "PASS", "APOP", "STAT", "LIST", "RETR", "DELE",
        "NOOP", "RSET", "TOP", "UIDL", "QUIT", "CAPA", "AUTH",
        "STLS",
        nullptr
    };

    for (int i = 0; pop3Commands[i] != nullptr; i++) {
        // POP3 命令通常是 4 个字母（部分例外）
        size_t cmdLen = strlen(pop3Commands[i]);

        if (length >= cmdLen) {
            if (strncmp(str, pop3Commands[i], cmdLen) == 0) {
                // 检查命令后是空格或行尾
                if (length == cmdLen || str[cmdLen] == ' ' || str[cmdLen] == '\r') {
                    return true;
                }
            }
        }
    }

    return false;
}

bool POP3::matchesStandardPort(uint16_t port) {
    return port == 110 || port == 995;
}

} // namespace proto
