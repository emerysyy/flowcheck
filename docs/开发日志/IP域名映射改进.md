# IP-Domains 映射改进说明

## 改进内容

### 问题
之前的实现是一对一映射：每个 IP 只映射到它对应的 A/AAAA 记录的域名。

```cpp
// 旧实现 - 一对一映射
if (ans.type == A_RECORD) {
    auto ipv4 = ans.ipv4();
    if (ipv4.has_value() && !ans.name.empty()) {
        addIPDomainMapping(ipv4.value(), ans.name);  // 只映射到记录名
    }
}
```

**问题**：
- 一个 DNS 响应中可能包含多个相关域名（问题域名、CNAME、答案域名等）
- 这些域名都应该关联到响应中的所有 IP
- 实际场景：`www.example.com` (CNAME) -> `example.com` (A) -> `93.184.216.34`
  - 这个 IP 应该同时关联到 `www.example.com` 和 `example.com`

---

## 新实现

### 1. 批量映射方法

```cpp
/**
 * Add IP-domains mapping (multiple domains)
 * @param ip IP address
 * @param domains Vector of domain names
 */
void addIPDomainMappings(const std::string& ip, const std::vector<std::string>& domains);

/**
 * Add multiple IPs to multiple domains mapping
 * @param ips Vector of IP addresses
 * @param domains Vector of domain names
 */
void addIPsDomainsMappings(const std::vector<std::string>& ips, const std::vector<std::string>& domains);
```

### 2. 改进的 handleResponse 逻辑

```cpp
void DNSEngine::handleResponse(FlowContext &ctx, const PacketView &pkt) {
    // ... 解析 DNS 响应 ...

    std::vector<std::string> domains;      // 收集所有域名
    std::vector<std::string> ip_addresses; // 收集所有 IP

    // 1. 收集问题域名
    for (const auto& q : msg.questions) {
        if (!q.name.empty()) {
            domains.push_back(q.name);
        }
    }

    // 2. 收集答案中的所有信息
    for (const auto& ans : msg.answers) {
        // 答案记录名
        if (!ans.name.empty()) {
            domains.push_back(ans.name);
        }

        // IP 地址
        if (ans.type == A_RECORD) {
            auto ipv4 = ans.ipv4();
            if (ipv4.has_value()) {
                ip_addresses.push_back(ipv4.value());
            }
        }

        // CNAME 域名
        if (ans.type == CNAME && ans.domain.has_value()) {
            domains.push_back(ans.domain.value());
        }

        // ... 其他记录类型 ...
    }

    // 3. 建立完整映射：所有 IP -> 所有域名
    if (!ip_addresses.empty() && !domains.empty()) {
        addIPsDomainsMappings(ip_addresses, domains);
    }
}
```

---

## 实际场景示例

### 场景 1：CNAME 链

**DNS 响应**：
```
Question: www.example.com
Answer:
  www.example.com  CNAME  example.com
  example.com      A      93.184.216.34
```

**旧实现**：
```
93.184.216.34 -> [example.com]  // 只映射到 A 记录的域名
```

**新实现**：
```
93.184.216.34 -> [www.example.com, example.com]  // 映射到所有相关域名
```

### 场景 2：多个 A 记录

**DNS 响应**：
```
Question: example.com
Answer:
  example.com  A  93.184.216.34
  example.com  A  93.184.216.35
```

**旧实现**：
```
93.184.216.34 -> [example.com]
93.184.216.35 -> [example.com]
```

**新实现**：
```
93.184.216.34 -> [example.com]
93.184.216.35 -> [example.com]
```
（这种情况两种实现相同）

### 场景 3：复杂的 CNAME + 多 IP

**DNS 响应**：
```
Question: www.cdn.example.com
Answer:
  www.cdn.example.com  CNAME  cdn.example.com
  cdn.example.com      CNAME  edge.example.com
  edge.example.com     A      1.2.3.4
  edge.example.com     A      1.2.3.5
```

**旧实现**：
```
1.2.3.4 -> [edge.example.com]
1.2.3.5 -> [edge.example.com]
```

**新实现**：
```
1.2.3.4 -> [www.cdn.example.com, cdn.example.com, edge.example.com]
1.2.3.5 -> [www.cdn.example.com, cdn.example.com, edge.example.com]
```

---

## 优势

### 1. 更完整的信息
当看到一个 IP 连接时，可以知道所有相关的域名：
```cpp
auto domains = dns_engine.getDomainsForIP("93.184.216.34");
// 返回: ["www.example.com", "example.com"]
// 而不仅仅是: ["example.com"]
```

### 2. 更好的日志和分析
```cpp
void logConnection(const std::string& dst_ip) {
    auto domains = dns_engine.getDomainsForIP(dst_ip);

    if (!domains.empty()) {
        std::cout << "Connection to " << dst_ip << " (";
        for (size_t i = 0; i < domains.size(); ++i) {
            std::cout << domains[i];
            if (i < domains.size() - 1) std::cout << ", ";
        }
        std::cout << ")" << std::endl;
    }
}

// 输出: Connection to 93.184.216.34 (www.example.com, example.com)
```

