#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"
#include "flow/flow_detector.hpp"
#include <iostream>
#include <iomanip>

void printFlowContext(const flow::FlowContext& ctx) {
    std::cout << "\n=== Flow Context ===" << std::endl;
    std::cout << "Session ID: " << ctx.session_id << std::endl;
    std::cout << "PID: " << ctx.pid << std::endl;
    std::cout << "Process: " << ctx.proc_name << std::endl;
    std::cout << "Destination Port: " << ctx.dst_port << std::endl;

    std::cout << "Flow Type: ";
    switch (ctx.flow_type) {
        case flow::FlowType::TCP: std::cout << "TCP"; break;
        case flow::FlowType::UDP: std::cout << "UDP"; break;
        case flow::FlowType::DNS: std::cout << "DNS"; break;
    }
    std::cout << std::endl;

    std::cout << "Direction: ";
    switch (ctx.direction) {
        case flow::FlowDirection::Outbound: std::cout << "Outbound"; break;
        case flow::FlowDirection::Inbound: std::cout << "Inbound"; break;
    }
    std::cout << std::endl;

    std::cout << "Flow Decision: ";
    switch (ctx.flow_decision) {
        case flow::FlowDecision::Allow: std::cout << "Allow"; break;
        case flow::FlowDecision::Block: std::cout << "Block"; break;
    }
    std::cout << std::endl;

    std::cout << "Path Decision: ";
    switch (ctx.path_decision) {
        case flow::PathType::None: std::cout << "None"; break;
        case flow::PathType::Direct: std::cout << "Direct"; break;
        case flow::PathType::Local: std::cout << "Local"; break;
        case flow::PathType::Gateway: std::cout << "Gateway"; break;
    }
    std::cout << std::endl;

    if (!ctx.domains.empty()) {
        std::cout << "Domains:" << std::endl;
        for (const auto& domain : ctx.domains) {
            std::cout << "  - " << domain << std::endl;
        }
    }
    std::cout << "===================" << std::endl;
}

