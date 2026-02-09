#include "smtp_parser.hpp"
#include <cctype>
#include <cstring>

namespace proto {

bool SMTP::isMessage(const uint8_t* data, size_t length) {
    if (length < 4) return false;

    const char* str = reinterpret_cast<const char*>(data);

    // 检查 SMTP 响应（3位数字）
    if (isResponse(str, length)) {
        return true;
    }

    // 检查 SMTP 命令
    if (isCommand(str, length)) {
        return true;
    }

    return false;
}

bool SMTP::isResponse(const char* str, size_t length) {
    if (length < 4) return false;

    // SMTP 响应格式: 3位数字 + 空格或连字符
    // 例如: "220 ", "550-"
    if (!std::isdigit(str[0]) ||
        !std::isdigit(str[1]) ||
        !std::isdigit(str[2])) {
        return false;
    }

    if (str[3] == ' ' || str[3] == '-') {
        return true;
    }

    return false;
}

bool SMTP::isCommand(const char* str, size_t length) {
    if (length < 4) return false;

    // SMTP 命令以字母开头
    if (!std::isalpha(str[0])) {
        return false;
    }

    // 检查是否为已知 SMTP 命令
    return isKnownCommand(str, length);
}

bool SMTP::isKnownCommand(const char* str, size_t length) {
    // RFC 5321 定义的 SMTP 命令
    static const char* smtpCommands[] = {
        "HELO", "EHLO", "MAIL", "RCPT", "DATA", "RSET",
        "NOOP", "QUIT", "VRFY", "EXPN", "HELP", "SEND",
        "SOML", "SAML", "TURN",
        // 扩展命令
        "AUTH", "STARTTLS",
        nullptr
    };

    for (int i = 0; smtpCommands[i] != nullptr; i++) {
        size_t cmdLen = strlen(smtpCommands[i]);

        if (length >= cmdLen) {
            // 使用不区分大小写的比较
            if (strncasecmp(str, smtpCommands[i], cmdLen) == 0) {
                // 检查命令后是空格或行尾
                if (length == cmdLen || str[cmdLen] == ' ' || str[cmdLen] == '\r') {
                    return true;
                }
            }
        }
    }

    return false;
}

bool SMTP::matchesStandardPort(uint16_t port) {
    return port == 25 || port == 587;
}

} // namespace proto
