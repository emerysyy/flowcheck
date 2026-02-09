# FlowCheck API 文档

## 概述

FlowCheck 提供了一套简洁而强大的 API，用于网络流量检测、协议识别和域名提取。

## 核心类

### FlowEngine

流量引擎，采用单例模式，管理整个流量处理流程。

#### 获取实例

```cpp
#include "flow/flow_engine.hpp"

FlowEngine& engine = FlowEngine::getInstance();
```

**说明**：
- 单例模式，全局唯一实例
- 线程安全
- 自动管理生命周期

#### 方法列表

##### flowArrive

```cpp
void flowArrive(FlowContext& ctx);
```

**功能**：处理流量到达事件

**参数**：
- `ctx`: 流量上下文，包含流的元数据

**说明**：
- 区分 DNS 和普通流量
- 设置初始路由决策
- 默认允许所有流量

**示例**：
```cpp
FlowContext ctx;
ctx.dst_port = 443;
ctx.flow_type = FlowType::TCP;

engine.flowArrive(ctx);
// ctx.path_decision 和 ctx.flow_decision 已设置
```

---

##### flowOpen

```cpp
void flowOpen(FlowContext& ctx);
```

**功能**：处理流量打开事件

**参数**：
- `ctx`: 流量上下文

**说明**：
- 流量状态初始化
- 决策验证
- 预留扩展点

**示例**：
```cpp
engine.flowOpen(ctx);
```

---

##### flowSend (无响应版本)

```cpp
void flowSend(FlowContext& ctx, const PacketView& pkt);
```

**功能**：处理出站数据包（不需要 DNS 响应）

**参数**：
- `ctx`: 流量上下文
- `pkt`: 出站数据包视图

**说明**：
- 用于非 DNS 流量或不需要缓存响应的场景
- 自动进行协议检测
- 提取域名信息

**示例**：
```cpp
PacketView pkt;
pkt.data = packet_data;
pkt.len = packet_size;

engine.flowSend(ctx, pkt);

// 检查是否提取到域名
if (ctx.hasDomain()) {
    for (const auto& domain : ctx.domains) {
        std::cout << "域名: " << domain << std::endl;
    }
}
```

---

##### flowSend (带响应版本)

```cpp
bool flowSend(FlowContext& ctx, const PacketView& pkt, PacketView& response);
```

**功能**：处理出站数据包（可能返回 DNS 响应）

**参数**：
- `ctx`: 流量上下文
- `pkt`: 出站数据包视图
- `response`: DNS 响应输出参数（如果缓存命中）

**返回值**：
- `true`: 缓存命中，`response` 包含 DNS 响应
- `false`: 缓存未命中

**说明**：
- 专门用于 DNS 查询处理
- 如果缓存命中，直接返回响应，无需转发
- 如果缓存未命中，需要转发到上游 DNS

**示例**：
```cpp
PacketView pkt;
pkt.data = dns_query_data;
pkt.len = dns_query_size;

PacketView response;
if (engine.flowSend(ctx, pkt, response)) {
    // 缓存命中，直接返回给客户端
    send_to_client(response.data, response.len);
} else {
    // 缓存未命中，转发到上游 DNS
    forward_to_upstream(pkt.data, pkt.len);
}
```

---

##### flowRecv

```cpp
void flowRecv(FlowContext& ctx, const PacketView& pkt);
```

**功能**：处理入站数据包

**参数**：
- `ctx`: 流量上下文
- `pkt`: 入站数据包视图

**说明**：
- 处理 DNS 响应并更新缓存
- 进行协议检测
- 提取域名信息

**示例**：
```cpp
PacketView pkt;
pkt.data = response_data;
pkt.len = response_size;

engine.flowRecv(ctx, pkt);

// DNS 响应已缓存，域名已提取
```

---

##### flowClose

```cpp
void flowClose(FlowContext& ctx);
```

**功能**：处理流量关闭事件

**参数**：
- `ctx`: 流量上下文

