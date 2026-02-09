#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include "flow/flow_dns.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;
using namespace flow;

// 解析 context.txt 文件
struct FlowMetadata {
    uint64_t sessionId;
    int pid;
    std::string procPath;
    std::string procName;
    std::string srcIP;
    int srcPort;
    std::string dstIP;
    int dstPort;
    bool isTCP;
};

bool parseContextFile(const std::string& path, FlowMetadata& meta) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行
        if (line.empty()) continue;

        // 解析键值对
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        // 去除值前后的空格
        size_t value_start = value.find_first_not_of(" \t");
        if (value_start != std::string::npos) {
            value = value.substr(value_start);
        }
        size_t value_end = value.find_last_not_of(" \t\r\n");
        if (value_end != std::string::npos) {
            value = value.substr(0, value_end + 1);
        }

        if (key == "sessionId") {
            meta.sessionId = std::stoull(value);
        } else if (key == "pid") {
            meta.pid = std::stoi(value);
        } else if (key == "procPath") {
            meta.procPath = value;
        } else if (key == "procName") {
            meta.procName = value;
        } else if (key == "srcIP") {
            meta.srcIP = value;
        } else if (key == "srcPort") {
            meta.srcPort = std::stoi(value);
        } else if (key == "dstIP") {
            meta.dstIP = value;
        } else if (key == "dstPort") {
            meta.dstPort = std::stoi(value);
        } else if (key == "isTCP") {
            meta.isTCP = (value == "YES");
        }
    }

    return true;
}

// 读取二进制数据包文件
std::vector<uint8_t> readPacketFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    return data;
}

// 统计信息
struct Statistics {
    int total_flows = 0;
    int dns_flows = 0;
    int tcp_flows = 0;
    int udp_flows = 0;
    int flows_with_domains = 0;
    int total_packets = 0;
    int total_tx_packets = 0;
    int total_rx_packets = 0;
    std::map<int, int> port_distribution;
    std::map<std::string, int> protocol_distribution;
};

// 测试单个流
bool testFlow(const std::string& flowDir, Statistics& stats, bool verbose = false) {
    // 解析 context.txt
    FlowMetadata meta;
    if (!parseContextFile(flowDir + "/context.txt", meta)) {
        return false;
    }

    stats.total_flows++;
    if (meta.isTCP) {
        stats.tcp_flows++;
    } else {
        stats.udp_flows++;
    }

    if (!meta.isTCP && meta.dstPort == 53) {
        stats.dns_flows++;
    }

    stats.port_distribution[meta.dstPort]++;

    if (verbose) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "流: " << fs::path(flowDir).filename().string() << std::endl;
        std::cout << "  进程: " << meta.procName << " (PID: " << meta.pid << ")" << std::endl;
        std::cout << "  目标: " << meta.dstIP << ":" << meta.dstPort << std::endl;
        std::cout << "  协议: " << (meta.isTCP ? "TCP" : "UDP") << std::endl;
    }

    // 创建 FlowContext
    FlowContext ctx;
    ctx.session_id = meta.sessionId;
    ctx.dst_ip = FlowIP::fromString(meta.dstIP);
    ctx.dst_port = meta.dstPort;
    ctx.flow_type = meta.isTCP ? FlowType::TCP : FlowType::UDP;
    ctx.pid = meta.pid;
    ctx.proc_name = meta.procName;
    ctx.proc_path = meta.procPath;

    // 获取 FlowEngine 单例
    FlowEngine& engine = FlowEngine::getInstance();

    // 收集所有数据包文件
    std::vector<std::string> txFiles, rxFiles;
    for (const auto& entry : fs::directory_iterator(flowDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("TX_") == 0 && filename.find(".bin") != std::string::npos) {
            txFiles.push_back(entry.path().string());
        } else if (filename.find("RX_") == 0 && filename.find(".bin") != std::string::npos) {
            rxFiles.push_back(entry.path().string());
        }
    }

    // 排序文件名以确保顺序
    std::sort(txFiles.begin(), txFiles.end());
    std::sort(rxFiles.begin(), rxFiles.end());

    stats.total_tx_packets += txFiles.size();
    stats.total_rx_packets += rxFiles.size();
    stats.total_packets += txFiles.size() + rxFiles.size();

    // 处理发送的数据包
    for (const auto& txFile : txFiles) {
        std::vector<uint8_t> data = readPacketFile(txFile);
        if (data.empty()) continue;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        // 对于 DNS 查询，尝试获取响应
        if (ctx.flow_type == FlowType::UDP && meta.dstPort == 53) {
            PacketView response;
            engine.flowSend(ctx, pkt, response);
        } else {
            engine.flowSend(ctx, pkt);
        }
    }

    // 处理接收的数据包
    for (const auto& rxFile : rxFiles) {
        std::vector<uint8_t> data = readPacketFile(rxFile);
        if (data.empty()) continue;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        engine.flowRecv(ctx, pkt);
    }

    // 统计域名提取
    if (ctx.hasDomain()) {
        stats.flows_with_domains++;

        if (verbose) {
            std::cout << "  提取域名: ";
            for (const auto& domain : ctx.domains) {
                std::cout << domain << " ";
            }
            std::cout << std::endl;
        }

        // 识别协议类型
        if (meta.dstPort == 53) {
            stats.protocol_distribution["DNS"]++;
        } else if (meta.dstPort == 443) {
            stats.protocol_distribution["HTTPS"]++;
        } else if (meta.dstPort == 80) {
            stats.protocol_distribution["HTTP"]++;
        } else {
            stats.protocol_distribution["Other"]++;
        }
    }

    return true;
}

