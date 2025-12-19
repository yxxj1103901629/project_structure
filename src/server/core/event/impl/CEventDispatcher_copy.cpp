// #include "CEventDispatcher.h"

// #include <sstream>
// #include <iomanip>

// namespace Server {
// namespace Core {
// namespace Event {

// CEventDispatcher::CEventDispatcher()
//     : m_isInitialized(false)
//     , m_eventLoop(nullptr)
// {}

// CEventDispatcher::~CEventDispatcher() {
//     stop();
// }

// bool CEventDispatcher::init() {
//     if (m_isInitialized) {
//         return true;
//     }

//     // 如果没有设置事件循环，则使用默认的全局事件循环
//     if (m_eventLoop == nullptr) {
//         m_eventLoop = Common::Network::CUVLoop::getInstance();
//     }

//     m_isInitialized = true;
//     return true;
// }

// void CEventDispatcher::stop() {
//     if (!m_isInitialized) {
//         return;
//     }

//     // 清空事件处理器
//     {
//         std::lock_guard<std::mutex> lock(m_handlersMutex);
//         m_eventHandlers.clear();
//     }

//     m_isInitialized = false;
// }

// bool CEventDispatcher::registerHandler(Common::Network::NetworkType networkType,
//                                        Common::Network::NetworkEventType eventType,
//                                        EventHandler handler) {
//     if (!m_isInitialized) {
//         return false;
//     }

//     // 构建事件处理器键
//     std::string key = buildHandlerKey(networkType, eventType);

//     // 注册事件处理器
//     {
//         std::lock_guard<std::mutex> lock(m_handlersMutex);
//         m_eventHandlers[key].push_back(handler);
//     }

//     return true;
// }

// bool CEventDispatcher::unregisterHandler(Common::Network::NetworkType networkType,
//                                          Common::Network::NetworkEventType eventType) {
//     if (!m_isInitialized) {
//         return false;
//     }

//     // 构建事件处理器键
//     std::string key = buildHandlerKey(networkType, eventType);

//     // 注销事件处理器
//     {
//         std::lock_guard<std::mutex> lock(m_handlersMutex);
//         auto it = m_eventHandlers.find(key);
//         if (it != m_eventHandlers.end()) {
//             m_eventHandlers.erase(it);
//             return true;
//         }
//     }

//     return false;
// }

// bool CEventDispatcher::dispatchEvent(const EventData& eventData) {
//     if (!m_isInitialized) {
//         return false;
//     }

//     // 构建事件处理器键
//     std::string key = buildHandlerKey(eventData.source.networkType, eventData.eventType);

//     // 查找并调用对应的事件处理器
//     {
//         std::lock_guard<std::mutex> lock(m_handlersMutex);
//         auto it = m_eventHandlers.find(key);
//         if (it != m_eventHandlers.end()) {
//             // 遍历并调用所有注册的事件处理器
//             for (const auto& handler : it->second) {
//                 handler(eventData);
//             }
//             return true;
//         }
//     }

//     // 尝试查找通用事件处理器（不区分网络类型）
//     {
//         std::lock_guard<std::mutex> lock(m_handlersMutex);
//         std::string genericKey = buildHandlerKey(Common::Network::NetworkType::CUSTOM, eventData.eventType);
//         auto it = m_eventHandlers.find(genericKey);
//         if (it != m_eventHandlers.end()) {
//             // 遍历并调用所有注册的通用事件处理器
//             for (const auto& handler : it->second) {
//                 handler(eventData);
//             }
//             return true;
//         }
//     }

//     return false;
// }

// EventData CEventDispatcher::convertToEvent(Common::Network::NetworkType networkType,
//                                          const std::string& connectionId,
//                                          const std::string& sourceAddress,
//                                          const std::string& destinationAddress,
//                                          Common::Network::NetworkEventType networkEventType,
//                                          const std::string& data) {
//     EventData eventData;
    
//     // 设置事件类型
//     eventData.eventType = networkEventType;
    
//     // 设置事件源
//     eventData.source.networkType = networkType;
//     eventData.source.connectionId = connectionId;
//     eventData.source.sourceAddress = sourceAddress;
//     eventData.source.destinationAddress = destinationAddress;
    
//     // 设置事件数据
//     eventData.data = data;
    
//     // 设置时间戳
//     eventData.timestamp = generateTimestamp();
    
//     // 设置附加信息（可以根据需要扩展）
//     std::stringstream ss;
//     ss << "NetworkType: " << static_cast<int>(networkType) << ", ConnectionId: " << connectionId;
//     eventData.additionalInfo = ss.str();
    
//     return eventData;
// }

// Common::Network::CUVLoop* CEventDispatcher::getEventLoop() {
//     std::lock_guard<std::mutex> lock(m_eventLoopMutex);
//     return m_eventLoop;
// }

// void CEventDispatcher::setEventLoop(Common::Network::CUVLoop* loop) {
//     if (m_isInitialized) {
//         return;
//     }
    
//     std::lock_guard<std::mutex> lock(m_eventLoopMutex);
//     m_eventLoop = loop;
// }

// void CEventDispatcher::postTask(const std::function<void()>& task) {
//     if (!m_isInitialized || m_eventLoop == nullptr) {
//         return;
//     }
    
//     m_eventLoop->postTask(task);
// }

// std::string CEventDispatcher::generateTimestamp() const {
//     auto now = std::chrono::system_clock::now();
//     auto now_c = std::chrono::system_clock::to_time_t(now);
//     auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
//     std::stringstream ss;
//     ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
//        << "." << std::setfill('0') << std::setw(3) << now_ms.count();
    
//     return ss.str();
// }

// std::string CEventDispatcher::buildHandlerKey(Common::Network::NetworkType networkType,
//                                              Common::Network::NetworkEventType eventType) const {
//     std::stringstream ss;
//     ss << static_cast<int>(networkType) << "|" << static_cast<int>(eventType);
//     return ss.str();
// }

// std::shared_ptr<IEventDispatcher> EventDispatcherFactory::createEventDispatcher() {
//     std::shared_ptr<CEventDispatcher> dispatcher = std::make_shared<CEventDispatcher>();
//     if (dispatcher->init()) {
//         return dispatcher;
//     }
//     return nullptr;
// }

// } // namespace Event
// } // namespace Core
// } // namespace Server
