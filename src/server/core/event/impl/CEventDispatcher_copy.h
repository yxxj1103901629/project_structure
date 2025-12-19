// #ifndef CEVENTDISPATCHER_COPY_H
// #define CEVENTDISPATCHER_COPY_H

// #include "../../../common/network/base/CUVLoop.h"
// #include "../../../common/network/base/NetworkType.h"
// #include "../base/IEventDispatcher.h"

// #include <atomic>
// #include <chrono>
// #include <mutex>
// #include <unordered_map>
// #include <vector>

// namespace Server {
// namespace Core {
// namespace Event {

// /**
//  * @brief 事件分发器实现类
//  * @details 实现IEventDispatcher接口，负责事件的注册、注销、分发和处理
//  */
// class CEventDispatcher : public IEventDispatcher
// {
// public:
//     CEventDispatcher();
//     virtual ~CEventDispatcher();

//     // 禁止拷贝和移动
//     CEventDispatcher(const CEventDispatcher&) = delete;
//     CEventDispatcher& operator=(const CEventDispatcher&) = delete;
//     CEventDispatcher(CEventDispatcher&&) = delete;
//     CEventDispatcher& operator=(CEventDispatcher&&) = delete;

//     // IEventDispatcher接口实现
//     bool init() override;
//     void stop() override;
//     bool registerHandler(Common::Network::NetworkType networkType,
//                          Common::Network::NetworkEventType eventType,
//                          EventHandler handler) override;
//     bool unregisterHandler(Common::Network::NetworkType networkType,
//                            Common::Network::NetworkEventType eventType) override;
//     bool dispatchEvent(const EventData& eventData) override;
//     EventData convertToEvent(Common::Network::NetworkType networkType,
//                              const std::string& connectionId,
//                              const std::string& sourceAddress,
//                              const std::string& destinationAddress,
//                              Common::Network::NetworkEventType networkEventType,
//                              const std::string& data) override;
//     Common::Network::CUVLoop* getEventLoop() override;
//     void setEventLoop(Common::Network::CUVLoop* loop) override;
//     void postTask(const std::function<void()>& task) override;

// private:
//     /**
//      * @brief 生成当前时间戳
//      * @return 格式化的时间戳字符串
//      */
//     std::string generateTimestamp() const;

//     /**
//      * @brief 构建事件处理器键
//      * @param networkType 网络类型
//      * @param eventType 事件类型
//      * @return 组合后的键字符串
//      */
//     std::string buildHandlerKey(Common::Network::NetworkType networkType,
//                                 Common::Network::NetworkEventType eventType) const;

// private:
//     std::atomic<bool> m_isInitialized;     // 初始化标志
//     Common::Network::CUVLoop* m_eventLoop; // 事件循环实例
//     std::mutex m_eventLoopMutex;           // 事件循环互斥锁

//     // 事件处理器映射表：键为"networkType|eventType"，值为事件处理器列表
//     std::unordered_map<std::string, std::vector<EventHandler>> m_eventHandlers;
//     std::mutex m_handlersMutex; // 事件处理器互斥锁
// };

// } // namespace Event
// } // namespace Core
// } // namespace Server

// #endif // CEVENTDISPATCHER_COPY_H