int main() {
    std::cout << "FlowCheck 高级数据测试程序" << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::string dataDir = "/Users//Documents/work/flowcheck/data";
    Statistics stats;

    // 测试所有 UDP 流
    std::cout << "测试 UDP 流..." << std::endl;
    int udp_count = 0;
    for (const auto& entry : fs::directory_iterator(dataDir + "/udp")) {
        if (entry.is_directory()) {
            testFlow(entry.path().string(), stats, false);
            udp_count++;
            if (udp_count >= 50) break; // 限制测试数量
        }
    }

    // 测试所有 TCP 流
    std::cout << "测试 TCP 流..." << std::endl;
    int tcp_count = 0;
    for (const auto& entry : fs::directory_iterator(dataDir + "/tcp")) {
        if (entry.is_directory()) {
            testFlow(entry.path().string(), stats, false);
            tcp_count++;
            if (tcp_count >= 50) break; // 限制测试数量
        }
    }

    // 显示统计信息
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试统计" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总流数: " << stats.total_flows << std::endl;
    std::cout << "  TCP 流: " << stats.tcp_flows << std::endl;
    std::cout << "  UDP 流: " << stats.udp_flows << std::endl;
    std::cout << "  DNS 流: " << stats.dns_flows << std::endl;
    std::cout << "\n总数据包数: " << stats.total_packets << std::endl;
    std::cout << "  发送包: " << stats.total_tx_packets << std::endl;
    std::cout << "  接收包: " << stats.total_rx_packets << std::endl;
    std::cout << "\n成功提取域名的流: " << stats.flows_with_domains
              << " (" << (stats.flows_with_domains * 100.0 / stats.total_flows) << "%)" << std::endl;

    std::cout << "\n协议分布:" << std::endl;
    for (const auto& [protocol, count] : stats.protocol_distribution) {
        std::cout << "  " << protocol << ": " << count << std::endl;
    }

    std::cout << "\n端口分布 (Top 10):" << std::endl;
    std::vector<std::pair<int, int>> port_vec(stats.port_distribution.begin(),
                                                stats.port_distribution.end());
    std::sort(port_vec.begin(), port_vec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < std::min(size_t(10), port_vec.size()); i++) {
        std::cout << "  端口 " << port_vec[i].first << ": "
                  << port_vec[i].second << " 个流" << std::endl;
    }

    // 测试 DNS 缓存功能
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试 DNS 缓存功能" << std::endl;
    std::cout << "========================================" << std::endl;

    // 查找一个 DNS 流进行详细测试
    for (const auto& entry : fs::directory_iterator(dataDir + "/udp")) {
        if (!entry.is_directory()) continue;

        FlowMetadata meta;
        if (!parseContextFile(entry.path().string() + "/context.txt", meta)) continue;
        if (meta.dstPort != 53) continue;

        std::cout << "\n使用 DNS 流进行缓存测试: " << entry.path().filename().string() << std::endl;

        // 第一次处理
        Statistics test_stats;
        testFlow(entry.path().string(), test_stats, true);

        // 第二次处理（应该命中缓存）
        std::cout << "\n第二次处理（测试缓存）:" << std::endl;
        testFlow(entry.path().string(), test_stats, true);

        break; // 只测试一个
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "所有测试完成" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
