//
//  flow_detector.hpp
//  AppProxy
//
//  Created by jacky on 2026/2/9.
// 
//

#ifndef flow_detector_hpp
#define flow_detector_hpp

#include <stdio.h>
#include "flow_defines.h"
#include "../protocol/protocol.h"

namespace flow {

/**
 * 协议检测器
 *
 * 用于检测网络协议类型并从数据包中提取域名信息
 */
class Detector {

public:

    /**
     * 从数据包中提取域名
     * @param ctx 流量上下文
     * @param pkt 数据包视图
     * @param protocol 输出参数，检测到的协议类型
     * @return 提取的域名，如果未找到则返回空
     */
    std::optional<std::string> extractDomain(const FlowContext &ctx, const PacketView &pkt, proto::ProtocolType &protocol);

    /**
     * 检测协议类型
     * @param ctx 流量上下文
     * @param pkt 数据包视图
     * @return 检测到的协议类型
     */
    proto::ProtocolType detectProtocol(const FlowContext &ctx, const PacketView &pkt);
};

}

#endif /* flow_detector_hpp */
