#include "tftp_parser.hpp"

namespace proto {

bool TFTP::isMessage(const uint8_t* data, size_t length) {
    if (length < 2) return false;

    // TFTP 操作码（2字节，大端序）
    uint16_t opcode = (static_cast<uint16_t>(data[0]) << 8) | data[1];

    // 检查是否为有效的 TFTP 操作码
    return isValidOpcode(opcode);
}

bool TFTP::isValidOpcode(uint16_t opcode) {
    // RFC 1350 定义的 TFTP 操作码:
    // 1 - Read Request (RRQ)
    // 2 - Write Request (WRQ)
    // 3 - Data (DATA)
    // 4 - Acknowledgment (ACK)
    // 5 - Error (ERROR)
    // 6 - Option Acknowledgment (OACK)

    return (opcode >= 1 && opcode <= 6);
}

bool TFTP::matchesStandardPort(uint16_t port) {
    return port == 69;
}

} // namespace proto