**说明**：
- 清理流量状态
- 预留扩展点

**示例**：
```cpp
engine.flowClose(ctx);
```

---

##### getDNSEngine

```cpp
DNSEngine& getDNSEngine();
```

**功能**：获取 DNS 引擎实例

**返回值**：DNS 引擎的引用

**说明**：
- 用于高级 DNS 操作
- 查询 IP-域名映射
- 清除缓存

**示例**：
```cpp
DNSEngine& dns = engine.getDNSEngine();
auto domains = dns.getDomainsForIP("8.8.8.8");
```

---

### DNSEngine

DNS 处理引擎，提供 DNS 缓存和 IP-域名映射功能。

#### 方法列表

##### handleQuery

```cpp
bool handleQuery(FlowContext& ctx, const PacketView& pkt, PacketView& resp);
```

**功能**：处理 DNS 查询

**参数**：
- `ctx`: 流量上下文
- `pkt`: DNS 查询数据包
- `resp`: DNS 响应输出参数

**返回值**：
- `true`: 缓存命中
- `false`: 缓存未命中

**说明**：
- 解析 DNS 查询包
- 提取查询域名
- 查找缓存
- 返回缓存响应（如果有）

**示例**：
```cpp
DNSEngine& dns = engine.getDNSEngine();

PacketView query, response;
query.data = dns_query;
query.len = query_size;

if (dns.handleQuery(ctx, query, response)) {
    // 缓存命中
    send_response(response.data, response.len);
}
```

---

##### handleResponse

```cpp
void handleResponse(FlowContext& ctx, const PacketView& pkt);
```

**功能**：处理 DNS 响应

**参数**：
- `ctx`: 流量上下文
- `pkt`: DNS 响应数据包

**说明**：
- 解析 DNS 响应包
- 提取域名和 IP 地址
- 更新 DNS 缓存
- 建立 IP-域名映射（多对多）
- 支持 A、AAAA、CNAME、PTR、MX、SRV 记录

**示例**：
```cpp
DNSEngine& dns = engine.getDNSEngine();

PacketView response;
response.data = dns_response;
response.len = response_size;

dns.handleResponse(ctx, response);
// 缓存已更新，IP-域名映射已建立
```

---

##### getDomainsForIP

```cpp
std::vector<std::string> getDomainsForIP(const std::string& ip) const;
```

**功能**：查询 IP 对应的域名列表

**参数**：
- `ip`: IP 地址字符串

**返回值**：域名列表（可能为空）

**说明**：
- 支持多对多映射
- 一个 IP 可能对应多个域名
- 线程安全

**示例**：
```cpp
DNSEngine& dns = engine.getDNSEngine();

std::vector<std::string> domains = dns.getDomainsForIP("220.181.174.34");
for (const auto& domain : domains) {
    std::cout << "域名: " << domain << std::endl;
}
```

---

##### clearCache

```cpp
void clearCache();
```

**功能**：清除所有缓存

**说明**：
- 清除 DNS 响应缓存
- 清除 IP-域名映射
- 线程安全

**示例**：
```cpp
DNSEngine& dns = engine.getDNSEngine();
dns.clearCache();
```

---

### Detector

协议检测器，识别网络协议并提取域名。

#### 方法列表

##### detectProtocol

```cpp
std::string detectProtocol(const FlowContext& ctx, const PacketView& pkt);
```

**功能**：检测网络协议

**参数**：
- `ctx`: 流量上下文
- `pkt`: 数据包视图

**返回值**：协议名称字符串（如 "HTTP", "TLS", "DNS"）

**支持的协议**：
- HTTP
- TLS/HTTPS
- DNS
- QUIC
- SSH
- FTP
- SMTP
- IMAP
- POP3
- RTP
- SMB
- TFTP

**示例**：
```cpp
Detector detector;
std::string protocol = detector.detectProtocol(ctx, pkt);
std::cout << "检测到协议: " << protocol << std::endl;
```

---

##### extractDomain

