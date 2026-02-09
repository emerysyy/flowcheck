#include "flow/flow_engine.hpp"
#include "flow/flow_dns.hpp"
#include "flow/flow_detector.hpp"
#include "flow/flow_defines.h"
#include "protocol/protocol.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

using namespace flow;
namespace fs = std::filesystem;

struct FlowMetadata {
    uint64_t sessionId;
    uint32_t pid;
    std::string procPath;
    std::string procName;
    std::string srcIP;
    uint16_t srcPort;
    std::string dstIP;
    uint16_t dstPort;
    bool isTCP;
};

bool parseContextFile(const std::string& filepath, FlowMetadata& meta) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find(':');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // 去除前后空格
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        if (key == "sessionId") {
            meta.sessionId = std::stoull(value);
        } else if (key == "pid") {
            meta.pid = std::stoul(value);
        } else if (key == "procPath") {
            meta.procPath = value;
        } else if (key == "procName") {
            meta.procName = value;
        } else if (key == "srcIP") {
            meta.srcIP = value;
        } else if (key == "srcPort") {
            meta.srcPort = std::stoul(value);
        } else if (key == "dstIP") {
            meta.dstIP = value;
        } else if (key == "dstPort") {
            meta.dstPort = std::stoul(value);
        } else if (key == "isTCP") {
            meta.isTCP = (value == "YES");
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <流目录>" << std::endl;
        std::cout << "示例: " << argv[0] << " data/tcp/12345" << std::endl;
        return 1;
    }

    std::string flow_dir = argv[1];

    std::cout << "FlowCheck 域名提取诊断工具" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 解析 context.txt
    FlowMetadata meta;
    std::string context_file = flow_dir + "/context.txt";
    if (!parseContextFile(context_file, meta)) {
        std::cerr << "无法读取 context.txt" << std::endl;
        return 1;
    }

    std::cout << "流信息:" << std::endl;
    std::cout << "  SessionId: " << meta.sessionId << std::endl;
    std::cout << "  进程: " << meta.procName << " (PID: " << meta.pid << ")" << std::endl;
    std::cout << "  目标: " << meta.dstIP << ":" << meta.dstPort << std::endl;
    std::cout << "  协议: " << (meta.isTCP ? "TCP" : "UDP") << std::endl;
    std::cout << std::endl;

    // 创建 FlowContext
    FlowContext ctx;
    ctx.session_id = meta.sessionId;
    ctx.dst_ip = FlowIP::fromString(meta.dstIP);
    ctx.dst_port = meta.dstPort;
    ctx.flow_type = meta.isTCP ? FlowType::TCP : FlowType::UDP;
    ctx.pid = meta.pid;
    ctx.proc_name = meta.procName;
    ctx.direction = FlowDirection::Outbound;

    // 获取 FlowEngine
    FlowEngine& engine = FlowEngine::getInstance();
    DNSEngine& dns_engine = engine.getDNSEngine();

    // 清除缓存
    dns_engine.clearCache();

    // 处理所有 TX 和 RX 文件
    std::vector<std::string> tx_files, rx_files;

    for (const auto& entry : fs::directory_iterator(flow_dir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("TX_") == 0 && filename.find(".bin") != std::string::npos) {
            tx_files.push_back(entry.path().string());
        } else if (filename.find("RX_") == 0 && filename.find(".bin") != std::string::npos) {
            rx_files.push_back(entry.path().string());
        }
    }

    std::sort(tx_files.begin(), tx_files.end());
    std::sort(rx_files.begin(), rx_files.end());

    std::cout << "找到 " << tx_files.size() << " 个发送包, " << rx_files.size() << " 个接收包\n" << std::endl;

    // 处理 TX 包
    for (const auto& tx_file : tx_files) {
        std::ifstream file(tx_file, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

        std::cout << "处理 TX: " << fs::path(tx_file).filename().string()
                  << " (" << data.size() << " 字节)" << std::endl;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        // 检测协议
        proto::ProtocolType protocol;
        auto domain = engine.getDNSEngine().getDomainsForIP(ctx.getIPStringRaw());

        std::cout << "  DNS 缓存查询: ";
        if (!domain.empty()) {
            std::cout << "命中 - ";
            for (const auto& d : domain) {
                std::cout << d << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "未命中" << std::endl;
        }

        // 尝试从包中提取域名
        Detector detector;
        auto extracted = detector.extractDomain(ctx, pkt, protocol);

        std::cout << "  协议检测: " << static_cast<int>(protocol) << std::endl;
        std::cout << "  域名提取: ";
        if (extracted.has_value()) {
            std::cout << extracted.value() << std::endl;
        } else {
            std::cout << "无" << std::endl;
        }

        // 处理包
        engine.flowSend(ctx, pkt);

        std::cout << "  当前域名列表: ";
        for (const auto& d : ctx.domains) {
            std::cout << d << " ";
        }
        std::cout << std::endl << std::endl;
    }

    // 处理 RX 包
    for (const auto& rx_file : rx_files) {
        std::ifstream file(rx_file, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

        std::cout << "处理 RX: " << fs::path(rx_file).filename().string()
                  << " (" << data.size() << " 字节)" << std::endl;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        // 检测协议
        Detector detector;
        proto::ProtocolType protocol;
        auto extracted = detector.extractDomain(ctx, pkt, protocol);

        std::cout << "  协议检测: " << static_cast<int>(protocol) << std::endl;
        std::cout << "  域名提取: ";
        if (extracted.has_value()) {
            std::cout << extracted.value() << std::endl;
        } else {
            std::cout << "无" << std::endl;
        }

        // 处理包
        engine.flowRecv(ctx, pkt);

        std::cout << "  当前域名列表: ";
        for (const auto& d : ctx.domains) {
            std::cout << d << " ";
        }
        std::cout << std::endl << std::endl;
    }

    // 最终结果
    std::cout << "========================================" << std::endl;
    std::cout << "最终结果:" << std::endl;
    std::cout << ctx.getDescription() << std::endl;

    if (ctx.domains.empty()) {
        std::cout << "\n⚠️  未提取到任何域名！" << std::endl;
        std::cout << "\n可能的原因:" << std::endl;
        std::cout << "  1. DNS 查询/响应未被捕获" << std::endl;
        std::cout << "  2. TLS ClientHello 未被捕获或不包含 SNI" << std::endl;
        std::cout << "  3. HTTP 请求未被捕获或不包含 Host 头" << std::endl;
        std::cout << "  4. 使用了 QUIC 或其他加密协议" << std::endl;
    } else {
        std::cout << "\n✓ 成功提取域名" << std::endl;
    }

    return 0;
}
