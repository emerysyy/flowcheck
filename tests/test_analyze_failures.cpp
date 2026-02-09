#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;
using namespace flow;

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
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

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

struct FlowResult {
    std::string flowId;
    FlowMetadata meta;
    bool hasDomain;
    int txCount;
    int rxCount;
    std::vector<std::string> domains;
};

FlowResult testFlow(const std::string& flowDir) {
    FlowResult result;
    result.flowId = fs::path(flowDir).filename().string();
    result.hasDomain = false;
    result.txCount = 0;
    result.rxCount = 0;

    // 解析 context.txt
    if (!parseContextFile(flowDir + "/context.txt", result.meta)) {
        return result;
    }

    // 创建 FlowContext
    FlowContext ctx;
    ctx.session_id = result.meta.sessionId;
    ctx.dst_ip = FlowIP::fromString(result.meta.dstIP);
    ctx.dst_port = result.meta.dstPort;
    ctx.flow_type = result.meta.isTCP ? FlowType::TCP : FlowType::UDP;
    ctx.pid = result.meta.pid;
    ctx.proc_name = result.meta.procName;
    ctx.proc_path = result.meta.procPath;

    FlowEngine& engine = FlowEngine::getInstance();

    // 收集数据包文件
    std::vector<std::string> txFiles, rxFiles;
    for (const auto& entry : fs::directory_iterator(flowDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("TX_") == 0 && filename.find(".bin") != std::string::npos) {
            txFiles.push_back(entry.path().string());
        } else if (filename.find("RX_") == 0 && filename.find(".bin") != std::string::npos) {
            rxFiles.push_back(entry.path().string());
        }
    }

    std::sort(txFiles.begin(), txFiles.end());
    std::sort(rxFiles.begin(), rxFiles.end());

    result.txCount = txFiles.size();
    result.rxCount = rxFiles.size();

    // 处理发送的数据包
    for (const auto& txFile : txFiles) {
        std::vector<uint8_t> data = readPacketFile(txFile);
        if (data.empty()) continue;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        if (ctx.flow_type == FlowType::UDP && result.meta.dstPort == 53) {
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

    // 检查是否提取到域名
    if (ctx.hasDomain()) {
        result.hasDomain = true;
        result.domains = ctx.domains;
    }

    return result;
}

int main() {
    std::cout << "FlowCheck 域名提取失败分析" << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::string dataDir = "/Users//Documents/work/flowcheck/data";

    std::vector<FlowResult> allResults;
    std::vector<FlowResult> failedResults;

    // 测试所有流
    std::cout << "分析 UDP 流..." << std::endl;
    int udp_count = 0;
    for (const auto& entry : fs::directory_iterator(dataDir + "/udp")) {
        if (entry.is_directory()) {
            auto result = testFlow(entry.path().string());
            allResults.push_back(result);
            if (!result.hasDomain) {
                failedResults.push_back(result);
            }
            udp_count++;
            if (udp_count >= 50) break;
        }
    }

    std::cout << "分析 TCP 流..." << std::endl;
    int tcp_count = 0;
    for (const auto& entry : fs::directory_iterator(dataDir + "/tcp")) {
        if (entry.is_directory()) {
            auto result = testFlow(entry.path().string());
            allResults.push_back(result);
            if (!result.hasDomain) {
                failedResults.push_back(result);
            }
            tcp_count++;
            if (tcp_count >= 50) break;
        }
    }

    // 统计分析
    std::cout << "\n========================================" << std::endl;
    std::cout << "总体统计" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总流数: " << allResults.size() << std::endl;
    std::cout << "成功提取域名: " << (allResults.size() - failedResults.size()) << std::endl;
    std::cout << "未提取到域名: " << failedResults.size() << std::endl;
    std::cout << "成功率: " << ((allResults.size() - failedResults.size()) * 100.0 / allResults.size()) << "%" << std::endl;

    // 分析失败原因
    std::cout << "\n========================================" << std::endl;
    std::cout << "未提取到域名的流分析" << std::endl;
    std::cout << "========================================" << std::endl;

    int no_packets = 0;
    int non_dns_udp = 0;
    int non_http_https_tcp = 0;
    std::map<int, int> failed_ports;

    for (const auto& result : failedResults) {
        failed_ports[result.meta.dstPort]++;

        if (result.txCount == 0 && result.rxCount == 0) {
            no_packets++;
        } else if (!result.meta.isTCP && result.meta.dstPort != 53) {
            non_dns_udp++;
        } else if (result.meta.isTCP && result.meta.dstPort != 80 && result.meta.dstPort != 443) {
            non_http_https_tcp++;
        }
    }

    std::cout << "\n失败原因分类:" << std::endl;
    std::cout << "  无数据包: " << no_packets << std::endl;
    std::cout << "  非 DNS 的 UDP 流: " << non_dns_udp << std::endl;
    std::cout << "  非 HTTP/HTTPS 的 TCP 流: " << non_http_https_tcp << std::endl;

    std::cout << "\n失败流的端口分布:" << std::endl;
    std::vector<std::pair<int, int>> port_vec(failed_ports.begin(), failed_ports.end());
    std::sort(port_vec.begin(), port_vec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& [port, count] : port_vec) {
        std::cout << "  端口 " << port << ": " << count << " 个流" << std::endl;
    }

    // 显示一些失败的详细示例
    std::cout << "\n========================================" << std::endl;
    std::cout << "失败流详细示例 (前 10 个)" << std::endl;
    std::cout << "========================================" << std::endl;

    for (size_t i = 0; i < std::min(size_t(10), failedResults.size()); i++) {
        const auto& result = failedResults[i];
        std::cout << "\n流 " << (i + 1) << ": " << result.flowId << std::endl;
        std::cout << "  进程: " << result.meta.procName << std::endl;
        std::cout << "  目标: " << result.meta.dstIP << ":" << result.meta.dstPort << std::endl;
        std::cout << "  协议: " << (result.meta.isTCP ? "TCP" : "UDP") << std::endl;
        std::cout << "  数据包: TX=" << result.txCount << ", RX=" << result.rxCount << std::endl;

        // 判断可能的原因
        if (result.txCount == 0 && result.rxCount == 0) {
            std::cout << "  原因: 没有数据包" << std::endl;
        } else if (!result.meta.isTCP && result.meta.dstPort != 53) {
            std::cout << "  原因: 非 DNS 的 UDP 流（端口 " << result.meta.dstPort << "）" << std::endl;
        } else if (result.meta.isTCP && result.meta.dstPort != 80 && result.meta.dstPort != 443) {
            std::cout << "  原因: 非 HTTP/HTTPS 的 TCP 流（端口 " << result.meta.dstPort << "）" << std::endl;
        } else {
            std::cout << "  原因: 数据包格式问题或协议解析失败" << std::endl;
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "分析完成" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
