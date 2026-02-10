//
//  flow_defines.h
//  
//
//  Created by jacky on 2026/2/9.
// 
//

#ifndef flow_defines_h
#define flow_defines_h

#include <string>
#include <vector>

namespace flow {

enum class FlowDirection {
    Outbound,
    Inbound
};

// FlowType
enum class FlowType {
    TCP,
    UDP,
    DNS
};

// FlowDecision
enum class FlowDecision {
    Block,
    Allow
};

// PathType
enum class PathType {
    None,
    Direct,    // 普通 Flow 才允许
    Local,
    Gateway
};

// ===== 数据视图（Proxy 收到的数据）=====

struct PacketView {
    const uint8_t* data {nullptr};
    size_t         len {0};
};


// FlowIP
struct FlowIP
{
    enum class Kind : uint8_t
    {
        Unknown = 0,
        V4,
        V6
    };

    Kind kind;

    union
    {
        uint32_t v4;
        struct
        {
            uint64_t hi;
            uint64_t lo;
        } v6;
    };

    FlowIP() noexcept;

    static FlowIP fromIPv4(uint32_t ip) noexcept;
    static FlowIP fromIPv6(uint64_t hi, uint64_t lo) noexcept;

    /**
     * @brief 从字符串解析 IP（IPv4 / IPv6 / IPv4-mapped IPv6）
     * @param ipStr 例如 "127.0.0.1", "::1", "::ffff:127.0.0.1"
     * @return FlowIP，解析失败则 kind=Unknown
     */
    static FlowIP fromString(const std::string& ipStr) noexcept;

    bool isUnknown() const noexcept;
    bool isV4() const noexcept;
    bool isV6() const noexcept;

    bool operator==(const FlowIP& other) const noexcept;
    bool operator!=(const FlowIP& other) const noexcept;
};

class FlowContext {
public:
    uint64_t        session_id {0};
    uint64_t        timestamp_ns {0};

    uint32_t        pid {0};
    std::string     proc_name;
    std::string     proc_path;

    FlowType        flow_type {FlowType::TCP};
    FlowDirection   direction {FlowDirection::Outbound};

    FlowIP          dst_ip;
    uint16_t        dst_port;

    std::vector<std::string> domains;

    // 缓存的 IP 字符串（避免重复转换）
    mutable std::string dst_ip_str;


    PathType        path_decision {PathType::Local};
    FlowDecision    flow_decision {FlowDecision::Allow};

public:
    void addDomains(const std::vector<std::string> &domains);
    bool hasDomain() const;

    bool isDNS() const;

    /**
     * @brief 获取流的描述信息（用于调试和日志）
     * @return 包含流关键信息的描述字符串
     */
    std::string getDescription() const;

    /**
     * @brief 获取目标 IP 的字符串表示（带缓存）
     * @return IP 地址字符串，IPv6 格式为 [addr]，IPv4 为 addr
     */
    const std::string& getIPString() const;

    /**
     * @brief 获取目标 IP 的纯字符串表示（不带括号，用于 DNS 查询）
     * @return IP 地址字符串，不带括号
     */
    std::string getIPStringRaw() const;

};

}

namespace std {
    template<>
    struct hash<flow::FlowIP> {
        size_t operator()(const flow::FlowIP &ip) const {
            size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(ip.kind));

            switch (ip.kind) {
            case flow::FlowIP::Kind::V4:
                h ^= std::hash<uint32_t>{}(ip.v4) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                break;

            case flow::FlowIP::Kind::V6:
                h ^= std::hash<uint64_t>{}(ip.v6.hi) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                h ^= std::hash<uint64_t>{}(ip.v6.lo) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                break;

            default:
                break;
            }

            return h;
        }
    };
}

#endif /* flow_defines_h */
