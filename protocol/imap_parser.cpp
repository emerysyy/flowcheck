#include "imap_parser.hpp"
#include <cctype>
#include <cstring>
#include <strings.h>

namespace proto {

bool IMAP::isMessage(const uint8_t* data, size_t length) {
    if (length < 3) return false;

    const char* str = reinterpret_cast<const char*>(data);

    // IMAP 响应以 '*' 开头
    if (isResponse(str, length)) {
        return true;
    }

    // 检查 IMAP 命令
    if (isCommand(str, length)) {
        return true;
    }

    return false;
}

bool IMAP::isResponse(const char* str, size_t length) {
    if (length < 3) return false;

    // IMAP 服务器响应以 '*' 开头
    // 例如: "* OK", "* BYE", "* LIST"
    if (str[0] == '*') {
        return true;
    }

    return false;
}

bool IMAP::isCommand(const char* str, size_t length) {
    if (length < 6) return false;

    // IMAP 命令格式: TAG COMMAND ...
    // TAG 可以是字母数字，COMMAND 是关键字

    // 第一个字符应该是字母数字（tag 的开始）
    if (!std::isalnum(static_cast<unsigned char>(str[0]))) {
        return false;
    }

    // 查找已知命令
    return isKnownCommand(str, length);
}

bool IMAP::isKnownCommand(const char* str, size_t length) {
    // RFC 3501 定义的 IMAP 命令
    static const char* imapCommands[] = {
        "LOGIN", "LOGOUT", "SELECT", "EXAMINE", "LIST", "LSUB",
        "STATUS", "SEARCH", "FETCH", "STORE", "COPY", "UID",
        "NOOP", "CHECK", "CLOSE", "EXPUNGE", "CREATE", "DELETE",
        "RENAME", "SUBSCRIBE", "UNSUBSCRIBE", "AUTHENTICATE",
        "STARTTLS", "IDLE", "ID", "UNSELECT",
        nullptr
    };

    for (int i = 0; imapCommands[i] != nullptr; i++) {
        size_t cmdLen = strlen(imapCommands[i]);

        if (length >= cmdLen) {
            // 使用不区分大小写的比较
            if (strncasecmp(str, imapCommands[i], cmdLen) == 0) {
                // 检查命令后是空格
                if (length > cmdLen && (str[cmdLen] == ' ' || str[cmdLen] == '\t')) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool IMAP::matchesStandardPort(uint16_t port) {
    return port == 143 || port == 993;
}

} // namespace proto