```cpp
std::string extractDomain(const FlowContext& ctx, const PacketView& pkt);
```

**功能**：从数据包中提取域名

**参数**：
- `ctx`: 流量上下文
- `pkt`: 数据包视图

**返回值**：域名字符串（如果提取成功）

**支持的协议**：
- HTTP: 提取 Host 头
- TLS: 提取 SNI (Server Name Indication)
- DNS: 提取查询域名

**示例**：
```cpp
Detector detector;
std::string domain = detector.extractDomain(ctx, pkt);
if (!domain.empty()) {
    std::cout << "提取域名: " << domain << std::endl;
}
```

---

## 数据结构

### FlowContext

流量上下文，包含流的所有元数据。

```cpp
class FlowContext {
public:
    uint64_t        session_id;      // 会话 ID
    uint64_t        timestamp_ns;    // 时间戳（纳秒）

    uint32_t        pid;             // 进程 ID
    std::string     proc_name;       // 进程名称
    std::string     proc_path;       // 进程路径

    FlowType        flow_type;       // 流类型 (TCP/UDP/DNS)
    FlowDirection   direction;       // 方向 (Outbound/Inbound)

    FlowIP          dst_ip;          // 目标 IP
    uint16_t        dst_port;        // 目标端口

    std::vector<std::string> domains; // 域名列表

    PathType        path_decision;   // 路径决策
    FlowDecision    flow_decision;   // 流量决策

private:
    mutable std::string dst_ip_str;  // 缓存的 IP 字符串（内部使用）

public:
    void addDomains(const std::vector<std::string>& domains);
    bool hasDomain() const;
    bool isDNS() const;
};
```

**方法**：

- `addDomains()`: 添加域名到列表
- `hasDomain()`: 检查是否有域名
- `isDNS()`: 检查是否为 DNS 流量
- `getDescription()`: 获取流的描述信息（用于调试和日志）
- `getIPString()`: 获取目标 IP 的字符串表示（带缓存，IPv6 带括号）
- `getIPStringRaw()`: 获取目标 IP 的纯字符串表示（不带括号，用于 DNS 查询）

**getIPString() 方法**：

返回目标 IP 的字符串表示，IPv6 地址会带括号 `[addr]`，IPv4 地址不带括号。

```cpp
FlowContext ctx;
ctx.dst_ip = FlowIP::fromString("2001:4860:4860::8888");
std::string ip = ctx.getIPString();  // 返回 "[2001:4860:4860::8888]"
```

**getIPStringRaw() 方法**：

返回目标 IP 的纯字符串表示，不带括号，适用于 DNS 缓存查询等场景。

```cpp
FlowContext ctx;
ctx.dst_ip = FlowIP::fromString("2001:4860:4860::8888");
std::string ip = ctx.getIPStringRaw();  // 返回 "2001:4860:4860::8888"
```

**getDescription() 方法**：

返回包含流关键信息的描述字符串，格式如下：
```
Session[会话ID] 协议 方向 -> IP:端口 (域名列表) [进程:PID] [决策]
```

**性能优化**：
- IP 地址字符串会被缓存在 `dst_ip_str` 字段中（mutable）
- 第一次调用 `getIPString()` 或 `getIPStringRaw()` 时进行 IP 转换（约 4-5 微秒）
- 后续调用直接使用缓存（约 0.3-0.5 微秒）
- 性能提升约 10-14 倍
- **所有需要 IP 字符串的地方都使用缓存**：
  - `getDescription()` 方法
  - DNS 缓存查询（`flow_engine.cpp` 中的 `flowSend()` 方法）
  - 避免了重复的 `inet_ntop()` 调用

