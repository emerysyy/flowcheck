//
//  flow_dns.hpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#ifndef flow_dns_hpp
#define flow_dns_hpp

#include <stdio.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include "flow_defines.h"

// 前向声明
namespace dns {
    class DNSResponseCache;
}

namespace flow {

/**
 * DNS 引擎，用于处理 DNS 查询和响应
 *
 * 功能：
 * - DNS 响应缓存（查询 -> 响应）
 * - IP 到域名映射（ip -> domains）
 * - 线程安全操作
 */
class DNSEngine {
public:
    DNSEngine();
    ~DNSEngine();

    /**
     * 处理 DNS 查询并检查缓存
     * @param ctx 流量上下文（会更新域名信息）
     * @param pkt DNS 查询数据包
     * @param resp 缓存响应的输出参数（如果缓存命中）
     * @return true 表示缓存命中，false 表示缓存未命中
     *
     * @warning 响应数据（resp.data）的有效期仅到：
     *          1. 同一线程中下次调用 handleQuery()
     *          2. DNSEngine 对象被销毁
     *          如果需要长期保存数据，请立即拷贝。
     *
     * @note 线程安全：每个线程有自己的响应缓冲区。
     */
    bool handleQuery(FlowContext &ctx, const PacketView &pkt, PacketView &resp);

    /**
     * 处理 DNS 响应并更新缓存
     * @param ctx 流量上下文（会更新域名信息）
     * @param pkt DNS 响应数据包
     *
     * @note 线程安全：使用内部锁保护缓存更新。
     */
    void handleResponse(FlowContext &ctx, const PacketView &pkt);

    /**
     * 获取与 IP 地址关联的域名
     * @param ip IP 地址（IPv4 或 IPv6 字符串）
     * @return 域名列表，如果未找到则返回空
     *
     * @note 线程安全：使用内部锁保护。
     */
    std::vector<std::string> getDomainsForIP(const std::string& ip) const;

    /**
     * 清除所有缓存（DNS 响应缓存和 IP-域名映射）
     */
    void clearCache();

private:
    // DNS 响应缓存（查询 -> 响应）
    std::unique_ptr<dns::DNSResponseCache> dns_cache_;

    // IP 到域名映射（ip -> domains）
    mutable std::mutex ip_domains_mutex_;
    std::unordered_map<std::string, std::vector<std::string>> ip_to_domains_;

    /**
     * 添加 IP-域名映射（单个域名）
     * @param ip IP 地址
     * @param domain 域名
     */
    void addIPDomainMapping(const std::string& ip, const std::string& domain);

    /**
     * 添加 IP-域名映射（多个域名）
     * @param ip IP 地址
     * @param domains 域名列表
     */
    void addIPDomainMappings(const std::string& ip, const std::vector<std::string>& domains);

    /**
     * 添加多个 IP 到多个域名的映射
     * @param ips IP 地址列表
     * @param domains 域名列表
     */
    void addIPsDomainsMappings(const std::vector<std::string>& ips, const std::vector<std::string>& domains);
};

}

#endif /* flow_dns_hpp */
