//
//  flow_defines.cpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#include "flow_defines.h"

#include <arpa/inet.h>
#include <cstring>

namespace flow {

#pragma mark - Flow IP

FlowIP::FlowIP() noexcept
    : kind(Kind::Unknown), v4(0)
{
}

FlowIP FlowIP::fromIPv4(uint32_t ip) noexcept
{
    FlowIP f;
    f.kind = Kind::V4;
    f.v4 = ip;
    return f;
}

FlowIP FlowIP::fromIPv6(uint64_t hi, uint64_t lo) noexcept
{
    FlowIP f;

    // IPv4-mapped IPv6: ::ffff:a.b.c.d
    if (hi == 0 && ((lo >> 32) == 0x0000FFFF))
    {
        f.kind = Kind::V4;
        f.v4 = static_cast<uint32_t>(lo & 0xFFFFFFFFULL);
    }
    else
    {
        f.kind = Kind::V6;
        f.v6.hi = hi;
        f.v6.lo = lo;
    }

    return f;
}

FlowIP FlowIP::fromString(const std::string& ipStr) noexcept
{
    FlowIP result;

    // 1️⃣ 尝试 IPv4
    in_addr addr4 {};
    if (::inet_pton(AF_INET, ipStr.c_str(), &addr4) == 1)
    {
        result.kind = Kind::V4;
        result.v4 = addr4.s_addr; // network byte order
        return result;
    }

    // 2️⃣ 尝试 IPv6
    in6_addr addr6 {};
    if (::inet_pton(AF_INET6, ipStr.c_str(), &addr6) == 1)
    {
        uint64_t hi = 0;
        uint64_t lo = 0;

        static_assert(sizeof(addr6.s6_addr) == 16, "invalid IPv6 size");

        std::memcpy(&hi, addr6.s6_addr, 8);
        std::memcpy(&lo, addr6.s6_addr + 8, 8);

        hi = ntohll(hi);
        lo = ntohll(lo);

        return fromIPv6(hi, lo);
    }

    // 3️⃣ 解析失败
    return result;
}

bool FlowIP::isUnknown() const noexcept
{
    return kind == Kind::Unknown;
}

bool FlowIP::isV4() const noexcept
{
    return kind == Kind::V4;
}

bool FlowIP::isV6() const noexcept
{
    return kind == Kind::V6;
}

bool FlowIP::operator==(const FlowIP& other) const noexcept
{
    if (kind != other.kind)
        return false;

    if (kind == Kind::V4)
        return v4 == other.v4;

    if (kind == Kind::V6)
        return v6.hi == other.v6.hi && v6.lo == other.v6.lo;

    return true; // Unknown == Unknown
}

bool FlowIP::operator!=(const FlowIP& other) const noexcept
{
    return !(*this == other);
}

#pragma mark - Flow Context

void FlowContext::addDomains(const std::vector<std::string>& new_domains) {
    if (new_domains.empty()) {
        return;
    }

    for (const auto& d : new_domains) {
        if (d.empty()) {
            continue;
        }

        auto it = std::find(domains.begin(), domains.end(), d);
        if (it == domains.end()) {
            domains.emplace_back(d);
        }
    }
}

bool FlowContext::hasDomain() const {
    return !domains.empty();
}

bool FlowContext::isDNS() const {
    return dst_port == 53;
}

const std::string& FlowContext::getIPString() const {
    // 如果已经缓存，直接返回
    if (!dst_ip_str.empty()) {
        return dst_ip_str;
    }

    // 否则转换并缓存
    if (dst_ip.isV4()) {
        // 转换 IPv4 地址为字符串
        char ip_str[INET_ADDRSTRLEN];
        in_addr addr;
        addr.s_addr = dst_ip.v4;
        inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
        dst_ip_str = std::string(ip_str);
    } else if (dst_ip.isV6()) {
        // 转换 IPv6 地址为字符串
        char ip_str[INET6_ADDRSTRLEN];
        in6_addr addr;

        // 将 hi 和 lo 转换回网络字节序
        uint64_t hi = htonll(dst_ip.v6.hi);
        uint64_t lo = htonll(dst_ip.v6.lo);

        std::memcpy(addr.s6_addr, &hi, 8);
        std::memcpy(addr.s6_addr + 8, &lo, 8);

        inet_ntop(AF_INET6, &addr, ip_str, INET6_ADDRSTRLEN);
        dst_ip_str = "[" + std::string(ip_str) + "]";
    } else {
        dst_ip_str = "[Unknown]";
    }

    return dst_ip_str;
}

std::string FlowContext::getIPStringRaw() const {
    const std::string& ip_str = getIPString();

    // 如果是 IPv6（带括号），去掉括号
    if (!ip_str.empty() && ip_str[0] == '[' && ip_str.back() == ']') {
        return ip_str.substr(1, ip_str.length() - 2);
    }

    return ip_str;
}

std::string FlowContext::getDescription() const {
    std::string desc;

    // 会话 ID
    desc += "Session[" + std::to_string(session_id) + "] ";

    // 协议类型
    if (flow_type == FlowType::TCP) {
        desc += "TCP ";
    } else if (flow_type == FlowType::UDP) {
        desc += "UDP ";
    } else if (flow_type == FlowType::DNS) {
        desc += "DNS ";
    }

    // 方向
    if (direction == FlowDirection::Outbound) {
        desc += "出站 ";
    } else {
        desc += "入站 ";
    }

    // 目标地址和端口
    desc += "-> ";
    desc += getIPString();
    desc += ":" + std::to_string(dst_port);

    // 域名
    if (!domains.empty()) {
        desc += " (";
        for (size_t i = 0; i < domains.size(); ++i) {
            if (i > 0) desc += ", ";
            desc += domains[i];
        }
        desc += ")";
    }

    // 进程信息
    if (!proc_name.empty()) {
        desc += " [" + proc_name;
        if (pid > 0) {
            desc += ":" + std::to_string(pid);
        }
        desc += "]";
    }

    // 决策
    if (flow_decision == FlowDecision::Block) {
        desc += " [阻止]";
    } else if (flow_decision == FlowDecision::Allow) {
        desc += " [允许]";
    }

    return desc;
}

}