示例输出：
```cpp
FlowContext ctx;
ctx.session_id = 12345;
ctx.flow_type = FlowType::TCP;
ctx.direction = FlowDirection::Outbound;
ctx.dst_ip = FlowIP::fromString("220.181.174.34");
ctx.dst_port = 443;
ctx.addDomains({"www.baidu.com", "baidu.com"});
ctx.proc_name = "Chrome";
ctx.pid = 1234;
ctx.flow_decision = FlowDecision::Allow;

std::cout << ctx.getDescription() << std::endl;
// 输出: Session[12345] TCP 出站 -> 220.181.174.34:443 (www.baidu.com, baidu.com) [Chrome:1234] [允许]
```

---

### PacketView

数据包视图，零拷贝设计。

```cpp
struct PacketView {
    const uint8_t* data;  // 数据指针
    size_t         len;   // 数据长度
};
```

**说明**：
- 不拥有数据，只是视图
- 避免数据拷贝
- 调用者负责数据生命周期管理

---

### FlowIP

IP 地址结构，支持 IPv4 和 IPv6。

```cpp
struct FlowIP {
    enum class Kind : uint8_t {
        Unknown = 0,
        V4,
        V6
    };

    Kind kind;

    union {
        uint32_t v4;  // IPv4 (网络字节序)
        struct {
            uint64_t hi;
            uint64_t lo;
        } v6;         // IPv6
    };

    static FlowIP fromIPv4(uint32_t ip) noexcept;
    static FlowIP fromIPv6(uint64_t hi, uint64_t lo) noexcept;
    static FlowIP fromString(const std::string& ipStr) noexcept;

    bool isUnknown() const noexcept;
    bool isV4() const noexcept;
    bool isV6() const noexcept;

    bool operator==(const FlowIP& other) const noexcept;
    bool operator!=(const FlowIP& other) const noexcept;
};
```

**工厂方法**：

- `fromIPv4()`: 从 IPv4 地址创建
- `fromIPv6()`: 从 IPv6 地址创建
- `fromString()`: 从字符串解析（支持 IPv4、IPv6、IPv4-mapped IPv6）

---

### 枚举类型

#### FlowType

```cpp
enum class FlowType {
    TCP,
    UDP,
    DNS
};
```

#### FlowDirection

```cpp
enum class FlowDirection {
    Outbound,  // 出站
    Inbound    // 入站
};
```

#### FlowDecision

```cpp
enum class FlowDecision {
    Block,  // 阻止
    Allow   // 允许
};
```

#### PathType

```cpp
enum class PathType {
    None,
    Direct,   // 直连
    Local,    // 本地
    Gateway   // 网关
};
```

---

## 使用示例

### 完整示例：处理 DNS 流量

```cpp
#include "flow/flow_engine.hpp"
#include "flow/flow_defines.h"

void processDNSQuery(const uint8_t* query_data, size_t query_size) {
    // 获取 FlowEngine 单例
    FlowEngine& engine = FlowEngine::getInstance();

    // 创建流上下文
    FlowContext ctx;
    ctx.session_id = generate_session_id();
    ctx.dst_ip = FlowIP::fromString("8.8.8.8");
    ctx.dst_port = 53;
    ctx.flow_type = FlowType::UDP;
    ctx.direction = FlowDirection::Outbound;

    // 流量到达
    engine.flowArrive(ctx);

    // 流量打开
    engine.flowOpen(ctx);

    // 处理 DNS 查询
    PacketView query;
    query.data = query_data;
    query.len = query_size;

    PacketView response;
    if (engine.flowSend(ctx, query, response)) {
        // 缓存命中，直接返回响应
        send_to_client(response.data, response.len);
        std::cout << "DNS 缓存命中" << std::endl;
    } else {
        // 缓存未命中，转发到上游 DNS
        forward_to_upstream(query.data, query.len);
        std::cout << "DNS 缓存未命中，转发查询" << std::endl;
    }

    // 显示提取的域名
    if (ctx.hasDomain()) {
        std::cout << "查询域名: ";
        for (const auto& domain : ctx.domains) {
            std::cout << domain << " ";
        }
        std::cout << std::endl;
    }
}

void processDNSResponse(const uint8_t* response_data, size_t response_size) {
    FlowEngine& engine = FlowEngine::getInstance();

    FlowContext ctx;
    ctx.session_id = get_session_id();
    ctx.dst_ip = FlowIP::fromString("8.8.8.8");
    ctx.dst_port = 53;
    ctx.flow_type = FlowType::UDP;
    ctx.direction = FlowDirection::Inbound;

    // 处理 DNS 响应
    PacketView response;
    response.data = response_data;
    response.len = response_size;

    engine.flowRecv(ctx, response);

    // 响应已缓存，域名已提取
    std::cout << "DNS 响应已缓存" << std::endl;

    // 查询 IP-域名映射
    DNSEngine& dns = engine.getDNSEngine();
    auto domains = dns.getDomainsForIP("220.181.174.34");
    if (!domains.empty()) {
        std::cout << "IP 220.181.174.34 对应的域名: ";
        for (const auto& domain : domains) {
            std::cout << domain << " ";
        }
        std::cout << std::endl;
    }

    // 流量关闭
    engine.flowClose(ctx);
}
```

