# PCAP 文件解析测试说明

## 概述

`test_pcap` 是一个用于解析 PCAP 文件并提取网络流量域名信息的测试程序。

## 功能特性

- 解析标准 PCAP 文件格式
- 支持 BSD loopback 链路类型
- 自动识别 TCP 和 UDP 协议
- **解析 HTTP 请求**（包括 CONNECT 方法）
- **解析 HTTPS/TLS ClientHello**（提取 SNI）
- 提取 HTTP、HTTPS/TLS 等协议中的域名
- 统计流量信息和端口分布
- 显示完整的 HTTP 请求头
- 显示 TLS 版本和 SNI 信息

## 编译

```bash
cd build
cmake ..
make test_pcap
```

## 使用方法

### 基本用法

默认解析 `/tmp/baidu_req.pcap` 文件：

```bash
./test_pcap
```

### 指定 PCAP 文件

```bash
./test_pcap /path/to/your/file.pcap
```

## 输出信息

### 1. PCAP 文件信息

```
PCAP 文件信息:
  版本: 2.4
  链路类型: 0
  最大捕获长度: 524288
```

### 2. 解析进度

```
开始解析数据包...
总共处理了 1044 个数据包
```

### 3. 流统计信息

```
流统计信息
========================================
总流数: 89
提取到域名的流: 39
HTTP 请求流: 39
HTTPS/TLS 流: 39
```

### 4. HTTP 请求详情

显示每个流的 HTTP 请求信息：
- 请求方法（GET, POST, CONNECT 等）
- 请求路径
- HTTP 版本
- Host 头
- User-Agent 头
- 完整请求头（前 500 字节）

示例：
```
流: 127.0.0.1:61809 -> 127.0.0.1:7897
  数据包数: 4
  总字节数: 4321

  HTTP 请求 #1:
    方法: CONNECT
    路径: hm.baidu.com:443
    版本: HTTP/1.1
    Host: hm.baidu.com:443
    User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36

    完整请求头（前 500 字节）:
    ----------------------------------------
CONNECT hm.baidu.com:443 HTTP/1.1
Host: hm.baidu.com:443
Proxy-Connection: keep-alive
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36

    ----------------------------------------
```

### 5. HTTPS/TLS ClientHello 详情

显示每个流的 TLS ClientHello 信息：
- TLS 版本
- SNI (Server Name Indication) - 域名

示例：
```
流: 127.0.0.1:61809 -> 127.0.0.1:7897
  数据包数: 4
  总字节数: 4321

  TLS ClientHello #1:
    TLS 版本: TLS 1.0
    SNI (域名): hm.baidu.com
```

### 6. FlowEngine 提取的域名

显示通过 FlowEngine 提取到的域名：
```
流: 127.0.0.1:61809 -> 127.0.0.1:7897
  协议: TCP
  数据包数: 4
  总字节数: 4321
  域名: hm.baidu.com
```

### 7. 端口分布

统计所有流的目标端口分布：
```
端口分布
========================================
  端口 7897: 44 个流
  端口 443 (HTTPS): 20 个流
  端口 80 (HTTP): 5 个流
```

## 支持的协议

### HTTP 协议
- **所有 HTTP 方法**: GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH, CONNECT
- **提取信息**:
  - 请求方法
  - 请求路径
  - HTTP 版本
  - Host 头（域名）
  - User-Agent 头
  - 完整请求头（前 500 字节）
- **特殊支持**: HTTP CONNECT 方法（用于 HTTPS 代理）

### HTTPS/TLS 协议
- **TLS 版本识别**: TLS 1.0, 1.1, 1.2, 1.3
- **提取信息**:
  - TLS 版本
  - SNI (Server Name Indication) - 域名
  - ClientHello 握手信息
- **工作原理**: 解析 TLS ClientHello 消息中的扩展字段

### DNS 协议
- **端口**: 53
- **提取信息**:
  - DNS 查询域名
  - DNS 响应域名（包括 CNAME）

## 技术细节

### HTTP 请求解析

程序会检测所有 TCP 流量中的 HTTP 请求，不限于特定端口。解析逻辑：

1. 检查数据包是否以 HTTP 方法开头（GET, POST, CONNECT 等）
2. 解析请求行（方法、路径、版本）
3. 解析 HTTP 头部：
   - Host 头：提取域名
   - User-Agent 头：识别客户端
