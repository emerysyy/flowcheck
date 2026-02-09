#include "flow/flow_dns.hpp"
#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include <iostream>
#include <iomanip>

// 简单的 DNS 响应构造器（用于测试）
std::vector<uint8_t> createSimpleDNSResponse(const std::string& domain, const std::string& ip) {
    std::vector<uint8_t> response;

    // DNS Header (12 bytes)
    response.push_back(0x12); response.push_back(0x34);  // Transaction ID
    response.push_back(0x81); response.push_back(0x80);  // Flags: response, no error
    response.push_back(0x00); response.push_back(0x01);  // Questions: 1
    response.push_back(0x00); response.push_back(0x01);  // Answers: 1
    response.push_back(0x00); response.push_back(0x00);  // Authority RRs: 0
    response.push_back(0x00); response.push_back(0x00);  // Additional RRs: 0

    // Question section
    // Domain name (simplified - just length + chars)
    for (const auto& part : {domain}) {
        size_t dot_pos = 0;
        size_t start = 0;
        while ((dot_pos = part.find('.', start)) != std::string::npos) {
            size_t len = dot_pos - start;
            response.push_back(static_cast<uint8_t>(len));
            for (size_t i = start; i < dot_pos; ++i) {
                response.push_back(part[i]);
            }
            start = dot_pos + 1;
        }
        // Last part
        size_t len = part.length() - start;
        response.push_back(static_cast<uint8_t>(len));
        for (size_t i = start; i < part.length(); ++i) {
            response.push_back(part[i]);
        }
    }
    response.push_back(0x00);  // End of name

    response.push_back(0x00); response.push_back(0x01);  // Type: A
    response.push_back(0x00); response.push_back(0x01);  // Class: IN

    // Answer section
    // Name (pointer to question)
    response.push_back(0xC0); response.push_back(0x0C);

    // Type: A, Class: IN
    response.push_back(0x00); response.push_back(0x01);
    response.push_back(0x00); response.push_back(0x01);

    // TTL: 300 seconds
    response.push_back(0x00); response.push_back(0x00);
    response.push_back(0x01); response.push_back(0x2C);

    // Data length: 4
    response.push_back(0x00); response.push_back(0x04);

    // IP address
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        size_t dot = ip.find('.', start);
        std::string octet = ip.substr(start, dot - start);
        response.push_back(static_cast<uint8_t>(std::stoi(octet)));
        start = dot + 1;
    }

    return response;
}

int main() {
    std::cout << "DNS Engine IP-Domains Mapping Test" << std::endl;
    std::cout << "===================================" << std::endl;

    // Get FlowEngine singleton and its DNS engine
    flow::FlowEngine& flow_engine = flow::FlowEngine::getInstance();
    flow::DNSEngine& dns_engine = flow_engine.getDNSEngine();

    // Test 1: Process DNS response and build IP-domain mapping
    std::cout << "\n[Test 1] Processing DNS responses..." << std::endl;

    struct TestCase {
        std::string domain;
        std::string ip;
    };

    std::vector<TestCase> test_cases = {
        {"example.com", "93.184.216.34"},
        {"google.com", "142.250.185.46"},
        {"github.com", "140.82.121.4"},
        {"example.com", "93.184.216.34"},  // Duplicate - should not add twice
    };

    for (const auto& tc : test_cases) {
        auto response_data = createSimpleDNSResponse(tc.domain, tc.ip);

        flow::FlowContext ctx;
        ctx.flow_type = flow::FlowType::UDP;
        ctx.dst_port = 53;

        flow::PacketView pkt;
        pkt.data = response_data.data();
        pkt.len = response_data.size();

        dns_engine.handleResponse(ctx, pkt);

        std::cout << "  ✓ Processed: " << tc.domain << " -> " << tc.ip << std::endl;
    }

    // Test 2: Query IP-domain mappings
    std::cout << "\n[Test 2] Querying IP-domain mappings..." << std::endl;

    std::vector<std::string> test_ips = {
        "93.184.216.34",
        "142.250.185.46",
        "140.82.121.4",
        "1.2.3.4",  // Not in cache
    };

    for (const auto& ip : test_ips) {
        auto domains = dns_engine.getDomainsForIP(ip);

        std::cout << "  IP: " << std::setw(15) << std::left << ip << " -> ";
        if (domains.empty()) {
            std::cout << "(not found)" << std::endl;
        } else {
            std::cout << "[";
            for (size_t i = 0; i < domains.size(); ++i) {
                std::cout << domains[i];
                if (i < domains.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }
    }

    // Test 3: Test multiple domains for one IP (simulating CNAME chain)
    std::cout << "\n[Test 3] Testing multiple domains for one IP..." << std::endl;
    std::cout << "  Simulating: www.example.com (CNAME) -> example.com (A) -> 93.184.216.35" << std::endl;

    // Create a more complex DNS response with CNAME
    // For simplicity, we'll just process multiple responses that should map to the same IP
    auto resp1 = createSimpleDNSResponse("www.example.com", "93.184.216.35");
    auto resp2 = createSimpleDNSResponse("example.com", "93.184.216.35");

    flow::FlowContext ctx1, ctx2;
    ctx1.flow_type = ctx2.flow_type = flow::FlowType::UDP;
    ctx1.dst_port = ctx2.dst_port = 53;

    flow::PacketView pkt1{resp1.data(), resp1.size()};
    flow::PacketView pkt2{resp2.data(), resp2.size()};

    dns_engine.handleResponse(ctx1, pkt1);
    dns_engine.handleResponse(ctx2, pkt2);

    auto domains_for_ip = dns_engine.getDomainsForIP("93.184.216.35");
    std::cout << "  IP: 93.184.216.35 -> [";
    for (size_t i = 0; i < domains_for_ip.size(); ++i) {
        std::cout << domains_for_ip[i];
        if (i < domains_for_ip.size() - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    if (domains_for_ip.size() >= 2) {
        std::cout << "  ✓ Multiple domains correctly mapped to one IP" << std::endl;
    }

    // Test 4: Clear cache
    std::cout << "\n[Test 4] Clearing cache..." << std::endl;
    dns_engine.clearCache();
    std::cout << "  ✓ Cache cleared" << std::endl;

    // Test 5: Verify cache is empty
    std::cout << "\n[Test 5] Verifying cache is empty..." << std::endl;
    auto domains_after_clear = dns_engine.getDomainsForIP("93.184.216.34");
    if (domains_after_clear.empty()) {
        std::cout << "  ✓ Cache is empty (as expected)" << std::endl;
    } else {
        std::cout << "  ✗ Cache still has data!" << std::endl;
    }

    std::cout << "\n===================================" << std::endl;
    std::cout << "All tests completed!" << std::endl;
    std::cout << "===================================" << std::endl;

    return 0;
}
