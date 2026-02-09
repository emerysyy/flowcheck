# FlowEngine API 使用指南

## flowSend() 方法重载

`FlowEngine` 提供了两个 `flowSend()` 重载版本，用于不同的使用场景。

---

## 版本 1: 简单版本（不处理 DNS 响应）

### 函数签名
```cpp
void flowSend(FlowContext& ctx, const PacketView& pkt);
```

### 使用场景
- 普通 TCP/UDP 流量处理
- 不需要获取 DNS 缓存响应
- 只关心协议检测和域名提取

### 示例代码
```cpp
flow::FlowEngine engine;
flow::FlowContext ctx;
ctx.flow_type = flow::FlowType::TCP;
ctx.dst_port = 443;

// 发送 HTTPS 数据包
flow::PacketView pkt;
pkt.data = https_data;
pkt.len = https_len;

// 简单调用，不需要处理响应
engine.flowSend(ctx, pkt);

// 引擎会自动：
// 1. 检测协议（TLS）
// 2. 提取 SNI 域名
// 3. 更新流量决策
```

---

## 版本 2: 完整版本（处理 DNS 响应）

### 函数签名
```cpp
bool flowSend(FlowContext& ctx, const PacketView& pkt, PacketView& response);
```

### 使用场景
- DNS 查询处理
- 需要获取 DNS 缓存响应
- 实现 DNS 代理功能

### 返回值
- `true`: DNS 缓存命中，`response` 包含缓存的 DNS 响应
- `false`: DNS 缓存未命中，需要转发查询到上游 DNS 服务器

### 示例代码

#### 场景 1: DNS 代理实现
```cpp
flow::FlowEngine engine;
flow::FlowContext dns_ctx;
dns_ctx.flow_type = flow::FlowType::UDP;
dns_ctx.dst_port = 53;  // DNS port

// 接收到 DNS 查询
flow::PacketView query;
query.data = dns_query_data;
query.len = dns_query_len;

// 尝试从缓存获取响应
flow::PacketView cached_response;
bool cache_hit = engine.flowSend(dns_ctx, query, cached_response);

if (cache_hit) {
    // 缓存命中 - 直接返回缓存的响应
    send_to_client(cached_response.data, cached_response.len);
    std::cout << "DNS cache hit! Saved a DNS query." << std::endl;
} else {
    // 缓存未命中 - 转发到上游 DNS 服务器
    forward_to_upstream_dns(query.data, query.len);
    std::cout << "DNS cache miss, forwarding query..." << std::endl;
}
```

#### 场景 2: DNS 性能监控
```cpp
flow::FlowEngine engine;
int cache_hits = 0;
int cache_misses = 0;

void process_dns_query(const uint8_t* data, size_t len) {
    flow::FlowContext ctx;
    ctx.flow_type = flow::FlowType::UDP;
    ctx.dst_port = 53;

    flow::PacketView query{data, len};
    flow::PacketView response;

    if (engine.flowSend(ctx, query, response)) {
        cache_hits++;
        // 使用缓存响应
        handle_cached_response(response);
    } else {
        cache_misses++;
        // 转发查询
        forward_query(query);
    }

    // 计算缓存命中率
    double hit_rate = (double)cache_hits / (cache_hits + cache_misses) * 100;
    std::cout << "DNS cache hit rate: " << hit_rate << "%" << std::endl;
}
```

---

## 设计优势

### 1. 类型安全
```cpp
// ✓ 编译时检查 - 不需要响应时不需要传递额外参数
engine.flowSend(ctx, pkt);

// ✓ 编译时检查 - 需要响应时必须提供有效的引用
PacketView resp;
engine.flowSend(ctx, pkt, resp);

// ✗ 编译错误 - 不能传递 nullptr
// engine.flowSend(ctx, pkt, nullptr);  // 编译失败
```

### 2. 语义清晰
```cpp
// 意图明确：不关心响应
void process_normal_traffic() {
    engine.flowSend(ctx, pkt);
}

// 意图明确：需要处理响应
void process_dns_traffic() {
    PacketView response;
    if (engine.flowSend(ctx, pkt, response)) {
        // 处理响应
    }
}
```

### 3. 性能优化
```cpp
// 版本 1：不需要响应时，避免不必要的缓存查找开销
engine.flowSend(ctx, pkt);  // 更快，适合非 DNS 流量

// 版本 2：只在需要时才进行完整的缓存查找
PacketView resp;
bool has_resp = engine.flowSend(ctx, pkt, resp);  // 完整处理
```

---

## 完整示例：DNS 透明代理

