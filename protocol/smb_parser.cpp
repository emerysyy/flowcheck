#include "smb_parser.hpp"

namespace proto {

bool SMB::isMessage(const uint8_t* data, size_t length) {
    if (length < 4) return false;

    // 检查 NetBIOS Session Service header
    if (isNetBIOSHeader(data, length)) {
        return true;
    }

    // 检查 SMB 魔数
    if (isSMBMagic(data, length)) {
        return true;
    }

    return false;
}

bool SMB::isNetBIOSHeader(const uint8_t* data, size_t length) {
    if (length < 4) return false;

    // NetBIOS Session Service header 格式:
    // 0x00 0x00 0x00 xx (消息长度)
    // 前3个字节固定为0

    if (data[0] == 0x00 &&
        data[1] == 0x00 &&
        data[2] == 0x00) {
        return true;
    }

    return false;
}

bool SMB::isSMBMagic(const uint8_t* data, size_t length) {
    if (length < 4) return false;

    // SMB 魔数: 0xFF 'S' 'M' 'B'

    if (data[0] == 0xFF &&
        data[1] == 'S' &&
        data[2] == 'M' &&
        data[3] == 'B') {
        return true;
    }

    return false;
}

bool SMB::matchesStandardPort(uint16_t port) {
    return port == 445 || port == 139;
}

} // namespace proto
