# Flow DNS 实现总结

## 概述

`flow_dns` 模块提供了完整的 DNS 查询和响应处理功能，包括缓存管理、域名提取和 IP 地址解析。

---

## 核心功能

### 1. DNS 查询处理 (`handleQuery`)

#### 功能
- 解析 DNS 查询包
- 提取查询的域名
- 从缓存中查找响应
- 返回缓存的响应（如果命中）

#### 实现细节
```cpp
bool DNSEngine::handleQuery(FlowContext &ctx, const PacketView &pkt, PacketView &resp)
```

**处理流程**：
1. 验证数据包有效性
2. 使用 `dns::DNSParser` 解析查询
3. 提取问题部分的域名
4. 更新 `FlowContext` 的域名列表
5. 查询 DNS 缓存
6. 如果缓存命中，返回缓存的响应

**返回值**：
- `true`: 缓存命中，`resp` 包含响应数据
- `false`: 缓存未命中，需要转发查询

---

### 2. DNS 响应处理 (`handleResponse`)

#### 功能
- 解析 DNS 响应包
- 提取所有域名（问题、答案、CNAME 等）
- 提取 IP 地址（A 和 AAAA 记录）
- 验证响应有效性
- 存储响应到缓存

#### 实现细节
```cpp
void DNSEngine::handleResponse(FlowContext &ctx, const PacketView &pkt)
```

**处理流程**：
1. **验证数据包**
   - 检查最小长度（12 字节 DNS 头部）
   - 验证数据有效性

2. **解析 DNS 响应**
   - 使用 `dns::DNSParser` 解析
   - 检查 QR 位确认是响应包

3. **提取域名**
   - 问题部分的域名
   - 答案记录的域名
   - CNAME 记录的目标域名
   - PTR 记录的域名
   - MX 记录的邮件服务器域名
   - SRV 记录的目标域名

4. **提取 IP 地址**
   - A 记录（IPv4）
   - AAAA 记录（IPv6）

5. **更新上下文**
   - 将所有域名添加到 `FlowContext`

6. **缓存响应**
   - 仅当有有效 IP 地址时才缓存
   - 使用 `dns::DNSResponseCache::storeResponse`

---

## 支持的 DNS 记录类型

| 记录类型 | 值 | 提取内容 | 说明 |
|---------|---|---------|------|
| A | 1 | IPv4 地址 | 域名到 IPv4 映射 |
| AAAA | 28 | IPv6 地址 | 域名到 IPv6 映射 |
| CNAME | 5 | 目标域名 | 域名别名 |
| PTR | 12 | 域名 | IP 反向解析 |
| MX | 15 | 邮件服务器域名 | 邮件交换记录 |
| SRV | 33 | 目标域名 | 服务定位记录 |

---

## DNS 缓存机制

### 缓存实例
```cpp
static std::unique_ptr<dns::DNSResponseCache> g_dns_cache;
```

- **单例模式**：全局唯一的缓存实例
- **延迟初始化**：首次使用时创建
- **容量**：2048 条记录
- **线程安全**：内部使用互斥锁保护

### 缓存策略
1. **存储条件**：
   - 响应包必须有效
   - 必须包含至少一个 IP 地址（A 或 AAAA 记录）
   - 响应码为成功（RCODE = 0）

2. **查找机制**：
   - 基于查询的域名、类型和类别
   - 自动处理 TTL 过期
   - LRU 淘汰策略

3. **响应重写**：
   - 自动更新事务 ID（Transaction ID）
   - 保持原始响应格式

---

## 与 Objective-C 代码的对应关系

### OC: `handleDNSQuery:responseData:`
对应 C++: `DNSEngine::handleQuery`

| OC 代码 | C++ 代码 | 说明 |
|---------|---------|------|
| `EP_DNS_RESP_CACHE.buildResponseFromCache` | `cache.buildResponseFromCache` | 从缓存获取响应 |
| `*response = [NSData dataWithBytes:...]` | `resp.data = cached_response.data()` | 返回响应数据 |
| 返回 `BOOL` | 返回 `bool` | 表示缓存是否命中 |

