#include "ssh_parser.hpp"
#include <cstring>

namespace proto {

bool SSH::isMessage(const uint8_t* data, size_t length) {
    if (length < 4) return false;

    const char* str = reinterpret_cast<const char*>(data);

    // SSH 握手以 "SSH-" 开头
    if (memcmp(str, "SSH-", 4) == 0) {
        // 验证版本格式: "SSH-2.0-" 或 "SSH-1.99-"
        if (length >= 8) {
            if ((memcmp(str + 4, "2.0-", 4) == 0) ||
                (memcmp(str + 4, "1.99", 4) == 0)) {
                return true;
            }
        }
    }

    // SSH 二进制包格式（握手后）
    if (length >= 6) {
        uint32_t packet_len = (data[0] << 24) | (data[1] << 16) |
                              (data[2] << 8) | data[3];
        uint8_t padding_len = data[4];

        // SSH 包长度检查
        if (packet_len > 35000 || packet_len < padding_len + 1) {
            return false;
        }

        // 填充长度必须 >= 4
        if (padding_len < 4) {
            return false;
        }

        return true;
    }

    return false;
}

bool SSH::isVersionString(const char* str, size_t length) {
    // SSH 版本字符串格式: "SSH-1.99-xxx" 或 "SSH-2.0-xxx"
    if (length < 4) return false;

    return (strncmp(str, "SSH-", 4) == 0);
}

bool SSH::isBinaryPacket(const uint8_t* data, size_t length) {
    if (length < 6) return false;

    // SSH 二进制包格式（握手后）
    uint32_t packet_len = (data[0] << 24) | (data[1] << 16) |
                          (data[2] << 8) | data[3];
    uint8_t padding_len = data[4];

    // SSH 包长度检查
    if (packet_len > 35000 || packet_len < padding_len + 1) {
        return false;
    }

    // 填充长度必须 >= 4
    if (padding_len < 4) {
        return false;
    }

    return true;
}

bool SSH::isSFTPPacket(const uint8_t* data, size_t length) {
    // SFTP 是 SSH 的子系统协议
    // SFTP 版本 3-6：通过 SSH exec 通道运行
    //
    // 难以从单个数据包区分，因为：
    // 1. SFTP 在 SSH 连接建立后运行
    // 2. SFTP 包使用 SSH 二进制包格式
    // 3. 需要会话状态跟踪来准确识别
    //
    // 当前实现返回 false，将 SFTP 归入 SSH 类型
    // TODO: 实现会话状态跟踪以准确识别 SFTP
    return false;
}

bool SSH::isSCPPacket(const uint8_t* data, size_t length) {
    // SCP 通过 SSH exec 通道运行
    // 检查 SCP 命令行特征
    //
    // SCP 命令格式:
    // scp [-v] [-P port] [-p] [-q] [-r] [-S] src... target
    //
    // 难以从单个包识别，因为：
    // 1. SCP 在 SSH 连接建立后运行
    // 2. 使用 SSH exec 通道，需要会话状态
    //
    // 当前实现返回 false，将 SCP 归入 SSH 类型
    // TODO: 实现会话状态跟踪以准确识别 SCP

    if (length < 4) return false;

    const char* str = reinterpret_cast<const char*>(data);

    // 简单检查 scp 命令开头
    if (str[0] == 's' && str[1] == 'c' && str[2] == 'p') {
        // 检查后面是空格或制表符
        if (length >= 4 && (str[3] == ' ' || str[3] == '\t')) {
            return true;
        }
    }

    return false;
}

bool SSH::matchesStandardPort(uint16_t port) {
    return port == 22 || port == 2222;
}

} // namespace proto
