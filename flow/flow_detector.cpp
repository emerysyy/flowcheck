//
//  flow_detector.cpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#include "flow_detector.hpp"
#include "../protocol/http_parser.hpp"
#include "../protocol/tls_parser.hpp"
#include "../protocol/ssh_parser.hpp"
#include "../protocol/ftp_parser.hpp"
#include "../protocol/smtp_parser.hpp"
#include "../protocol/imap_parser.hpp"
#include "../protocol/pop3_parser.hpp"
#include "../protocol/quic_parser.hpp"

namespace flow {

proto::ProtocolType Detector::detectProtocol(const FlowContext &ctx, const PacketView &pkt) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return proto::ProtocolType::Unknown;
    }

    // DNS 检测（基于端口）
    if (ctx.isDNS()) {
        return proto::ProtocolType::DNS;
    }

    // TLS 检测
    proto::TLS tls;
    if (tls.isRecord(pkt.data, pkt.len)) {
        if (tls.isClientHello(pkt.data, pkt.len)) {
            return proto::ProtocolType::TLS;
        }
        return proto::ProtocolType::TLS;
    }

    // HTTP 检测
    proto::HTTP http;
    if (http.isRequest(pkt.data, pkt.len) || http.isResponse(pkt.data, pkt.len)) {
        return proto::ProtocolType::HTTP;
    }

    // QUIC 检测
    proto::QUIC quic;
    if (ctx.flow_type == FlowType::UDP && quic.isPacket(pkt.data, pkt.len)) {
        return proto::ProtocolType::QUIC;
    }

    // SSH 检测
    proto::SSH ssh;
    if (ssh.isMessage(pkt.data, pkt.len)) {
        return proto::ProtocolType::SSH;
    }

    // FTP 检测
    proto::FTP ftp;
    if (ftp.isMessage(pkt.data, pkt.len)) {
        return proto::ProtocolType::FTP;
    }

    // SMTP 检测
    proto::SMTP smtp;
    if (smtp.isMessage(pkt.data, pkt.len)) {
        return proto::ProtocolType::SMTP;
    }

    // IMAP 检测
    proto::IMAP imap;
    if (imap.isMessage(pkt.data, pkt.len)) {
        return proto::ProtocolType::IMAP;
    }

    // POP3 检测
    proto::POP3 pop3;
    if (pop3.isMessage(pkt.data, pkt.len)) {
        return proto::ProtocolType::POP3;
    }

    // 根据流量类型默认为 TCP/UDP
    if (ctx.flow_type == FlowType::TCP) {
        return proto::ProtocolType::TCP;
    } else if (ctx.flow_type == FlowType::UDP) {
        return proto::ProtocolType::UDP;
    }

    return proto::ProtocolType::Unknown;
}

std::optional<std::string> Detector::extractDomain(const FlowContext &ctx, const PacketView &pkt, proto::ProtocolType &protocol) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return std::nullopt;
    }

    // 首先检测协议
    protocol = detectProtocol(ctx, pkt);

    // 根据协议提取域名
    switch (protocol) {
        case proto::ProtocolType::HTTP: {
            proto::HTTP http;
            auto result = http.parseHost(pkt.data, pkt.len);
            if (result.success && !result.host.empty()) {
                return result.host;
            }
            break;
        }

        case proto::ProtocolType::TLS:
        case proto::ProtocolType::HTTPS: {
            proto::TLS tls;
            auto result = tls.parseSNI(pkt.data, pkt.len);
            if (result.success && !result.sni.empty()) {
                return result.sni;
            }
            break;
        }

        default:
            break;
    }

    return std::nullopt;
}

}
