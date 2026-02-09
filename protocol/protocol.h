//
//  protocol.h
//  
//
//  Created by jacky on 2026/2/9.
// 
//

#ifndef protocol_h
#define protocol_h


namespace proto {

// 协议类型枚举
enum class ProtocolType {
    Unknown,    // 未知协议

    // 基础协议
    DNS,        // DNS (UDP/TCP)
    HTTP,       // HTTP/1.x
    HTTPS,      // HTTPS (TLS 上的 HTTP)
    TLS,        // TLS/SSL
    TCP,        // 纯 TCP
    UDP,        // 纯 UDP

    // 核心协议
    FTP,        // File Transfer Protocol
    SSH,        // Secure Shell
    SMTP,       // Simple Mail Transfer Protocol

    // 邮件协议
    IMAP,       // Internet Message Access Protocol
    POP3,       // Post Office Protocol v3

    // 文件传输协议
    SFTP,       // SSH File Transfer Protocol
    SCP,        // Secure Copy
    SMB,        // Server Message Block
    TFTP,       // Trivial File Transfer Protocol

    // 实时通信协议
    QUIC,       // QUIC (UDP-based)
    RTP,        // Real-time Transport Protocol
    RTCP,       // Real-time Transport Control Protocol
};

}

#endif /* protocol_h */
