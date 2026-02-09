# FlowCheck 项目指南（给 Claude AI）

本文档为 Claude AI 助手提供项目上下文和开发指引。

## 项目概述

FlowCheck 是一个高性能的网络流量检测与分析系统，用 C++17 实现。

### 核心功能
- 网络协议检测（11 种协议）
- 域名提取（DNS、HTTP、HTTPS/TLS）
- DNS 响应缓存（LRU）
- IP-域名映射（多对多）
- 流量决策引擎

### 技术栈
- **语言**：C++17
- **构建系统**：CMake 3.15+
- **编译器**：支持 C++17 的编译器（AppleClang, GCC, MSVC）
- **平台**：跨平台（macOS, Linux, Windows）

## 项目结构

```
flowcheck/
├── dns/           # DNS 解析模块（独立）
├── protocol/      # 协议解析器（11 种协议）
├── flow/          # 流量管理核心（依赖 dns 和 protocol）
├── tests/         # 测试用例（5 个测试程序）
├── docs/          # 项目文档（全中文）
├── scripts/       # 工具脚本
├── data/          # 测试数据（1068 个有效流）
└── build/         # 编译输出目录
```

### 模块依赖关系
```
flow_lib → dns_lib + protocol_lib
测试程序 → flow_lib + dns_lib + protocol_lib
```

## 代码规范

### C++ 编码规范

1. **命名约定**
   - 类名：PascalCase（如 `FlowEngine`, `DNSEngine`）
   - 函数名：camelCase（如 `flowSend`, `handleQuery`）
   - 变量名：snake_case（如 `dns_cache_`, `session_id`）
   - 成员变量：以下划线结尾（如 `dns_engine_`, `detector_`）
   - 常量：UPPER_CASE（如 `MAX_CACHE_SIZE`）

2. **代码风格**
   - 缩进：4 个空格（不使用 Tab）
   - 大括号：K&R 风格（函数和类的大括号另起一行）
   - 行宽：建议不超过 100 字符
   - 注释：使用中文注释

3. **C++17 特性使用**
   - ✅ 使用 `std::unique_ptr` 和 `std::shared_ptr`
   - ✅ 使用 `std::optional` 表示可选值
   - ✅ 使用 `std::string_view` 避免拷贝
   - ✅ 使用结构化绑定（`auto [a, b] = ...`）
   - ✅ 使用 `if constexpr` 编译期分支
   - ❌ 避免使用原始指针（除非必要）
   - ❌ 避免使用 `new/delete`（使用智能指针）

4. **内存管理**
   - 遵循 RAII 原则
   - 使用智能指针管理资源
   - 避免内存泄漏
   - 零拷贝设计（使用 `PacketView`）

5. **线程安全**
   - 单例模式使用 C++11 静态局部变量（线程安全）
   - 共享资源使用 `std::mutex` 保护
   - DNS 缓存是线程安全的
   - `FlowContext` 不是线程安全的（每个线程独立实例）

## 架构关键点

### 1. 单例模式

**FlowEngine 是单例**，原因：
- 全局共享 DNS 缓存
- 避免重复 DNS 查询
- 简化资源管理

```cpp
FlowEngine& engine = FlowEngine::getInstance();
```

### 2. 成员变量 vs 静态变量

**重要**：所有组件都是成员变量，不是静态变量
- ✅ `dns_engine_` 是成员变量
- ✅ `detector_` 是成员变量
- ❌ 不要使用 `static` 局部变量（除了单例）

### 3. 函数重载

**flowSend() 有两个重载版本**：
```cpp
// 版本 1：不需要 DNS 响应
void flowSend(FlowContext& ctx, const PacketView& pkt);

// 版本 2：需要 DNS 响应（返回 true 表示缓存命中）
bool flowSend(FlowContext& ctx, const PacketView& pkt, PacketView& response);
```

**为什么不用可选参数（指针）？**
- 类型安全（引用不能为空）
- 语义清晰（返回值表示是否有响应）
- 避免空指针检查

### 4. IP-域名映射

**多对多映射**：
- 一个 IP 可以对应多个域名
- 一个域名可以对应多个 IP
- 支持 CNAME 链
- 批量映射更新

### 5. 零拷贝设计

**PacketView 结构**：
```cpp
struct PacketView {
    const uint8_t* data;  // 不拥有数据
    size_t len;
};
```

## 常见任务

### 添加新协议支持

1. 创建解析器文件：
```bash
touch protocol/new_protocol_parser.cpp
touch protocol/new_protocol_parser.hpp
```

