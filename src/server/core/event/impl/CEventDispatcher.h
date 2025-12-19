#ifndef CEVENTDISPATCHER_H
#define CEVENTDISPATCHER_H

#include "../../common/eventloop/CUVMultiloop.h"
#include "../base/IEventDispatcher.h"
#include <nlohmann/json.hpp>

/**
 * @brief 事件分发器实现类
 * @details 实现IEventDispatcher接口，负责事件的注册、注销、分发和处理
 */
class CEventDispatcher : public IEventDispatcher
{
public:
    // IEventDispatcher接口实现
    bool init() override;
    void registerEventCallback(EventType type, const EventData& eventData) override;
    void triggerEvent(const EventData &eventData) override;
private:
    CEventDispatcher();
    ~CEventDispatcher() override;

    // 禁止拷贝和移动
    CEventDispatcher(const CEventDispatcher&) = delete;
    CEventDispatcher& operator=(const CEventDispatcher&) = delete;
    CEventDispatcher(CEventDispatcher&&) = delete;
    CEventDispatcher& operator=(CEventDispatcher&&) = delete;

    friend class CEventDispatcherFactory;

private:
    OutSide::Common::EventLoop::CUVMultiloop* m_loop; // 事件循环
    std::atomic<bool> m_isInitialized;                // 初始化状态
};

#endif // CEVENTDISPATCHER_H