### 3. 更准确的域名匹配
```cpp
bool isAccessingDomain(const std::string& ip, const std::string& target_domain) {
    auto domains = dns_engine.getDomainsForIP(ip);

    for (const auto& domain : domains) {
        if (domain == target_domain ||
            domain.find(target_domain) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 现在可以检测到通过 CNAME 访问的域名
if (isAccessingDomain("93.184.216.34", "example.com")) {
    // 即使用户访问的是 www.example.com，也能检测到
}
```

---

## 实现细节

### addIPDomainMappings
```cpp
void DNSEngine::addIPDomainMappings(const std::string& ip, const std::vector<std::string>& domains) {
    if (domains.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(ip_domains_mutex_);

    auto& existing_domains = ip_to_domains_[ip];

    // Add all domains, avoiding duplicates
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
```

**特点**：
- ✅ 批量添加多个域名
- ✅ 自动去重
- ✅ 线程安全

### addIPsDomainsMappings
```cpp
void DNSEngine::addIPsDomainsMappings(const std::vector<std::string>& ips, const std::vector<std::string>& domains) {
    if (ips.empty() || domains.empty()) {
        return;
    }

    // Map all IPs to all domains in this response
    for (const auto& ip : ips) {
        if (!ip.empty()) {
            addIPDomainMappings(ip, domains);
        }
    }
}
```

**特点**：
- ✅ 多对多映射
- ✅ 一次调用建立所有关系
- ✅ 代表 DNS 响应中所有信息的关联性

---

## 测试结果

### 测试 1：基本映射
```
✓ Processed: example.com -> 93.184.216.34
IP: 93.184.216.34 -> [example.com]
```

### 测试 2：多域名映射
```
✓ Processed: www.example.com -> 93.184.216.35
✓ Processed: example.com -> 93.184.216.35
IP: 93.184.216.35 -> [www.example.com, example.com]
✓ Multiple domains correctly mapped to one IP
```

### 测试 3：去重
```
✓ Processed: example.com -> 93.184.216.34
✓ Processed: example.com -> 93.184.216.34  (duplicate)
IP: 93.184.216.34 -> [example.com]  (只有一个，没有重复)
```

---

## 性能考虑

### 时间复杂度
- **addIPDomainMappings**: O(n*m)
  - n = 新域名数量
  - m = 已存在域名数量（通常很小）
- **addIPsDomainsMappings**: O(k*n*m)
  - k = IP 数量
  - n = 域名数量
  - m = 每个 IP 已有域名数量

### 空间复杂度
- 每个 IP 可能关联多个域名
- 但通常一个 DNS 响应中的域名数量有限（< 10）
- 总体空间增长可控

### 优化建议
如果担心内存使用，可以：
1. 限制每个 IP 的最大域名数量
2. 使用 `std::unordered_set` 代替 `std::vector`（更快的查找）
3. 添加 LRU 淘汰策略

---

## 与 OC 代码的对应

### OC 代码
```objc
// 保存所有 IP 和所有域名的关系
std::vector<std::string> ips(ipList.begin(), ipList.end());
std::vector<std::string> domains(self->_mDNSDomains.begin(), self->_mDNSDomains.end());

EP_PROXY_EXTENSION.saveDNSCache(
    self.isForward,
    rawRequest,
    rawResponse,
    self.dnsQuestionDomain.UTF8String,
    ips,      // 所有 IP
    domains,  // 所有域名
    maxTTL
);
```

### C++ 代码
```cpp
// 收集所有 IP 和所有域名
std::vector<std::string> domains;
std::vector<std::string> ip_addresses;

// ... 解析并收集 ...

// 建立完整映射
if (!ip_addresses.empty() && !domains.empty()) {
    addIPsDomainsMappings(ip_addresses, domains);
}
```

**完全对应**：
- ✅ 都收集所有 IP
- ✅ 都收集所有域名
- ✅ 都建立完整的关联关系

---

## 总结

### 改进点
1. ✅ **支持批量映射**：`addIPDomainMappings` 和 `addIPsDomainsMappings`
2. ✅ **完整的关联关系**：响应中所有 IP 关联到所有域名
3. ✅ **更准确的信息**：包含 CNAME 链中的所有域名
4. ✅ **自动去重**：避免重复添加相同域名
5. ✅ **线程安全**：所有操作都有锁保护

### 使用场景
- ✅ 流量分析：知道 IP 对应的所有域名
- ✅ 日志增强：显示完整的域名信息
- ✅ 安全检测：检测通过 CNAME 访问的域名
- ✅ 调试：理解 DNS 解析链

### 测试验证
- ✅ 基本映射测试通过
- ✅ 多域名映射测试通过
- ✅ 去重功能测试通过
- ✅ 清除缓存测试通过

这个改进使得 IP-域名映射更加完整和实用！
