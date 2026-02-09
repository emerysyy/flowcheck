#include "ftp_parser.hpp"
#include <cctype>
#include <cstring>

namespace proto {

bool FTP::isMessage(const uint8_t* data, size_t length) {
    if (length < 5) return false;

    const char* str = reinterpret_cast<const char*>(data);

    // 检查 3 位数字响应
    if (isResponse(str, length)) {
        return true;
    }

    // 检查 FTP 命令
    if (isCommand(str, length)) {
        return true;
    }

    return false;
}

bool FTP::isResponse(const char* str, size_t length) {
    if (length < 5) return false;

    // 检查是否为 3 位数字
    if (!std::isdigit(str[0]) ||
        !std::isdigit(str[1]) ||
        !std::isdigit(str[2])) {
        return false;
    }

    // 第 4 个字符必须是空格或连字符
    if (str[3] == ' ' || str[3] == '-') {
        return true;
    }

    return false;
}

bool FTP::isCommand(const char* str, size_t length) {
    if (length < 5) return false;

    // 检查前 4 个字符是否都是字母
    bool allAlpha = true;
    for (int i = 0; i < 4; i++) {
        if (!std::isalpha(static_cast<unsigned char>(str[i]))) {
            allAlpha = false;
            break;
        }
    }

    if (!allAlpha) {
        return false;
    }

    // 第 5 个字符必须是空格
    if (str[4] != ' ') {
        return false;
    }

    // 检查是否为已知 FTP 命令
    return isKnownCommand(str, length);
}

bool FTP::isKnownCommand(const char* str, size_t length) {
    // RFC 959 定义的 FTP 命令
    static const char* ftpCommands[] = {
        "USER", "PASS", "ACCT", "CWD",  "CDUP", "SMNT", "QUIT", "REIN",
        "PORT", "PASV", "TYPE", "STRU", "MODE", "RETR", "STOR", "APPE",
        "ALLO", "REST", "RNFR", "RNTO", "ABOR", "DELE", "RMD",  "MKD",
        "PWD",  "LIST", "NLST", "SITE", "SYST", "STAT", "HELP", "NOOP",
        nullptr
    };

    for (int i = 0; ftpCommands[i] != nullptr; i++) {
        if (strncmp(str, ftpCommands[i], 4) == 0) {
            return true;
        }
    }

    return false;
}

bool FTP::matchesStandardPort(uint16_t port) {
    return port == 21;
}

} // namespace proto
