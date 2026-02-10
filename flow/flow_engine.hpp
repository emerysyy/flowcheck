//
//  flow_engine.hpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#ifndef flow_engine_hpp
#define flow_engine_hpp

#include <stdio.h>
#include <memory>
#include "flow_defines.h"

namespace flow {

// 前向声明
class DNSEngine;
class Detector;

/**
 * FlowEngine - 单例模式
 *
 * 管理网络流量处理、协议检测和 DNS 缓存。
 * 使用 getInstance() 获取单例实例。
 *
 * 线程安全：单例实例是线程安全的。
 * 所有方法通过 DNSEngine 内部的锁机制保证线程安全。
 */
class FlowEngine {

public:
    /**
     * 获取单例实例
     * @return FlowEngine 单例实例的引用
     */
    static FlowEngine& getInstance();

    // 删除拷贝构造函数和赋值运算符
    FlowEngine(const FlowEngine&) = delete;
    FlowEngine& operator=(const FlowEngine&) = delete;

    /**
     * 流量到达处理
     * @param ctx 流量上下文
     */
    void flowArrive(FlowContext& ctx);

    /**
     * 流量打开处理
     * @param ctx 流量上下文
     */
    void flowOpen(FlowContext& ctx);

    /**
     * 处理出站数据包（不处理 DNS 响应）
     * @param ctx 流量上下文
     * @param pkt 出站数据包
     */
    void flowSend(FlowContext& ctx, const PacketView& pkt);

    /**
     * 处理出站数据包（处理 DNS 响应）
     * @param ctx 流量上下文
     * @param pkt 出站数据包
     * @param response DNS 响应输出参数（如果缓存命中）
     * @return true 表示有 DNS 响应需要发送回去（缓存命中）
     */
    bool flowSend(FlowContext& ctx, const PacketView& pkt, PacketView& response);

    /**
     * 处理入站数据包
     * @param ctx 流量上下文
     * @param pkt 入站数据包
     */
    void flowRecv(FlowContext& ctx, const PacketView& pkt);

    /**
     * 流量关闭处理
     * @param ctx 流量上下文
     */
    void flowClose(FlowContext& ctx);

    /**
     * 获取 DNS 引擎（用于高级操作）
     * @return DNS 引擎的引用
     */
    DNSEngine& getDNSEngine();

private:
    FlowEngine();
    ~FlowEngine();

    /**
     * 尝试补全域名信息（仅从 DNS 缓存）
     *
     * 从 DNS 缓存中通过目标 IP 查询域名。
     * 用于流量到达时（还没有数据包）的域名补全。
     *
     * @param ctx 流量上下文
     * @return true 表示新获得了域名，false 表示没有新域名或已有域名
     */
    bool tryResolveDomain(FlowContext& ctx);

    /**
     * 尝试补全域名信息（从 DNS 缓存和数据包）
     *
     * 从以下来源尝试获取域名：
     * 1. DNS 缓存（通过目标 IP 查询）
     * 2. 数据包协议解析（HTTP、TLS 等）
     *
     * @param ctx 流量上下文
     * @param pkt 数据包（用于协议解析）
     * @return true 表示新获得了域名，false 表示没有新域名或已有域名
     */
    bool tryResolveDomain(FlowContext& ctx, const PacketView& pkt);

    /**
     * 重新评估流量决策
     *
     * 当域名或其他属性发生变化时调用，重新计算：
     * - flow_decision（允许/阻止）
     * - path_decision（本地/代理）
     *
     * @param ctx 流量上下文
     */
    void reevaluateDecision(FlowContext& ctx);

    std::unique_ptr<DNSEngine> dns_engine_;  // DNS 引擎
    std::unique_ptr<Detector> detector_;     // 协议检测器
};

}


#endif /* flow_engine_hpp */
