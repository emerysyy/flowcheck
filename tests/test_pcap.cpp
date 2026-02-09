#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <map>

using namespace flow;

// PCAP 文件头结构（24 字节）
struct PcapFileHeader {
    uint32_t magic;           // 魔数：0xa1b2c3d4
    uint16_t version_major;   // 主版本号
    uint16_t version_minor;   // 次版本号
    int32_t  thiszone;        // 时区修正
    uint32_t sigfigs;         // 时间戳精度
    uint32_t snaplen;         // 最大捕获长度
    uint32_t linktype;        // 链路类型
};

// PCAP 数据包头结构（16 字节）
struct PcapPacketHeader {
    uint32_t ts_sec;          // 时间戳（秒）
    uint32_t ts_usec;         // 时间戳（微秒）
    uint32_t incl_len;        // 捕获的数据包长度
    uint32_t orig_len;        // 原始数据包长度
};

// IP 头部结构（简化版）
struct IPHeader {
    uint8_t  version_ihl;     // 版本 (4 bits) + 头长度 (4 bits)
    uint8_t  tos;             // 服务类型
    uint16_t total_length;    // 总长度
    uint16_t identification;  // 标识
    uint16_t flags_fragment;  // 标志 + 片偏移
    uint8_t  ttl;             // 生存时间
    uint8_t  protocol;        // 协议
    uint16_t checksum;        // 校验和
    uint32_t src_ip;          // 源 IP
    uint32_t dst_ip;          // 目标 IP
};

// TCP 头部结构（简化版）
struct TCPHeader {
    uint16_t src_port;        // 源端口
    uint16_t dst_port;        // 目标端口
    uint32_t seq_num;         // 序列号
    uint32_t ack_num;         // 确认号
    uint8_t  data_offset;     // 数据偏移（高 4 位）
    uint8_t  flags;           // 标志
    uint16_t window;          // 窗口大小
    uint16_t checksum;        // 校验和
    uint16_t urgent_ptr;      // 紧急指针
};

// UDP 头部结构
struct UDPHeader {
    uint16_t src_port;        // 源端口
    uint16_t dst_port;        // 目标端口
    uint16_t length;          // 长度
    uint16_t checksum;        // 校验和
};

// HTTP 请求信息
struct HttpRequest {
    std::string method;           // GET, POST, etc.
    std::string path;             // /path/to/resource
    std::string version;          // HTTP/1.1
    std::string host;             // Host header
    std::string user_agent;       // User-Agent header
    std::string full_request;     // 完整请求（前 500 字节）
};

// TLS ClientHello 信息
struct TlsClientHello {
    std::string sni;              // Server Name Indication
    std::string version;          // TLS 版本
    std::vector<std::string> cipher_suites;  // 加密套件
};

// 流统计信息
struct FlowStats {
    std::string src_ip;
    uint16_t src_port;
    std::string dst_ip;
    uint16_t dst_port;
    std::string protocol;
    int packet_count;
    int total_bytes;
    std::vector<std::string> domains;
    std::vector<HttpRequest> http_requests;      // HTTP 请求列表
    std::vector<TlsClientHello> tls_hellos;      // TLS ClientHello 列表
};