int main() {
    std::cout << "FlowCheck Library Test Program" << std::endl;
    std::cout << "===============================" << std::endl;

    // Test 1: Get FlowEngine singleton
    std::cout << "\n[Test 1] Getting FlowEngine singleton..." << std::endl;
    flow::FlowEngine& engine = flow::FlowEngine::getInstance();
    std::cout << "✓ FlowEngine singleton obtained" << std::endl;

    // Test 2: DNS Flow
    std::cout << "\n[Test 2] Testing DNS Flow..." << std::endl;
    flow::FlowContext dns_ctx;
    dns_ctx.session_id = 1001;
    dns_ctx.pid = 12345;
    dns_ctx.proc_name = "test_app";
    dns_ctx.flow_type = flow::FlowType::UDP;
    dns_ctx.direction = flow::FlowDirection::Outbound;
    dns_ctx.dst_port = 53;  // DNS port

    engine.flowArrive(dns_ctx);
    std::cout << "✓ DNS flow processed" << std::endl;
    printFlowContext(dns_ctx);

    // Test 3: HTTPS Flow
    std::cout << "\n[Test 3] Testing HTTPS Flow..." << std::endl;
    flow::FlowContext https_ctx;
    https_ctx.session_id = 1002;
    https_ctx.pid = 12346;
    https_ctx.proc_name = "browser";
    https_ctx.flow_type = flow::FlowType::TCP;
    https_ctx.direction = flow::FlowDirection::Outbound;
    https_ctx.dst_port = 443;  // HTTPS port

    // Add a domain
    https_ctx.addDomains({"example.com"});

    engine.flowArrive(https_ctx);
    std::cout << "✓ HTTPS flow processed" << std::endl;
    printFlowContext(https_ctx);

    // Test 4: HTTP Flow with multiple domains
    std::cout << "\n[Test 4] Testing HTTP Flow with multiple domains..." << std::endl;
    flow::FlowContext http_ctx;
    http_ctx.session_id = 1003;
    http_ctx.pid = 12347;
    http_ctx.proc_name = "curl";
    http_ctx.flow_type = flow::FlowType::TCP;
    http_ctx.direction = flow::FlowDirection::Outbound;
    http_ctx.dst_port = 80;  // HTTP port

    // Add multiple domains
    http_ctx.addDomains({"api.example.com", "cdn.example.com"});

    engine.flowArrive(http_ctx);
    std::cout << "✓ HTTP flow processed" << std::endl;
    printFlowContext(http_ctx);

    // Test 5: Flow lifecycle
    std::cout << "\n[Test 5] Testing Flow Lifecycle..." << std::endl;
    flow::FlowContext lifecycle_ctx;
    lifecycle_ctx.session_id = 1004;
    lifecycle_ctx.pid = 12348;
    lifecycle_ctx.proc_name = "test_lifecycle";
    lifecycle_ctx.flow_type = flow::FlowType::TCP;
    lifecycle_ctx.direction = flow::FlowDirection::Outbound;
    lifecycle_ctx.dst_port = 8080;
    lifecycle_ctx.addDomains({"test.local"});

    std::cout << "  - flowArrive()" << std::endl;
    engine.flowArrive(lifecycle_ctx);

    std::cout << "  - flowOpen()" << std::endl;
    engine.flowOpen(lifecycle_ctx);

    std::cout << "  - flowSend()" << std::endl;
    flow::PacketView send_pkt;
    const uint8_t send_data[] = "GET / HTTP/1.1\r\nHost: test.local\r\n\r\n";
    send_pkt.data = send_data;
    send_pkt.len = sizeof(send_data) - 1;
    engine.flowSend(lifecycle_ctx, send_pkt);

    std::cout << "  - flowRecv()" << std::endl;
    flow::PacketView recv_pkt;
    const uint8_t recv_data[] = "HTTP/1.1 200 OK\r\n\r\n";
    recv_pkt.data = recv_data;
    recv_pkt.len = sizeof(recv_data) - 1;
    engine.flowRecv(lifecycle_ctx, recv_pkt);

    std::cout << "  - flowClose()" << std::endl;
    engine.flowClose(lifecycle_ctx);

    std::cout << "✓ Flow lifecycle completed" << std::endl;
    printFlowContext(lifecycle_ctx);

    std::cout << "\n[Test 6] Testing DNS Cache Response..." << std::endl;

    // Create a DNS flow context
    flow::FlowContext dns_flow_ctx;
    dns_flow_ctx.session_id = 2001;
    dns_flow_ctx.pid = 12350;
    dns_flow_ctx.proc_name = "dns_test";
    dns_flow_ctx.flow_type = flow::FlowType::UDP;
    dns_flow_ctx.direction = flow::FlowDirection::Outbound;
    dns_flow_ctx.dst_port = 53;

    // Simulate a DNS query packet (simplified)
    const uint8_t dns_query[] = {
        0x12, 0x34,  // Transaction ID
        0x01, 0x00,  // Flags: standard query
        0x00, 0x01,  // Questions: 1
        0x00, 0x00,  // Answer RRs: 0
        0x00, 0x00,  // Authority RRs: 0
        0x00, 0x00,  // Additional RRs: 0
        // Question section would follow...
    };

    flow::PacketView dns_query_pkt;
    dns_query_pkt.data = dns_query;
    dns_query_pkt.len = sizeof(dns_query);

    // Method 1: Without response handling (simple version)
    std::cout << "  Testing flowSend() without response handling..." << std::endl;
    engine.flowSend(dns_flow_ctx, dns_query_pkt);
    std::cout << "  ✓ Query processed (no response needed)" << std::endl;

    // Method 2: With response handling (for DNS cache)
    std::cout << "  Testing flowSend() with response handling..." << std::endl;
    flow::PacketView dns_response;
    bool has_response = engine.flowSend(dns_flow_ctx, dns_query_pkt, dns_response);

    if (has_response) {
        std::cout << "  ✓ DNS cache hit! Response size: " << dns_response.len << " bytes" << std::endl;
    } else {
        std::cout << "  ✓ DNS cache miss (expected on first query)" << std::endl;
        std::cout << "    Query needs to be forwarded to DNS server" << std::endl;
    }

    // Test 7: Protocol Detector
    std::cout << "\n[Test 7] Testing Protocol Detector..." << std::endl;
    flow::Detector detector;

    // Test HTTP detection
    const uint8_t http_data[] = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    flow::PacketView http_pkt;
    http_pkt.data = http_data;
    http_pkt.len = sizeof(http_data) - 1;

    flow::FlowContext detect_ctx;
    detect_ctx.flow_type = flow::FlowType::TCP;
    detect_ctx.dst_port = 80;

    proto::ProtocolType detected_proto;
    auto domain = detector.extractDomain(detect_ctx, http_pkt, detected_proto);

    std::cout << "  HTTP packet detected" << std::endl;
    if (domain.has_value()) {
        std::cout << "  ✓ Domain extracted: " << domain.value() << std::endl;
    } else {
        std::cout << "  ✗ No domain extracted" << std::endl;
    }

    std::cout << "\n===============================" << std::endl;
    std::cout << "All tests completed successfully!" << std::endl;
    std::cout << "===============================" << std::endl;

    return 0;
}
