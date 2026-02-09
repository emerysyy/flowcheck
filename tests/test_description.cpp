#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include <iostream>

using namespace flow;

int main() {
    std::cout << "FlowContext getDescription() 测试" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 获取 FlowEngine 单例
    FlowEngine& engine = FlowEngine::getInstance();

    // 测试 1: DNS 流
    {
        std::cout << "测试 1: DNS 流" << std::endl;
        FlowContext ctx;
        ctx.session_id = 12345;
        ctx.flow_type = FlowType::UDP;
        ctx.direction = FlowDirection::Outbound;
        ctx.dst_ip = FlowIP::fromString("8.8.8.8");
        ctx.dst_port = 53;
        ctx.pid = 1234;
        ctx.proc_name = "Chrome";
        ctx.addDomains({"www.google.com"});
        ctx.flow_decision = FlowDecision::Allow;

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    // 测试 2: HTTPS 流
    {
        std::cout << "测试 2: HTTPS 流" << std::endl;
        FlowContext ctx;
        ctx.session_id = 67890;
        ctx.flow_type = FlowType::TCP;
        ctx.direction = FlowDirection::Outbound;
        ctx.dst_ip = FlowIP::fromString("220.181.174.34");
        ctx.dst_port = 443;
        ctx.pid = 5678;
        ctx.proc_name = "Safari";
        ctx.addDomains({"www.baidu.com", "baidu.com"});
        ctx.flow_decision = FlowDecision::Allow;

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    // 测试 3: 被阻止的流
    {
        std::cout << "测试 3: 被阻止的流" << std::endl;
        FlowContext ctx;
        ctx.session_id = 99999;
        ctx.flow_type = FlowType::TCP;
        ctx.direction = FlowDirection::Outbound;
        ctx.dst_ip = FlowIP::fromString("1.2.3.4");
        ctx.dst_port = 80;
        ctx.pid = 9999;
        ctx.proc_name = "malware";
        ctx.addDomains({"bad.example.com"});
        ctx.flow_decision = FlowDecision::Block;

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    // 测试 4: 多个域名
    {
        std::cout << "测试 4: 多个域名" << std::endl;
        FlowContext ctx;
        ctx.session_id = 11111;
        ctx.flow_type = FlowType::TCP;
        ctx.direction = FlowDirection::Inbound;
        ctx.dst_ip = FlowIP::fromString("10.0.0.1");
        ctx.dst_port = 8080;
        ctx.addDomains({"domain1.com", "domain2.com", "domain3.com", "domain4.com"});

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    // 测试 5: 无域名的流
    {
        std::cout << "测试 5: 无域名的流" << std::endl;
        FlowContext ctx;
        ctx.session_id = 22222;
        ctx.flow_type = FlowType::TCP;
        ctx.direction = FlowDirection::Outbound;
        ctx.dst_ip = FlowIP::fromString("192.168.1.1");
        ctx.dst_port = 22;
        ctx.pid = 3333;
        ctx.proc_name = "ssh";

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    // 测试 6: IPv6 地址
    {
        std::cout << "测试 6: IPv6 地址" << std::endl;
        FlowContext ctx;
        ctx.session_id = 33333;
        ctx.flow_type = FlowType::TCP;
        ctx.direction = FlowDirection::Outbound;
        ctx.dst_ip = FlowIP::fromString("2001:4860:4860::8888");
        ctx.dst_port = 443;
        ctx.pid = 4444;
        ctx.proc_name = "Firefox";
        ctx.addDomains({"www.google.com"});

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    // 测试 7: IPv4-mapped IPv6 地址
    {
        std::cout << "测试 7: IPv4-mapped IPv6 地址" << std::endl;
        FlowContext ctx;
        ctx.session_id = 44444;
        ctx.flow_type = FlowType::TCP;
        ctx.direction = FlowDirection::Outbound;
        ctx.dst_ip = FlowIP::fromString("::ffff:192.168.1.1");
        ctx.dst_port = 80;
        ctx.pid = 5555;
        ctx.proc_name = "curl";

        std::cout << "描述: " << ctx.getDescription() << std::endl;
        std::cout << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "所有测试完成" << std::endl;

    return 0;
}