### 完整示例：处理 HTTPS 流量

```cpp
void processHTTPSTraffic(const uint8_t* tls_data, size_t tls_size) {
    FlowEngine& engine = FlowEngine::getInstance();

    // 创建流上下文
    FlowContext ctx;
    ctx.session_id = generate_session_id();
    ctx.dst_ip = FlowIP::fromString("220.181.174.34");
    ctx.dst_port = 443;
    ctx.flow_type = FlowType::TCP;
    ctx.direction = FlowDirection::Outbound;

    // 流量处理
    engine.flowArrive(ctx);
    engine.flowOpen(ctx);

    // 处理 TLS ClientHello
    PacketView pkt;
    pkt.data = tls_data;
    pkt.len = tls_size;

    engine.flowSend(ctx, pkt);

    // 检查是否提取到 SNI
    if (ctx.hasDomain()) {
        std::cout << "提取到 SNI: ";
        for (const auto& domain : ctx.domains) {
            std::cout << domain << " ";
        }
        std::cout << std::endl;

        // 基于域名做决策
        if (is_blocked_domain(ctx.domains[0])) {
            ctx.flow_decision = FlowDecision::Block;
            std::cout << "域名被阻止" << std::endl;
        }
    }

    engine.flowClose(ctx);
}
```

---

## 错误处理

### 返回值约定

- `bool` 返回值：`true` 表示成功，`false` 表示失败
- 字符串返回值：空字符串表示未找到或失败
- 向量返回值：空向量表示未找到

### 异常安全

- 所有 API 都是异常安全的
- 不会抛出异常
- 使用返回值表示错误

### 输入验证

```cpp
// 示例：检查输入有效性
if (!pkt.data || pkt.len == 0) {
    // 无效输入
    return false;
}
```

---

## 线程安全

### 线程安全的 API

- `FlowEngine::getInstance()`: 线程安全
- `DNSEngine::handleQuery()`: 线程安全
- `DNSEngine::handleResponse()`: 线程安全
- `DNSEngine::getDomainsForIP()`: 线程安全
- `DNSEngine::clearCache()`: 线程安全

### 非线程安全的 API

- `FlowContext`: 不是线程安全的，每个线程应使用独立的实例

---

## 性能考虑

### 最佳实践

1. **重用 FlowContext**：避免频繁创建和销毁
2. **零拷贝**：使用 PacketView 避免数据拷贝
3. **批量操作**：尽可能批量处理流量
4. **缓存利用**：充分利用 DNS 缓存减少查询

### 性能指标

- DNS 缓存命中率：> 90%
- 协议检测延迟：< 1ms
- 域名提取成功率：99%

---

## 版本信息

- **版本**：1.0.0
- **C++ 标准**：C++17
- **编译器要求**：支持 C++17 的编译器
- **平台**：跨平台（Linux, macOS, Windows）

---

## 参考资料

- [测试说明](测试说明.md)
- [架构设计](架构设计.md)
- [README](../README.md)