2. 实现解析器：
```cpp
class NewProtocolParser {
public:
    static bool isProtocol(const PacketView& pkt);
    static std::string extractDomain(const PacketView& pkt);
};
```

3. 注册到 Detector（`flow/flow_detector.cpp`）：
```cpp
std::string Detector::detectProtocol(...) {
    if (NewProtocolParser::isProtocol(pkt)) {
        return "NewProtocol";
    }
    // ...
}
```

4. 更新 CMakeLists.txt：
```cmake
set(PROTOCOL_SOURCES
    ...
    protocol/new_protocol_parser.cpp
)
```

### 添加测试用例

1. 创建测试文件：`tests/test_new_feature.cpp`
2. 更新 CMakeLists.txt：
```cmake
add_executable(test_new_feature tests/test_new_feature.cpp)
target_link_libraries(test_new_feature flow_lib dns_lib protocol_lib)
```
3. 编译运行：
```bash
cd build
cmake ..
make test_new_feature
./test_new_feature
```

### 修改代码后的流程

```bash
# 1. 修改代码
vim flow/flow_engine.cpp

# 2. 编译
cd build
make

# 3. 运行测试
./flowcheck_test
./test_data_advanced

# 4. 如果测试通过，提交代码
git add flow/flow_engine.cpp
git commit -m "feat: 添加 XXX 功能"
```

## 重要注意事项

### ⚠️ 不要做的事情

1. **不要修改静态变量为成员变量**
   - 已经完成重构，所有组件都是成员变量

2. **不要破坏单例模式**
   - FlowEngine 必须保持单例
   - 不要创建多个实例

3. **不要修改 PacketView 结构**
   - 零拷贝设计，不要添加数据拷贝

4. **不要在 FlowContext 中添加锁**
   - FlowContext 不是线程安全的（设计如此）
   - 每个线程使用独立的 FlowContext

5. **不要删除 Co-Authored-By**
   - Git 提交消息必须包含 Co-Authored-By

6. **不要使用英文注释**
   - 所有注释必须使用中文

### ✅ 应该做的事情

1. **保持代码风格一致**
   - 遵循现有的命名和格式规范

2. **添加中文注释**
   - 所有新代码都要有中文注释

3. **更新文档**
   - 修改功能后更新相应文档

4. **运行测试**
   - 修改代码后必须运行测试验证

5. **检查线程安全**
   - 修改共享资源时考虑线程安全

## 文档链接

### 主要文档
- [README.md](README.md) - 项目主文档
- [docs/测试说明.md](docs/测试说明.md) - 测试用例说明
- [docs/架构设计.md](docs/架构设计.md) - 系统架构设计
- [docs/API文档.md](docs/API文档.md) - API 接口文档
- [docs/项目结构.md](docs/项目结构.md) - 目录结构说明
- [docs/编译说明.md](docs/编译说明.md) - 编译配置指南

### 开发日志
- [docs/开发日志/](docs/开发日志/) - 设计决策和改进记录

## 性能指标

基于 1068 个真实流的测试：
- **域名提取成功率**：99%
- **DNS 流识别率**：100%
- **HTTPS 流识别率**：100%
- **HTTP 流识别率**：100%
- **平均数据包/流**：26+

## 编译环境

- **操作系统**：macOS Darwin 24.6.0
- **编译器**：AppleClang 16.0.0
- **C++ 标准**：C++17
- **构建类型**：Release (-O3)

## 快速命令参考

```bash
# 编译项目
cd build && cmake .. && make -j4

# 运行所有测试
./flowcheck_test && ./test_dns_mapping && ./test_with_data && ./test_data_advanced

# 清理无效数据
../scripts/scan_invalid_dirs.sh
../scripts/clean_invalid_dirs.sh

# 查看项目结构
tree -L 2 -I 'build|data'

# 查看文档
ls -la docs/
```

## 调试技巧

1. **查看编译警告**：
```bash
make 2>&1 | grep warning
```

2. **运行单个测试**：
```bash
./flowcheck_test  # 基础测试
./test_data_advanced  # 高级测试
```

3. **查看 DNS 缓存状态**：
   - 使用 `test_dns_mapping` 测试

4. **分析域名提取失败**：
```bash
./test_analyze_failures
```

## 版本信息

- **版本**：1.0.0
- **创建日期**：2026-02-09
- **维护者**：jacky
- **许可证**：Copyright © 2026 WireGuard LLC

## 联系方式

如需帮助，请参考：
- 查看文档：`docs/` 目录
- 运行测试：`build/` 目录
- 查看示例：`tests/` 目录

---

**最后更新**：2026-02-09

**注意**：本文档专门为 Claude AI 助手编写，帮助理解项目结构和开发规范。
