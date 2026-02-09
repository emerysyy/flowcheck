#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

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

// 测试单个流
void testFlow(const std::string& flowDir) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试流: " << fs::path(flowDir).filename().string() << std::endl;
    std::cout << "========================================" << std::endl;

    // 解析 context.txt
    FlowMetadata meta;
    if (!parseContextFile(flowDir + "/context.txt", meta)) {
        std::cerr << "无法解析 context.txt" << std::endl;
        return;
    }

    std::cout << "流信息:" << std::endl;
    std::cout << "  SessionId: " << meta.sessionId << std::endl;
    std::cout << "  进程: " << meta.procName << " (PID: " << meta.pid << ")" << std::endl;
    std::cout << "  源地址: " << meta.srcIP << ":" << meta.srcPort << std::endl;
    std::cout << "  目标地址: " << meta.dstIP << ":" << meta.dstPort << std::endl;
    std::cout << "  协议: " << (meta.isTCP ? "TCP" : "UDP") << std::endl;

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

    std::cout << "\n找到 " << txFiles.size() << " 个发送包, "
              << rxFiles.size() << " 个接收包" << std::endl;

    // 处理发送的数据包
    for (const auto& txFile : txFiles) {
        std::vector<uint8_t> data = readPacketFile(txFile);
        if (data.empty()) continue;

        std::cout << "\n处理 TX: " << fs::path(txFile).filename().string()
                  << " (" << data.size() << " 字节)" << std::endl;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        // 对于 DNS 查询，尝试获取响应
        if (ctx.flow_type == FlowType::UDP && meta.dstPort == 53) {
            PacketView response;
            if (engine.flowSend(ctx, pkt, response)) {
                std::cout << "  DNS 查询已缓存，返回响应 ("
                          << response.len << " 字节)" << std::endl;
            } else {
                std::cout << "  DNS 查询未缓存" << std::endl;
            }
        } else {
            engine.flowSend(ctx, pkt);
        }

        // 显示检测到的域名
        if (ctx.hasDomain()) {
            std::cout << "  提取域名: ";
            for (const auto& domain : ctx.domains) {
                std::cout << domain << " ";
            }
            std::cout << std::endl;
        }
    }

    // 处理接收的数据包
    for (const auto& rxFile : rxFiles) {
        std::vector<uint8_t> data = readPacketFile(rxFile);
        if (data.empty()) continue;

        std::cout << "\n处理 RX: " << fs::path(rxFile).filename().string()
                  << " (" << data.size() << " 字节)" << std::endl;

        PacketView pkt;
        pkt.data = data.data();
        pkt.len = data.size();

        engine.flowRecv(ctx, pkt);

        // 显示检测到的域名
        if (ctx.hasDomain()) {
            std::cout << "  提取域名: ";
            for (const auto& domain : ctx.domains) {
                std::cout << domain << " ";
            }
            std::cout << std::endl;
        }
    }

    // 显示 IP 到域名的映射（如果是 DNS 流）
    if (ctx.flow_type == FlowType::UDP && meta.dstPort == 53 && ctx.hasDomain()) {
        std::cout << "\n查询 IP 到域名映射:" << std::endl;
        // 显示当前域名
        for (const auto& domain : ctx.domains) {
            std::cout << "  域名: " << domain << std::endl;
        }
    }

    std::cout << "\n流处理完成" << std::endl;
}

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused))) {
    std::cout << "FlowCheck 数据测试程序" << std::endl;
    std::cout << "使用 data/ 目录下的真实流数据进行测试" << std::endl;

    std::string dataDir = "/Users//Documents/work/flowcheck/data";

    // 测试几个不同类型的流
    std::vector<std::string> testFlows = {
        dataDir + "/udp/103574652127166",  // DNS 流
        dataDir + "/tcp/103578995731791",  // HTTPS 流
        dataDir + "/tcp/103671170833666",  // HTTP 流（如果存在）
    };

    for (const auto& flowDir : testFlows) {
        if (fs::exists(flowDir)) {
            testFlow(flowDir);
        } else {
            std::cout << "\n流目录不存在: " << flowDir << std::endl;
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "所有测试完成" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