4. 保存完整请求头（前 500 字节）

### TLS ClientHello 解析

程序会检测所有 TCP 流量中的 TLS ClientHello，不限于特定端口。解析逻辑：

1. 检查 TLS 记录类型（0x16 = Handshake）
2. 检查握手类型（0x01 = ClientHello）
3. 解析 TLS 版本
4. 遍历扩展字段查找 SNI 扩展（type = 0）
5. 提取 SNI 中的域名

### 代理流量支持

程序特别支持通过 HTTP 代理的流量：
- **HTTP CONNECT**: 用于建立 HTTPS 隧道
- **代理端口**: 自动识别非标准端口（如 7897）
- **域名提取**: 从 CONNECT 请求的路径中提取域名

示例 PCAP 文件中的流量都是通过本地代理（端口 7897）转发的 HTTPS 流量。

## 技术细节

### PCAP 文件格式

程序支持标准的 PCAP 文件格式：
- 全局文件头（24 字节）
- 数据包头（16 字节）+ 数据包数据

### 链路类型支持

- **NULL (0)**: BSD loopback
  - 4 字节 loopback 头部
  - IP 头部
  - TCP/UDP 头部
  - 应用层数据

### 数据包解析流程

1. 读取 PCAP 文件头
2. 循环读取每个数据包：
   - 读取数据包头（时间戳、长度）
   - 读取数据包数据
   - 跳过链路层头部（loopback: 4 字节）
   - 解析 IP 头部（获取源/目标 IP、协议类型）
   - 解析传输层头部（TCP/UDP，获取端口）
   - 提取应用层数据
   - 使用 FlowEngine 处理数据包
   - 记录流统计信息

## 示例输出

基于 `/tmp/baidu_req.pcap` 的测试结果：

- **总数据包**: 1044 个
- **总流数**: 89 个
- **成功提取域名**: 39 个流 (43.8%)
- **HTTP 请求**: 39 个流（全部为 CONNECT 方法）
- **HTTPS/TLS**: 39 个流（全部包含 TLS ClientHello）

### 主要发现

1. **代理流量**: 所有流量都通过本地代理（端口 7897）转发
2. **HTTPS 隧道**: 使用 HTTP CONNECT 方法建立 HTTPS 隧道
3. **TLS 版本**: 大部分使用 TLS 1.0（实际协商版本可能更高）
4. **域名提取**: 三种方式都成功提取到域名：
   - HTTP CONNECT 请求路径
   - TLS ClientHello SNI
   - FlowEngine 协议解析

### 提取到的域名示例

- **百度相关**:
  - www.baidu.com
  - hm.baidu.com
  - chat.baidu.com
  - mbd.baidu.com
  - sp1.baidu.com
  - sp2.baidu.com
  - hpd.baidu.com
  - passport.baidu.com
  - pss.bdstatic.com
  - himg.bdimg.com
  - aisearch.cdn.bcebos.com
  - hectorstatic.baidu.com

- **其他服务**:
  - api.wetab.link
  - encrypted-tbn0.gstatic.com
  - data-api.similarsites.com
  - config.immersivetranslate.com
  - download.clashverge.dev

## 注意事项

1. **链路类型**: 目前主要支持 BSD loopback (NULL) 类型，其他链路类型（如以太网）可能需要调整头部偏移量

2. **内存使用**: 程序会将整个数据包读入内存，对于大型 PCAP 文件可能需要较多内存

3. **协议支持**: 只能提取支持的协议（HTTP、HTTPS、DNS）中的域名，其他协议的流量会被统计但不会提取域名

4. **本地回环**: 示例 PCAP 文件包含的是本地回环流量（127.0.0.1），实际网络流量的 IP 地址会不同

## 扩展建议

如需支持其他链路类型，可以修改 `parsePacket()` 函数中的偏移量：

```cpp
// 以太网: 14 字节
size_t offset = 14;

// BSD loopback: 4 字节
size_t offset = 4;

// Linux cooked capture: 16 字节
size_t offset = 16;
```

## 相关文件

- 源代码: `tests/test_pcap.cpp`
- CMake 配置: `CMakeLists.txt`
- 依赖库: `flow_lib`, `dns_lib`, `protocol_lib`

## 版本信息

- 创建日期: 2026-02-09
- 作者: jacky & Claude
- 版本: 1.0.0
