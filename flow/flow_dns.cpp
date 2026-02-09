//
//  flow_dns.cpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#include "flow_dns.hpp"
#include "../dns/dns_message.hpp"
#include "../dns/dns_cache.hpp"
#include <algorithm>

namespace flow {

DNSEngine::DNSEngine()
    : dns_cache_(std::make_unique<dns::DNSResponseCache>(2048))
{
}

DNSEngine::~DNSEngine() = default;

bool DNSEngine::handleQuery(FlowContext &ctx, const PacketView &pkt, PacketView &resp) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return false;
    }

    // 解析 DNS 查询
    dns::DNSParser parser;
    dns::DNSMessage msg;

    if (!parser.parse(pkt.data, pkt.len, msg)) {
        return false;
    }

    // 从问题部分提取域名
    std::vector<std::string> domains;
    for (const auto& q : msg.questions) {
        if (!q.name.empty()) {
            domains.push_back(q.name);
        }
    }

    // 将域名添加到上下文
    ctx.addDomains(domains);

    // 尝试从缓存获取响应
    static thread_local std::vector<uint8_t> cached_response;
    cached_response.clear();

    if (dns_cache_->buildResponseFromCache(pkt.data, pkt.len, cached_response)) {
        // 缓存命中 - 返回缓存的响应
        resp.data = cached_response.data();
        resp.len = cached_response.size();
        return true;
    }

    // 缓存未命中
    return false;
}

void DNSEngine::handleResponse(FlowContext &ctx, const PacketView &pkt) {
    if (pkt.data == nullptr || pkt.len == 0) {
        return;
    }

    // DNS 响应最小长度（仅头部）
    if (pkt.len < 12) {
        return;
    }

    // 解析 DNS 响应
    dns::DNSParser parser;
    dns::DNSMessage msg;

    if (!parser.parse(pkt.data, pkt.len, msg)) {
        return;
    }

    // 检查是否确实是响应包（QR 位应该为 1）
    bool isResponse = (msg.header.flags & 0x8000) != 0;
    if (!isResponse) {
        return;  // 不是响应包
    }

    // 从问题和答案中提取域名
    std::vector<std::string> domains;
    std::vector<std::string> ip_addresses;

    // 添加问题域名
    for (const auto& q : msg.questions) {
        if (!q.name.empty()) {
            domains.push_back(q.name);
        }
    }

    // 处理答案记录
    for (const auto& ans : msg.answers) {
        // 添加记录名作为域名
        if (!ans.name.empty()) {
            domains.push_back(ans.name);
        }

        // 提取 IPv4 地址（A 记录）
        if (ans.type == static_cast<uint16_t>(dns::RecordType::A)) {
            auto ipv4 = ans.ipv4();
            if (ipv4.has_value()) {
                ip_addresses.push_back(ipv4.value());
            }
        }

        // 提取 IPv6 地址（AAAA 记录）
        if (ans.type == static_cast<uint16_t>(dns::RecordType::AAAA)) {
            auto ipv6 = ans.ipv6();
            if (ipv6.has_value()) {
                ip_addresses.push_back(ipv6.value());
            }
        }

        // 从 CNAME 记录提取域名
        if (ans.type == static_cast<uint16_t>(dns::RecordType::CNAME) && ans.domain.has_value()) {
            domains.push_back(ans.domain.value());
        }

        // 从 PTR 记录提取域名
        if (ans.type == static_cast<uint16_t>(dns::RecordType::PTR) && ans.domain.has_value()) {
            domains.push_back(ans.domain.value());
        }

        // 从 MX 记录提取域名
        if (ans.type == static_cast<uint16_t>(dns::RecordType::MX) && ans.mx.has_value()) {
            domains.push_back(ans.mx.value().exchange);
        }

        // 从 SRV 记录提取域名
        if (ans.type == static_cast<uint16_t>(dns::RecordType::SRV) && ans.srv.has_value()) {
            domains.push_back(ans.srv.value().target);
        }
    }

    // 用所有提取的域名更新上下文
    ctx.addDomains(domains);

    // 建立 IP-域名映射：此响应中的所有 IP 映射到所有域名
    // 这符合实际场景，DNS 响应中的信息是相关的
    if (!ip_addresses.empty() && !domains.empty()) {
        addIPsDomainsMappings(ip_addresses, domains);
    }

    // 仅当有有效 IP 地址时才存储响应到缓存
    // 这与 OC 代码的行为一致，只有 ipList.size() > 0 时才缓存
    if (!ip_addresses.empty()) {
        dns_cache_->storeResponse(pkt.data, pkt.len);
    }
}

std::vector<std::string> DNSEngine::getDomainsForIP(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(ip_domains_mutex_);

    auto it = ip_to_domains_.find(ip);
    if (it != ip_to_domains_.end()) {
        return it->second;
    }

    return {};  // 未找到则返回空
}

void DNSEngine::clearCache() {
    // 清除 DNS 响应缓存
    dns_cache_ = std::make_unique<dns::DNSResponseCache>(2048);

    // 清除 IP-域名映射
    std::lock_guard<std::mutex> lock(ip_domains_mutex_);
    ip_to_domains_.clear();
}

void DNSEngine::addIPDomainMapping(const std::string& ip, const std::string& domain) {
    std::lock_guard<std::mutex> lock(ip_domains_mutex_);

    auto& domains = ip_to_domains_[ip];

    // 检查域名是否已存在
    auto it = std::find(domains.begin(), domains.end(), domain);
    if (it == domains.end()) {
        domains.push_back(domain);
    }
}

void DNSEngine::addIPDomainMappings(const std::string& ip, const std::vector<std::string>& domains) {
    if (domains.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(ip_domains_mutex_);

    auto& existing_domains = ip_to_domains_[ip];

    // 添加所有域名，避免重复
    for (const auto& domain : domains) {
        if (domain.empty()) {
            continue;
        }

        auto it = std::find(existing_domains.begin(), existing_domains.end(), domain);
        if (it == existing_domains.end()) {
            existing_domains.push_back(domain);
        }
    }
}

void DNSEngine::addIPsDomainsMappings(const std::vector<std::string>& ips, const std::vector<std::string>& domains) {
    if (ips.empty() || domains.empty()) {
        return;
    }

    // 将所有 IP 映射到所有域名
    // 这表示 DNS 响应中的所有信息是相关的
    for (const auto& ip : ips) {
        if (!ip.empty()) {
            addIPDomainMappings(ip, domains);
        }
    }
}

}
