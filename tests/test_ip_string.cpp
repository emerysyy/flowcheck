#include "flow/flow_defines.h"
#include <iostream>
#include <cassert>

using namespace flow;

int main() {
    std::cout << "FlowContext IP 字符串方法测试" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 测试 1: IPv4 地址
    {
        std::cout << "测试 1: IPv4 地址" << std::endl;
        FlowContext ctx;
        ctx.dst_ip = FlowIP::fromString("8.8.8.8");
        ctx.dst_port = 53;

        std::string ip_str = ctx.getIPString();
        std::string ip_raw = ctx.getIPStringRaw();

        std::cout << "  getIPString():    \"" << ip_str << "\"" << std::endl;
        std::cout << "  getIPStringRaw(): \"" << ip_raw << "\"" << std::endl;

        assert(ip_str == "8.8.8.8");
        assert(ip_raw == "8.8.8.8");
        std::cout << "  ✓ IPv4 测试通过\n" << std::endl;
    }

    // 测试 2: IPv6 地址
    {
        std::cout << "测试 2: IPv6 地址" << std::endl;
        FlowContext ctx;
        ctx.dst_ip = FlowIP::fromString("2001:4860:4860::8888");
        ctx.dst_port = 443;

        std::string ip_str = ctx.getIPString();
        std::string ip_raw = ctx.getIPStringRaw();

        std::cout << "  getIPString():    \"" << ip_str << "\"" << std::endl;
        std::cout << "  getIPStringRaw(): \"" << ip_raw << "\"" << std::endl;

        assert(ip_str == "[2001:4860:4860::8888]");
        assert(ip_raw == "2001:4860:4860::8888");
        std::cout << "  ✓ IPv6 测试通过\n" << std::endl;
    }

    // 测试 3: 缓存验证
    {
        std::cout << "测试 3: 缓存验证（多次调用返回相同结果）" << std::endl;
        FlowContext ctx;
        ctx.dst_ip = FlowIP::fromString("1.1.1.1");
        ctx.dst_port = 80;

        std::string ip1 = ctx.getIPString();
        std::string ip2 = ctx.getIPString();
        std::string ip3 = ctx.getIPStringRaw();
        std::string ip4 = ctx.getIPStringRaw();

        assert(ip1 == ip2);
        assert(ip3 == ip4);
        assert(ip1 == "1.1.1.1");
        assert(ip3 == "1.1.1.1");
        std::cout << "  ✓ 缓存测试通过\n" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "所有测试通过！" << std::endl;

    return 0;
}