// IP 地址转字符串
std::string ipToString(uint32_t ip) {
    char buf[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = ip;
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

// 解析 HTTP 请求
bool parseHttpRequest(const uint8_t* data, size_t len, HttpRequest& req) {
    if (len < 16) return false;  // 太短，不可能是 HTTP 请求

    // 检查是否以 HTTP 方法开头
    std::string start(reinterpret_cast<const char*>(data), std::min(len, size_t(10)));
    if (start.find("GET ") != 0 && start.find("POST ") != 0 &&
        start.find("PUT ") != 0 && start.find("HEAD ") != 0 &&
        start.find("DELETE ") != 0 && start.find("OPTIONS ") != 0 &&
        start.find("PATCH ") != 0 && start.find("CONNECT ") != 0) {
        return false;
    }

    // 转换为字符串（限制长度避免过大）
    size_t parse_len = std::min(len, size_t(2000));
    std::string request(reinterpret_cast<const char*>(data), parse_len);

    // 保存完整请求（前 500 字节）
    req.full_request = request.substr(0, std::min(size_t(500), request.length()));

    // 解析请求行
    size_t first_line_end = request.find("\r\n");
    if (first_line_end == std::string::npos) {
        first_line_end = request.find("\n");
    }

    if (first_line_end != std::string::npos) {
        std::string first_line = request.substr(0, first_line_end);

        // 解析方法、路径、版本
        size_t method_end = first_line.find(' ');
        if (method_end != std::string::npos) {
            req.method = first_line.substr(0, method_end);

            size_t path_start = method_end + 1;
            size_t path_end = first_line.find(' ', path_start);
            if (path_end != std::string::npos) {
                req.path = first_line.substr(path_start, path_end - path_start);
                req.version = first_line.substr(path_end + 1);
            }
        }
    }

    // 解析 Host 头
    size_t host_pos = request.find("Host: ");
    if (host_pos == std::string::npos) {
        host_pos = request.find("host: ");
    }
    if (host_pos != std::string::npos) {
        size_t host_start = host_pos + 6;
        size_t host_end = request.find("\r\n", host_start);
        if (host_end == std::string::npos) {
            host_end = request.find("\n", host_start);
        }
        if (host_end != std::string::npos) {
            req.host = request.substr(host_start, host_end - host_start);
        }
    }

    // 解析 User-Agent 头
    size_t ua_pos = request.find("User-Agent: ");
    if (ua_pos == std::string::npos) {
        ua_pos = request.find("user-agent: ");
    }
    if (ua_pos != std::string::npos) {
        size_t ua_start = ua_pos + 12;
        size_t ua_end = request.find("\r\n", ua_start);
        if (ua_end == std::string::npos) {
            ua_end = request.find("\n", ua_start);
        }
        if (ua_end != std::string::npos) {
            req.user_agent = request.substr(ua_start, ua_end - ua_start);
        }
    }

    return !req.method.empty();
}

// 解析 TLS ClientHello
bool parseTlsClientHello(const uint8_t* data, size_t len, TlsClientHello& hello) {
    if (len < 43) return false;  // TLS ClientHello 最小长度

    // 检查是否是 TLS Handshake (0x16)
    if (data[0] != 0x16) return false;

    // 检查 TLS 版本
    uint16_t version = (data[1] << 8) | data[2];
    if (version == 0x0301) hello.version = "TLS 1.0";
    else if (version == 0x0302) hello.version = "TLS 1.1";
    else if (version == 0x0303) hello.version = "TLS 1.2";
    else if (version == 0x0304) hello.version = "TLS 1.3";
    else hello.version = "Unknown";

    // 检查 Handshake Type (0x01 = ClientHello)
    if (data[5] != 0x01) return false;

    // 跳过到扩展部分查找 SNI
    // TLS 记录头 (5) + Handshake 头 (4) + Version (2) + Random (32) + Session ID Length (1)
    size_t offset = 5 + 4 + 2 + 32;
    if (offset >= len) return false;

    // Session ID Length
    uint8_t session_id_len = data[offset];
    offset += 1 + session_id_len;
    if (offset + 2 >= len) return false;

    // Cipher Suites Length
    uint16_t cipher_suites_len = (data[offset] << 8) | data[offset + 1];
    offset += 2 + cipher_suites_len;
    if (offset + 1 >= len) return false;

    // Compression Methods Length
    uint8_t compression_len = data[offset];
    offset += 1 + compression_len;
    if (offset + 2 >= len) return false;

    // Extensions Length
    uint16_t extensions_len = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    // 解析扩展
    size_t extensions_end = offset + extensions_len;
    while (offset + 4 <= extensions_end && offset + 4 <= len) {
        uint16_t ext_type = (data[offset] << 8) | data[offset + 1];
        uint16_t ext_len = (data[offset + 2] << 8) | data[offset + 3];
        offset += 4;

        if (offset + ext_len > len) break;

        // SNI 扩展 (type = 0)
        if (ext_type == 0 && ext_len > 5) {
            // SNI List Length (2) + SNI Type (1) + SNI Length (2)
            uint16_t sni_len = (data[offset + 3] << 8) | data[offset + 4];
            if (offset + 5 + sni_len <= len) {
                hello.sni = std::string(reinterpret_cast<const char*>(data + offset + 5), sni_len);
                return true;
            }
        }

        offset += ext_len;
    }

    return !hello.version.empty();
}

// 解析 PCAP 文件
class PcapParser {
private:
    std::ifstream file_;
    PcapFileHeader file_header_;
    FlowEngine& engine_;
    std::map<std::string, FlowStats> flows_;

public:
    PcapParser(const std::string& filename)
        : engine_(FlowEngine::getInstance()) {
        file_.open(filename, std::ios::binary);
        if (!file_.is_open()) {
            throw std::runtime_error("无法打开文件: " + filename);
        }

        // 读取文件头
        file_.read(reinterpret_cast<char*>(&file_header_), sizeof(file_header_));
        if (!file_.good()) {
            throw std::runtime_error("无法读取 PCAP 文件头");
        }

        // 检查魔数
        if (file_header_.magic != 0xa1b2c3d4 && file_header_.magic != 0xd4c3b2a1) {
            throw std::runtime_error("无效的 PCAP 文件格式");
        }

        std::cout << "PCAP 文件信息:" << std::endl;
        std::cout << "  版本: " << file_header_.version_major << "."
                  << file_header_.version_minor << std::endl;
        std::cout << "  链路类型: " << file_header_.linktype << std::endl;
        std::cout << "  最大捕获长度: " << file_header_.snaplen << std::endl;
    }

    void parse() {
        int packet_num = 0;

        while (file_.good() && !file_.eof()) {
            PcapPacketHeader pkt_header;
            file_.read(reinterpret_cast<char*>(&pkt_header), sizeof(pkt_header));

            if (file_.gcount() != sizeof(pkt_header)) {
                break;  // 文件结束
            }

            // 读取数据包数据
            std::vector<uint8_t> packet_data(pkt_header.incl_len);
            file_.read(reinterpret_cast<char*>(packet_data.data()), pkt_header.incl_len);

            if (file_.gcount() != static_cast<std::streamsize>(pkt_header.incl_len)) {
                std::cerr << "数据包 " << packet_num << " 读取不完整" << std::endl;
                break;
            }

            packet_num++;

            // 解析数据包
            parsePacket(packet_num, packet_data);
        }

        std::cout << "\n总共处理了 " << packet_num << " 个数据包" << std::endl;
    }

    void parsePacket(int packet_num, const std::vector<uint8_t>& data) {
        // BSD loopback 格式：前 4 字节是地址族
        if (data.size() < 4) {
            return;
        }

        // 跳过 loopback 头部（4 字节）
        size_t offset = 4;

        // 解析 IP 头部
        if (data.size() < offset + sizeof(IPHeader)) {
            return;
        }

        const IPHeader* ip_header = reinterpret_cast<const IPHeader*>(data.data() + offset);

        // 获取 IP 头部长度（IHL 字段，单位是 4 字节）
        uint8_t ip_header_len = (ip_header->version_ihl & 0x0F) * 4;
        offset += ip_header_len;

        std::string src_ip = ipToString(ip_header->src_ip);
        std::string dst_ip = ipToString(ip_header->dst_ip);
        uint8_t protocol = ip_header->protocol;

        // 解析传输层协议
        if (protocol == 6) {  // TCP
            parseTCP(packet_num, data, offset, src_ip, dst_ip);
        } else if (protocol == 17) {  // UDP
            parseUDP(packet_num, data, offset, src_ip, dst_ip);
        }
    }

    void parseTCP(int packet_num, const std::vector<uint8_t>& data,
                  size_t offset, const std::string& src_ip, const std::string& dst_ip) {
        if (data.size() < offset + sizeof(TCPHeader)) {
            return;
        }

        const TCPHeader* tcp_header = reinterpret_cast<const TCPHeader*>(data.data() + offset);

        uint16_t src_port = ntohs(tcp_header->src_port);
        uint16_t dst_port = ntohs(tcp_header->dst_port);

        // 获取 TCP 头部长度（数据偏移字段，单位是 4 字节）
        uint8_t tcp_header_len = (tcp_header->data_offset >> 4) * 4;
        offset += tcp_header_len;

        // 获取应用层数据
        if (offset < data.size()) {
            size_t payload_len = data.size() - offset;

            // 创建 FlowContext
            FlowContext ctx;
            ctx.dst_ip = FlowIP::fromString(dst_ip);
            ctx.dst_port = dst_port;
            ctx.flow_type = FlowType::TCP;
            ctx.session_id = (static_cast<uint64_t>(packet_num) << 32) | dst_port;

            // 创建 PacketView
            PacketView pkt;
            pkt.data = data.data() + offset;
            pkt.len = payload_len;

            // 处理数据包
            if (payload_len > 0) {
                engine_.flowSend(ctx, pkt);

                // 记录流统计
                std::string flow_key = src_ip + ":" + std::to_string(src_port) +
                                      " -> " + dst_ip + ":" + std::to_string(dst_port);

                auto& stats = flows_[flow_key];
                stats.src_ip = src_ip;
                stats.src_port = src_port;
                stats.dst_ip = dst_ip;
                stats.dst_port = dst_port;
                stats.protocol = "TCP";
                stats.packet_count++;
                stats.total_bytes += payload_len;

                // 尝试解析 HTTP 请求（不限端口）
                HttpRequest http_req;
                if (parseHttpRequest(pkt.data, pkt.len, http_req)) {
                    // 避免重复添加相同的请求
                    bool is_duplicate = false;
                    for (const auto& existing : stats.http_requests) {
                        if (existing.method == http_req.method &&
                            existing.path == http_req.path &&
                            existing.host == http_req.host) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (!is_duplicate) {
                        stats.http_requests.push_back(http_req);
                    }
                }

                // 尝试解析 TLS ClientHello（不限端口）
                TlsClientHello tls_hello;
                if (parseTlsClientHello(pkt.data, pkt.len, tls_hello)) {
                    // 避免重复添加相同的 ClientHello
                    bool is_duplicate = false;
                    for (const auto& existing : stats.tls_hellos) {
                        if (existing.sni == tls_hello.sni) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (!is_duplicate) {
                        stats.tls_hellos.push_back(tls_hello);
                    }
                }

                // 检查是否提取到域名
                if (ctx.hasDomain()) {
                    for (const auto& domain : ctx.domains) {
                        if (std::find(stats.domains.begin(), stats.domains.end(), domain)
                            == stats.domains.end()) {
                            stats.domains.push_back(domain);
                        }
                    }
                }
            }
        }
    }

    void parseUDP(int packet_num, const std::vector<uint8_t>& data,
                  size_t offset, const std::string& src_ip, const std::string& dst_ip) {
        if (data.size() < offset + sizeof(UDPHeader)) {
            return;
        }

        const UDPHeader* udp_header = reinterpret_cast<const UDPHeader*>(data.data() + offset);

        uint16_t src_port = ntohs(udp_header->src_port);
        uint16_t dst_port = ntohs(udp_header->dst_port);

        offset += sizeof(UDPHeader);

        // 获取应用层数据
        if (offset < data.size()) {
            size_t payload_len = data.size() - offset;

            // 创建 FlowContext
            FlowContext ctx;
            ctx.dst_ip = FlowIP::fromString(dst_ip);
            ctx.dst_port = dst_port;
            ctx.flow_type = FlowType::UDP;
            ctx.session_id = (static_cast<uint64_t>(packet_num) << 32) | dst_port;

            // 创建 PacketView
            PacketView pkt;
            pkt.data = data.data() + offset;
            pkt.len = payload_len;

            // 处理数据包（DNS 查询可能需要响应）
            if (dst_port == 53) {
                PacketView response;
                engine_.flowSend(ctx, pkt, response);
            } else {
                engine_.flowSend(ctx, pkt);
            }

            // 记录流统计
            std::string flow_key = src_ip + ":" + std::to_string(src_port) +
                                  " -> " + dst_ip + ":" + std::to_string(dst_port);

            auto& stats = flows_[flow_key];
            stats.src_ip = src_ip;
            stats.src_port = src_port;
            stats.dst_ip = dst_ip;
            stats.dst_port = dst_port;
            stats.protocol = "UDP";
            stats.packet_count++;
            stats.total_bytes += payload_len;

            // 检查是否提取到域名
            if (ctx.hasDomain()) {
                for (const auto& domain : ctx.domains) {
                    if (std::find(stats.domains.begin(), stats.domains.end(), domain)
                        == stats.domains.end()) {
                        stats.domains.push_back(domain);
                    }
                }
            }
        }
    }

    void printStats() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "流统计信息" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "总流数: " << flows_.size() << std::endl;

        int flows_with_domains = 0;
        int flows_with_http = 0;
        int flows_with_tls = 0;

        for (const auto& [key, stats] : flows_) {
            if (!stats.domains.empty()) {
                flows_with_domains++;
            }
            if (!stats.http_requests.empty()) {
                flows_with_http++;
            }
            if (!stats.tls_hellos.empty()) {
                flows_with_tls++;
            }
        }

        std::cout << "提取到域名的流: " << flows_with_domains << std::endl;
        std::cout << "HTTP 请求流: " << flows_with_http << std::endl;
        std::cout << "HTTPS/TLS 流: " << flows_with_tls << std::endl;

        // 显示 HTTP 请求详情
        if (flows_with_http > 0) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "HTTP 请求详情" << std::endl;
            std::cout << "========================================" << std::endl;

            for (const auto& [key, stats] : flows_) {
                if (!stats.http_requests.empty()) {
                    std::cout << "\n流: " << key << std::endl;
                    std::cout << "  数据包数: " << stats.packet_count << std::endl;
                    std::cout << "  总字节数: " << stats.total_bytes << std::endl;

                    for (size_t i = 0; i < stats.http_requests.size(); i++) {
                        const auto& req = stats.http_requests[i];
                        std::cout << "\n  HTTP 请求 #" << (i + 1) << ":" << std::endl;
                        std::cout << "    方法: " << req.method << std::endl;
                        std::cout << "    路径: " << req.path << std::endl;
                        std::cout << "    版本: " << req.version << std::endl;
                        if (!req.host.empty()) {
                            std::cout << "    Host: " << req.host << std::endl;
                        }
                        if (!req.user_agent.empty()) {
                            std::cout << "    User-Agent: " << req.user_agent << std::endl;
                        }
                        std::cout << "\n    完整请求头（前 500 字节）:" << std::endl;
                        std::cout << "    ----------------------------------------" << std::endl;
                        std::cout << req.full_request << std::endl;
                        std::cout << "    ----------------------------------------" << std::endl;
                    }
                }
            }
        }

        // 显示 HTTPS/TLS 请求详情
        if (flows_with_tls > 0) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "HTTPS/TLS ClientHello 详情" << std::endl;
            std::cout << "========================================" << std::endl;

            for (const auto& [key, stats] : flows_) {
                if (!stats.tls_hellos.empty()) {
                    std::cout << "\n流: " << key << std::endl;
                    std::cout << "  数据包数: " << stats.packet_count << std::endl;
                    std::cout << "  总字节数: " << stats.total_bytes << std::endl;

                    for (size_t i = 0; i < stats.tls_hellos.size(); i++) {
                        const auto& hello = stats.tls_hellos[i];
                        std::cout << "\n  TLS ClientHello #" << (i + 1) << ":" << std::endl;
                        std::cout << "    TLS 版本: " << hello.version << std::endl;
                        if (!hello.sni.empty()) {
                            std::cout << "    SNI (域名): " << hello.sni << std::endl;
                        } else {
                            std::cout << "    SNI: (未找到)" << std::endl;
                        }
                    }
                }
            }
        }

        // 显示提取到域名的流（通过 FlowEngine）
        if (flows_with_domains > 0) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "FlowEngine 提取的域名" << std::endl;
            std::cout << "========================================" << std::endl;

            for (const auto& [key, stats] : flows_) {
                if (!stats.domains.empty()) {
                    std::cout << "\n流: " << key << std::endl;
                    std::cout << "  协议: " << stats.protocol << std::endl;
                    std::cout << "  数据包数: " << stats.packet_count << std::endl;
                    std::cout << "  总字节数: " << stats.total_bytes << std::endl;
                    std::cout << "  域名: ";
                    for (const auto& domain : stats.domains) {
                        std::cout << domain << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }

        // 显示端口分布
        std::map<uint16_t, int> port_dist;
        for (const auto& [key, stats] : flows_) {
            port_dist[stats.dst_port]++;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "端口分布" << std::endl;
        std::cout << "========================================" << std::endl;
        for (const auto& [port, count] : port_dist) {
            std::string port_desc = "";
            if (port == 80) port_desc = " (HTTP)";
            else if (port == 443) port_desc = " (HTTPS)";
            else if (port == 53) port_desc = " (DNS)";
            else if (port == 8080) port_desc = " (HTTP-Alt)";
            std::cout << "  端口 " << port << port_desc << ": " << count << " 个流" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "FlowCheck PCAP 文件解析测试" << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::string pcap_file = "/tmp/req.pcap";

    // 如果提供了命令行参数，使用参数作为文件路径
    if (argc > 1) {
        pcap_file = argv[1];
    }

    try {
        PcapParser parser(pcap_file);

        std::cout << "\n开始解析数据包..." << std::endl;
        parser.parse();

        std::cout << "\n解析完成！" << std::endl;
        parser.printStats();

    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