```cpp
#include "flow/flow_engine.hpp"
#include <iostream>

class DNSProxy {
private:
    flow::FlowEngine engine_;

public:
    void handle_client_query(const uint8_t* data, size_t len,
                            const std::string& client_ip) {
        // 创建流量上下文
        flow::FlowContext ctx;
        ctx.flow_type = flow::FlowType::UDP;
        ctx.dst_port = 53;
        ctx.proc_name = "dns_proxy";

        // 构造数据包视图
        flow::PacketView query{data, len};
        flow::PacketView cached_response;

        // 尝试从缓存获取响应
        if (engine_.flowSend(ctx, query, cached_response)) {
            // 缓存命中
            std::cout << "[" << client_ip << "] DNS cache HIT" << std::endl;

            // 提取查询的域名
            if (!ctx.domains.empty()) {
                std::cout << "  Domain: " << ctx.domains[0] << std::endl;
            }

            // 直接返回缓存的响应给客户端
            send_response_to_client(client_ip,
                                   cached_response.data,
                                   cached_response.len);

            // 统计
            log_cache_hit();
        } else {
            // 缓存未命中
            std::cout << "[" << client_ip << "] DNS cache MISS" << std::endl;

            // 转发查询到上游 DNS 服务器
            forward_to_upstream(query.data, query.len,
                              [this, ctx](const uint8_t* resp, size_t resp_len) {
                // 收到上游响应后，让引擎处理并缓存
                flow::PacketView response{resp, resp_len};

                // 使用 flowRecv 处理响应（会自动缓存）
                flow::FlowContext recv_ctx = ctx;
                engine_.flowRecv(recv_ctx, response);

                // 返回给客户端
                send_response_to_client(client_ip, resp, resp_len);
            });

            // 统计
            log_cache_miss();
        }
    }

private:
    void send_response_to_client(const std::string& ip,
                                 const uint8_t* data, size_t len) {
        // 实现发送逻辑
    }

    void forward_to_upstream(const uint8_t* data, size_t len,
                            std::function<void(const uint8_t*, size_t)> callback) {
        // 实现转发逻辑
    }

    void log_cache_hit() { /* 统计 */ }
    void log_cache_miss() { /* 统计 */ }
};
```

---

## API 对比

### 旧设计（指针 + 默认参数）
```cpp
// ❌ 不够清晰
bool flowSend(FlowContext& ctx, const PacketView& pkt, PacketView* response = nullptr);

// 调用时需要判断是否传递指针
PacketView resp;
if (need_response) {
    engine.flowSend(ctx, pkt, &resp);  // 需要取地址
} else {
    engine.flowSend(ctx, pkt);  // 或者传 nullptr
}
```

### 新设计（函数重载）
```cpp
// ✓ 清晰明确
void flowSend(FlowContext& ctx, const PacketView& pkt);
bool flowSend(FlowContext& ctx, const PacketView& pkt, PacketView& response);

// 调用时意图明确
engine.flowSend(ctx, pkt);           // 不需要响应

PacketView resp;
bool has = engine.flowSend(ctx, pkt, resp);  // 需要响应，使用引用
```

---

## 最佳实践

### 1. 选择合适的版本
```cpp
// ✓ 对于 HTTP/HTTPS/TCP 流量，使用简单版本
if (ctx.flow_type == flow::FlowType::TCP) {
    engine.flowSend(ctx, pkt);
}

// ✓ 对于 DNS 流量且需要缓存，使用完整版本
if (ctx.isDNS()) {
    PacketView response;
    if (engine.flowSend(ctx, pkt, response)) {
        // 处理缓存响应
    }
}
```

### 2. 错误处理
```cpp
PacketView response;
bool cache_hit = engine.flowSend(ctx, pkt, response);

if (cache_hit) {
    // 验证响应数据
    if (response.data != nullptr && response.len > 0) {
        // 安全使用响应
        process_response(response);
    }
}
```

### 3. 性能考虑
```cpp
// ✓ 对于高频非 DNS 流量，使用简单版本避免开销
for (auto& packet : tcp_packets) {
    engine.flowSend(ctx, packet);  // 快速路径
}

// ✓ 对于 DNS 流量，使用完整版本获得缓存优势
for (auto& dns_query : dns_queries) {
    PacketView resp;
    if (engine.flowSend(ctx, dns_query, resp)) {
        // 缓存命中，节省网络往返
    }
}
```

---

## 总结

| 特性 | 简单版本 | 完整版本 |
|------|---------|---------|
| 函数签名 | `void flowSend(...)` | `bool flowSend(..., PacketView&)` |
| 返回值 | 无 | bool (是否有响应) |
| DNS 响应 | 不处理 | 通过引用返回 |
| 适用场景 | 普通流量 | DNS 代理/缓存 |
| 性能 | 更快（无额外开销） | 完整功能 |
| 类型安全 | ✓ | ✓ |

选择合适的版本可以让代码更清晰、更高效！
