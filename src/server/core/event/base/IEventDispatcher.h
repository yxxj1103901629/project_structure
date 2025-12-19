#ifndef IEVENTDISPATCHER_H
#define IEVENTDISPATCHER_H

#include "../../common/network/base/NetworkType.h"
#include <functional>
#include <string>

using EventType = int; // 事件类型可以根据具体需求定义

struct EventData
{
    OutSide::Common::Network::NetworkType networkType;           // 网络类型
    OutSide::Common::Network::NetworkEventType networkEventType; // 网络事件类型
    std::string source;      // 源地址，可能是topic，可能是IP:Port等
    std::string destination; // 目标地址，可能是topic，可能是IP:Port等
    std::string time;        // 时间
};

using EventHandler = std::function<void(const EventData &data)>;

/**
 * @brief 事件分发器接口
 * @details 作为业务逻辑层与网络传输模块之间的桥梁，实现数据解耦和高效的事件分发
 */
class IEventDispatcher
{
public:
    virtual ~IEventDispatcher() = default;

    /**
     * @brief 初始化事件分发器
     * @param config 可选的配置参数，默认为空
     * @return 初始化是否成功
     */
    virtual bool init() = 0;
    /**
     * @brief 注册特定事件类型的回调函数
     * @param type 事件类型
     * @param callback 事件数据回调函数
     */
    virtual void registerEventCallback(EventType type, const EventData &eventData) = 0;
    /**
     * @brief 触发事件
     * @param eventData 事件数据
     */
    virtual void triggerEvent(const EventData &eventData) = 0;
};

#endif // IEVENTDISPATCHER_H
