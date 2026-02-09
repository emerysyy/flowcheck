//
//  flow_engine.cpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#include "flow_engine.hpp"
#include "flow_detector.hpp"
#include "flow_dns.hpp"
#include "../protocol/protocol.h"

#include <arpa/inet.h>
#include <cstring>

namespace flow {

// ===== FlowEngine =====

FlowEngine& FlowEngine::getInstance() {
    static FlowEngine instance;
    return instance;
}

FlowEngine::FlowEngine()
    : dns_engine_(std::make_unique<DNSEngine>()),
      detector_(std::make_unique<Detector>())
{
    // 后续可以注册更多解析器
}

FlowEngine::~FlowEngine() = default;

DNSEngine& FlowEngine::getDNSEngine() {
    return *dns_engine_;
}

void FlowEngine::flowArrive(FlowContext& ctx) {

    // DNS 流量
    {
        if (ctx.isDNS()) {
            // DNS 流量在 flowSend/flowRecv 中单独处理
            // 默认允许 DNS
            ctx.flow_decision = FlowDecision::Allow;
            ctx.path_decision = PathType::Local;
            return;
        }

    }

    // 普通流量
    {
        if (ctx.hasDomain()) {
            // 有域名信息，做出流量决策
            ctx.flow_decision = FlowDecision::Allow;
            // 做出路径决策
            ctx.path_decision = PathType::Local;
        } else {
            // 还没有域名，允许流量并等待协议检测
            ctx.flow_decision = FlowDecision::Allow;
            ctx.path_decision = PathType::Local;
        }
        return;
    }
}

void FlowEngine::flowOpen(FlowContext& ctx) {
    // 流量打开 - 如果需要可以初始化每个流量的状态
    // 目前只确保默认决策已设置
    if (ctx.flow_decision == FlowDecision::Block) {
        // 流量被阻止，无需进一步处理
        return;
    }
}

void FlowEngine::flowSend(FlowContext& ctx, const PacketView& pkt) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return;
    }

    // 处理 DNS 查询（不处理响应）
    if (ctx.isDNS()) {
        // 对于不需要响应处理的 DNS 流量，只提取域名
        static thread_local PacketView dummy_resp;
        dns_engine_->handleQuery(ctx, pkt, dummy_resp);
        return;
    }

    // 对于非 DNS 流量，尝试获取域名
    if (!ctx.hasDomain()) {
        // 1. 首先从 DNS 缓存中查询目标 IP 对应的域名
        if (ctx.dst_ip.isV4()) {
            // 使用缓存的 IP 字符串
            auto cached_domains = dns_engine_->getDomainsForIP(ctx.getIPStringRaw());
            if (!cached_domains.empty()) {
                // 从 DNS 缓存中找到了域名
                ctx.addDomains(cached_domains);

                // 现在有了域名，重新评估流量决策
                flowArrive(ctx);
                return;
            }
        }

        // 2. DNS 缓存中没找到，尝试从出站数据包中提取域名
        proto::ProtocolType protocol;
        auto domain = detector_->extractDomain(ctx, pkt, protocol);
        if (domain.has_value()) {
            std::vector<std::string> domains = {domain.value()};
            ctx.addDomains(domains);

            // 现在有了域名，重新评估流量决策
            flowArrive(ctx);
        }
    }
}

bool FlowEngine::flowSend(FlowContext& ctx, const PacketView& pkt, PacketView& response) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return false;
    }

    // 处理 DNS 查询并返回响应
    if (ctx.isDNS()) {
        // 尝试从缓存中处理 DNS 查询
        return dns_engine_->handleQuery(ctx, pkt, response);
    }

    // 对于非 DNS 流量，尝试获取域名
    if (!ctx.hasDomain()) {
        // 1. 首先从 DNS 缓存中查询目标 IP 对应的域名
        if (ctx.dst_ip.isV4()) {
            // 使用缓存的 IP 字符串
            auto cached_domains = dns_engine_->getDomainsForIP(ctx.getIPStringRaw());
            if (!cached_domains.empty()) {
                // 从 DNS 缓存中找到了域名
                ctx.addDomains(cached_domains);

                // 现在有了域名，重新评估流量决策
                flowArrive(ctx);
                return false;
            }
        }

        // 2. DNS 缓存中没找到，尝试从出站数据包中提取域名
        proto::ProtocolType protocol;
        auto domain = detector_->extractDomain(ctx, pkt, protocol);
        if (domain.has_value()) {
            std::vector<std::string> domains = {domain.value()};
            ctx.addDomains(domains);

            // 现在有了域名，重新评估流量决策
            flowArrive(ctx);
        }
    }

    return false;  // 非 DNS 流量没有响应
}

void FlowEngine::flowRecv(FlowContext& ctx, const PacketView& pkt) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return;
    }

    // 处理 DNS 响应
    if (ctx.isDNS()) {
        dns_engine_->handleResponse(ctx, pkt);
        return;
    }

    // 对于非 DNS 流量，尝试从入站数据包中提取域名
    if (!ctx.hasDomain()) {
        proto::ProtocolType protocol;

        auto domain = detector_->extractDomain(ctx, pkt, protocol);
        if (domain.has_value()) {
            std::vector<std::string> domains = {domain.value()};
            ctx.addDomains(domains);

            // 现在有了域名，重新评估流量决策
            flowArrive(ctx);
        }
    }
}

void FlowEngine::flowClose(FlowContext& ctx) {
    // 流量关闭 - 如果需要可以清理每个流量的状态
    // 目前无需处理
}



}
