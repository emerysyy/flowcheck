#include "flow/flow_engine.hpp"
#include "flow/flow_dns.hpp"
#include "dns/dns_message.hpp"
#include <iostream>
#include <vector>

using namespace flow;
using namespace dns;

// 构造一个包含 CNAME 的 DNS 响应
std::vector<uint8_t> buildDNSResponseWithCNAME() {
    std::vector<uint8_t> response;

    // DNS Header (12 bytes)
    // Transaction ID: 0x1234
    response.push_back(0x12);
    response.push_back(0x34);

    // Flags: 0x8180 (标准查询响应，无错误)
    response.push_back(0x81);
    response.push_back(0x80);

    // Questions: 1
    response.push_back(0x00);
    response.push_back(0x01);

    // Answer RRs: 3 (1 CNAME + 2 A records)
    response.push_back(0x00);
    response.push_back(0x03);

    // Authority RRs: 0
    response.push_back(0x00);
    response.push_back(0x00);

    // Additional RRs: 0
    response.push_back(0x00);
    response.push_back(0x00);

    // Question Section: www.baidu.com A?
    // 3www5baidu3com0
    response.push_back(3);
    response.insert(response.end(), {'w', 'w', 'w'});
    response.push_back(5);
    response.insert(response.end(), {'b', 'a', 'i', 'd', 'u'});
    response.push_back(3);
    response.insert(response.end(), {'c', 'o', 'm'});
    response.push_back(0);

    // Type: A (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // Class: IN (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // Answer 1: www.baidu.com CNAME www.a.shifen.com
    // Name: pointer to question (0xC00C)
    response.push_back(0xC0);
    response.push_back(0x0C);

    // Type: CNAME (5)
    response.push_back(0x00);
    response.push_back(0x05);

    // Class: IN (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // TTL: 10
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x0A);

    // RDLENGTH: 17 (3www1a6shifen3com0)
    response.push_back(0x00);
    response.push_back(0x11);

    // RDATA: www.a.shifen.com
    response.push_back(3);
    response.insert(response.end(), {'w', 'w', 'w'});
    response.push_back(1);
    response.push_back('a');
    response.push_back(6);
    response.insert(response.end(), {'s', 'h', 'i', 'f', 'e', 'n'});
    response.push_back(3);
    response.insert(response.end(), {'c', 'o', 'm'});
    response.push_back(0);

    // Answer 2: www.a.shifen.com A 183.2.172.177
    // Name: pointer to CNAME target (0xC02B)
    response.push_back(0xC0);
    response.push_back(0x2B);

    // Type: A (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // Class: IN (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // TTL: 10
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x0A);

    // RDLENGTH: 4
    response.push_back(0x00);
    response.push_back(0x04);

    // RDATA: 183.2.172.177
    response.push_back(183);
    response.push_back(2);
    response.push_back(172);
    response.push_back(177);

    // Answer 3: www.a.shifen.com A 183.2.172.17
    // Name: pointer to CNAME target (0xC02B)
    response.push_back(0xC0);
    response.push_back(0x2B);

    // Type: A (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // Class: IN (1)
    response.push_back(0x00);
    response.push_back(0x01);

    // TTL: 10
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x0A);

    // RDLENGTH: 4
    response.push_back(0x00);
    response.push_back(0x04);

    // RDATA: 183.2.172.17
    response.push_back(183);
    response.push_back(2);
    response.push_back(172);
    response.push_back(17);

    return response;
}

int main() {
    std::cout << "DNS CNAME 记录处理测试" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 获取 FlowEngine 实例
    FlowEngine& engine = FlowEngine::getInstance();
    DNSEngine& dns_engine = engine.getDNSEngine();

    // 清除缓存
    dns_engine.clearCache();

    // 创建 FlowContext
    FlowContext ctx;
    ctx.flow_type = FlowType::UDP;
    ctx.dst_port = 53;
    ctx.session_id = 12345;

    // 构造 DNS 响应
    auto response_data = buildDNSResponseWithCNAME();

    std::cout << "测试 DNS 响应解析（包含 CNAME）" << std::endl;
    std::cout << "DNS 响应结构:" << std::endl;
    std::cout << "  QUESTION: www.baidu.com A?" << std::endl;
    std::cout << "  ANSWER 1: www.baidu.com CNAME www.a.shifen.com" << std::endl;
    std::cout << "  ANSWER 2: www.a.shifen.com A 183.2.172.177" << std::endl;
    std::cout << "  ANSWER 3: www.a.shifen.com A 183.2.172.17\n" << std::endl;

    // 处理 DNS 响应
    PacketView pkt;
    pkt.data = response_data.data();
    pkt.len = response_data.size();

    engine.flowRecv(ctx, pkt);

    // 检查提取的域名
    std::cout << "提取的域名:" << std::endl;
    for (const auto& domain : ctx.domains) {
        std::cout << "  - " << domain << std::endl;
    }

    // 检查 IP-域名映射
    std::cout << "\nIP-域名映射:" << std::endl;

    auto domains1 = dns_engine.getDomainsForIP("183.2.172.177");
    std::cout << "  183.2.172.177 -> ";
    for (size_t i = 0; i < domains1.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << domains1[i];
    }
    std::cout << std::endl;

    auto domains2 = dns_engine.getDomainsForIP("183.2.172.17");
    std::cout << "  183.2.172.17 -> ";
    for (size_t i = 0; i < domains2.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << domains2[i];
    }
    std::cout << std::endl;

    // 验证结果
    std::cout << "\n========================================" << std::endl;

    bool has_baidu = false;
    bool has_shifen = false;

    for (const auto& domain : ctx.domains) {
        if (domain == "www.baidu.com") has_baidu = true;
        if (domain == "www.a.shifen.com") has_shifen = true;
    }

    if (has_baidu && has_shifen) {
        std::cout << "✓ CNAME 记录处理正确！" << std::endl;
        std::cout << "✓ 同时提取了原始域名和 CNAME 目标域名" << std::endl;
    } else {
        std::cout << "✗ CNAME 记录处理失败" << std::endl;
        if (!has_baidu) std::cout << "  缺少: www.baidu.com" << std::endl;
        if (!has_shifen) std::cout << "  缺少: www.a.shifen.com" << std::endl;
    }

    if (!domains1.empty() && !domains2.empty()) {
        std::cout << "✓ IP-域名映射建立成功" << std::endl;
    } else {
        std::cout << "✗ IP-域名映射建立失败" << std::endl;
    }

    return 0;
}
