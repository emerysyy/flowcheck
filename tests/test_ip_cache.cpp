#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include <iostream>
#include <chrono>

using namespace flow;

int main() {
    std::cout << "FlowContext IP 缓存测试" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 创建一个 FlowContext
    FlowContext ctx;
    ctx.session_id = 12345;
    ctx.flow_type = FlowType::TCP;
    ctx.direction = FlowDirection::Outbound;
    ctx.dst_ip = FlowIP::fromString("2001:4860:4860::8888");
    ctx.dst_port = 443;
    ctx.addDomains({"www.google.com"});
    ctx.proc_name = "Chrome";
    ctx.pid = 1234;

    std::cout << "测试：多次调用 getDescription() 验证缓存" << std::endl;
    std::cout << "第一次调用（会进行 IP 转换）:" << std::endl;
    auto start1 = std::chrono::high_resolution_clock::now();
    std::string desc1 = ctx.getDescription();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - start1).count();
    std::cout << desc1 << std::endl;
    std::cout << "耗时: " << duration1 << " 纳秒\n" << std::endl;

    std::cout << "第二次调用（使用缓存，不转换）:" << std::endl;
    auto start2 = std::chrono::high_resolution_clock::now();
    std::string desc2 = ctx.getDescription();
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();
    std::cout << desc2 << std::endl;
    std::cout << "耗时: " << duration2 << " 纳秒\n" << std::endl;

    std::cout << "第三次调用（使用缓存，不转换）:" << std::endl;
    auto start3 = std::chrono::high_resolution_clock::now();
    std::string desc3 = ctx.getDescription();
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::nanoseconds>(end3 - start3).count();
    std::cout << desc3 << std::endl;
    std::cout << "耗时: " << duration3 << " 纳秒\n" << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "性能对比:" << std::endl;
    std::cout << "  第一次: " << duration1 << " ns (包含 IP 转换)" << std::endl;
    std::cout << "  第二次: " << duration2 << " ns (使用缓存)" << std::endl;
    std::cout << "  第三次: " << duration3 << " ns (使用缓存)" << std::endl;

    if (duration2 < duration1 && duration3 < duration1) {
        std::cout << "\n✓ 缓存工作正常！后续调用更快" << std::endl;
    }

    return 0;
}