### OC: `parseDNSResponse:`
对应 C++: `DNSEngine::handleResponse`

| OC 代码 | C++ 代码 | 说明 |
|---------|---------|------|
| 手动解析 DNS 头部 | `dns::DNSParser::parse` | 使用专用解析器 |
| 检查 `isResponse` (QR 位) | `msg.header.flags & 0x8000` | 验证响应包 |
| 提取问题域名 | `msg.questions[].name` | 问题部分域名 |
| 提取 A 记录 IP | `ans.ipv4()` | IPv4 地址 |
| 提取 AAAA 记录 IP | `ans.ipv6()` | IPv6 地址 |
| 提取 CNAME | `ans.domain` (CNAME) | CNAME 目标 |
| `ipList.size() > 0` 才缓存 | `!ip_addresses.empty()` 才缓存 | 相同的缓存条件 |
| `EP_PROXY_EXTENSION.saveDNSCache` | `cache.storeResponse` | 存储到缓存 |

---

## 使用示例

### 示例 1: DNS 代理服务器

```cpp
#include "flow/flow_dns.hpp"
#include "flow/flow_engine.hpp"

class DNSProxyServer {
private:
    flow::FlowEngine engine_;

public:
    void handleClientQuery(const uint8_t* query_data, size_t query_len,
                          const std::string& client_addr) {
        // 创建流量上下文
        flow::FlowContext ctx;
        ctx.flow_type = flow::FlowType::UDP;
        ctx.dst_port = 53;
        ctx.proc_name = "dns_proxy";

        // 构造查询包
        flow::PacketView query{query_data, query_len};
        flow::PacketView response;

        // 尝试从缓存获取响应
        if (engine_.flowSend(ctx, query, response)) {
            // 缓存命中
            std::cout << "[" << client_addr << "] Cache HIT" << std::endl;

            // 打印查询的域名
            if (!ctx.domains.empty()) {
                std::cout << "  Domain: " << ctx.domains[0] << std::endl;
            }

            // 直接返回缓存响应
            sendToClient(client_addr, response.data, response.len);
        } else {
            // 缓存未命中
            std::cout << "[" << client_addr << "] Cache MISS" << std::endl;

            // 转发到上游 DNS
            forwardToUpstream(query_data, query_len,
                [this, ctx, client_addr](const uint8_t* resp, size_t resp_len) {
                    // 收到上游响应
                    flow::PacketView response{resp, resp_len};

                    // 处理并缓存响应
                    flow::FlowContext recv_ctx = ctx;
                    engine_.flowRecv(recv_ctx, response);

                    // 返回给客户端
                    sendToClient(client_addr, resp, resp_len);

                    // 打印解析的域名
                    if (!recv_ctx.domains.empty()) {
                        std::cout << "  Cached domains: ";
                        for (const auto& domain : recv_ctx.domains) {
                            std::cout << domain << " ";
                        }
                        std::cout << std::endl;
                    }
                });
        }
    }

private:
    void sendToClient(const std::string& addr, const uint8_t* data, size_t len) {
        // 实现发送逻辑
    }

    void forwardToUpstream(const uint8_t* data, size_t len,
                          std::function<void(const uint8_t*, size_t)> callback) {
        // 实现转发逻辑
    }
};
```

### 示例 2: DNS 查询监控

```cpp
#include "flow/flow_dns.hpp"

class DNSMonitor {
private:
    flow::DNSEngine dns_engine_;
    int total_queries_ = 0;
    int cache_hits_ = 0;

public:
    void monitorQuery(const uint8_t* query_data, size_t query_len) {
        flow::FlowContext ctx;
        ctx.flow_type = flow::FlowType::UDP;
        ctx.dst_port = 53;

        flow::PacketView query{query_data, query_len};
        flow::PacketView response;

        total_queries_++;

        if (dns_engine_.handleQuery(ctx, query, response)) {
            cache_hits_++;
            std::cout << "✓ Cache hit for: ";
        } else {
            std::cout << "✗ Cache miss for: ";
        }

        // 打印查询的域名
        if (!ctx.domains.empty()) {
            std::cout << ctx.domains[0] << std::endl;
        }

        // 打印统计
        double hit_rate = (double)cache_hits_ / total_queries_ * 100;
        std::cout << "Cache hit rate: " << hit_rate << "%" << std::endl;
    }

    void monitorResponse(const uint8_t* resp_data, size_t resp_len) {
        flow::FlowContext ctx;
        ctx.flow_type = flow::FlowType::UDP;
        ctx.dst_port = 53;

        flow::PacketView response{resp_data, resp_len};

        // 处理响应（会自动缓存）
        dns_engine_.handleResponse(ctx, response);

        // 打印解析结果
        std::cout << "Parsed DNS response:" << std::endl;
        std::cout << "  Domains: ";
        for (const auto& domain : ctx.domains) {
            std::cout << domain << " ";
        }
        std::cout << std::endl;
    }
};
```

---

## 关键特性

### 1. 自动域名提取
- ✅ 从问题部分提取查询域名
- ✅ 从答案部分提取所有相关域名
- ✅ 支持 CNAME 链
- ✅ 支持 MX 和 SRV 记录

### 2. IP 地址解析
- ✅ 提取 IPv4 地址（A 记录）
- ✅ 提取 IPv6 地址（AAAA 记录）
- ✅ 自动格式化为字符串

### 3. 智能缓存
- ✅ 仅缓存有效响应（包含 IP 地址）
- ✅ 自动 TTL 管理
- ✅ LRU 淘汰策略
- ✅ 线程安全

### 4. 响应验证
- ✅ 检查最小包长度
- ✅ 验证 QR 位（确保是响应）
- ✅ 解析失败自动忽略

---

## 性能优化

### 1. 线程局部存储
```cpp
static thread_local std::vector<uint8_t> cached_response;
```
- 避免频繁内存分配
- 每个线程独立缓冲区

### 2. 延迟初始化
```cpp
static std::unique_ptr<dns::DNSResponseCache> g_dns_cache;
```
- 仅在首次使用时创建
- 减少启动开销

### 3. 零拷贝设计
```cpp
resp.data = cached_response.data();
resp.len = cached_response.size();
```
- 使用 `PacketView` 避免数据拷贝
- 直接引用缓存数据

---

## 错误处理

### 输入验证
```cpp
if (pkt.data == nullptr || pkt.len == 0) {
    return false;  // 或 return;
}

if (pkt.len < 12) {
    return;  // DNS 头部最小 12 字节
}
```

### 解析失败
```cpp
if (!parser.parse(pkt.data, pkt.len, msg)) {
    return false;  // 解析失败，忽略
}
```

### 响应验证
```cpp
bool isResponse = (msg.header.flags & 0x8000) != 0;
if (!isResponse) {
    return;  // 不是响应包，忽略
}
```

---

## 与 OC 代码的改进

### 1. 使用专用解析器
- **OC**: 手动解析字节流
- **C++**: 使用 `dns::DNSParser`
- **优势**: 更安全、更易维护

### 2. 类型安全
- **OC**: 使用 `NSData` 和指针
- **C++**: 使用 `PacketView` 和引用
- **优势**: 编译时类型检查

### 3. 内存管理
- **OC**: ARC 自动管理
- **C++**: RAII 和智能指针
- **优势**: 确定性析构

### 4. 性能
- **OC**: 对象创建开销
- **C++**: 零拷贝设计
- **优势**: 更高性能

---

## 总结

`flow_dns` 模块提供了完整的 DNS 处理功能：

✅ **功能完整**：查询、响应、缓存
✅ **类型安全**：使用引用而非指针
✅ **性能优化**：零拷贝、线程局部存储
✅ **易于使用**：清晰的 API 设计
✅ **可靠性高**：完善的错误处理

与 Objective-C 代码相比，C++ 实现更加模块化、类型安全，并且性能更优。
