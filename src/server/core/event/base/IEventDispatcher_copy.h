// #ifndef IEVENTDISPATCHER_COPY_H
// #define IEVENTDISPATCHER_COPY_H

// #include "../../../common/network/base/NetworkType.h"
// #include "../../../common/network/base/CUVLoop.h"

// #include <string>
// #include <functional>
// #include <memory>

// namespace Server {
// namespace Core {
// namespace Event {

// /**
//  * @brief 事件源标识结构体
//  * @details 用于唯一标识事件的来源，包括网络类型和具体的连接标识
//  */
// struct EventSource {
//     Common::Network::NetworkType networkType; // 网络类型
//     std::string connectionId;                 // 连接标识（如TCP的IP:Port，MQTT的客户端ID）
//     std::string sourceAddress;                // 源地址
//     std::string destinationAddress;           // 目标地址

//     /**
//      * @brief 转换为字符串表示
//      * @return 字符串表示
//      */
//     std::string toString() const {
//         std::string typeStr;
//         switch (networkType) {
//             case Common::Network::NetworkType::TCP_SERVER: typeStr = "TCP_SERVER"; break;
//             case Common::Network::NetworkType::TCP_CLIENT: typeStr = "TCP_CLIENT"; break;
//             case Common::Network::NetworkType::MQTT_CLIENT: typeStr = "MQTT_CLIENT"; break;
//             case Common::Network::NetworkType::UDP_SERVER: typeStr = "UDP_SERVER"; break;
//             case Common::Network::NetworkType::UDP_CLIENT: typeStr = "UDP_CLIENT"; break;
//             case Common::Network::NetworkType::WEBSOCKET: typeStr = "WEBSOCKET"; break;
//             default: typeStr = "CUSTOM"; break;
//         }
//         return typeStr + "|" + connectionId + "|" + sourceAddress + "|" + destinationAddress;
//     }

//     /**
//      * @brief 比较运算符
//      */
//     bool operator==(const EventSource& other) const {
//         return networkType == other.networkType &&
//                connectionId == other.connectionId &&
//                sourceAddress == other.sourceAddress &&
//                destinationAddress == other.destinationAddress;
//     }
// };

// /**
//  * @brief 事件数据结构体
//  * @details 用于封装事件相关的数据，包括事件类型、事件源和具体的数据内容
//  */
// struct EventData {
//     Common::Network::NetworkEventType eventType; // 事件类型
//     EventSource source;                          // 事件源
//     std::string data;                            // 事件数据内容
//     std::string timestamp;                       // 事件时间戳
//     std::string additionalInfo;                  // 附加信息
// };

// /**
//  * @brief 事件处理回调函数类型定义
//  */
// typedef std::function<void(const EventData& eventData)> EventHandler;

// /**
//  * @brief 事件分发器接口
//  * @details 作为业务逻辑层与网络传输模块之间的桥梁，实现数据解耦和高效的事件分发
//  */
// class IEventDispatcher {
// public:
//     virtual ~IEventDispatcher() = default;

//     /**
//      * @brief 初始化事件分发器
//      * @details 初始化事件分发器的相关资源和设置，包括与CUVLoop的集成
//      * @return 初始化是否成功
//      */
//     virtual bool init() = 0;

//     /**
//      * @brief 停止事件分发器
//      * @details 停止事件分发器的运行并释放相关资源
//      */
//     virtual void stop() = 0;

//     /**
//      * @brief 注册事件处理回调
//      * @details 为指定的网络类型和事件类型注册事件处理回调函数
//      * @param networkType 网络类型
//      * @param eventType 事件类型
//      * @param handler 事件处理回调函数
//      * @return 注册是否成功
//      */
//     virtual bool registerHandler(Common::Network::NetworkType networkType,
//                                  Common::Network::NetworkEventType eventType,
//                                  EventHandler handler) = 0;

//     /**
//      * @brief 注销事件处理回调
//      * @details 注销指定网络类型和事件类型的事件处理回调函数
//      * @param networkType 网络类型
//      * @param eventType 事件类型
//      * @return 注销是否成功
//      */
//     virtual bool unregisterHandler(Common::Network::NetworkType networkType,
//                                    Common::Network::NetworkEventType eventType) = 0;

//     /**
//      * @brief 分发事件
//      * @details 将事件分发给注册的处理回调函数
//      * @param eventData 事件数据
//      * @return 分发是否成功
//      */
//     virtual bool dispatchEvent(const EventData& eventData) = 0;

//     /**
//      * @brief 网络数据转换为业务事件
//      * @details 将网络传输模块接收到的数据转换为业务事件
//      * @param networkType 网络类型
//      * @param connectionId 连接标识
//      * @param sourceAddress 源地址
//      * @param destinationAddress 目标地址
//      * @param networkEventType 网络事件类型
//      * @param data 网络数据
//      * @return 转换后的事件数据
//      */
//     virtual EventData convertToEvent(Common::Network::NetworkType networkType,
//                                      const std::string& connectionId,
//                                      const std::string& sourceAddress,
//                                      const std::string& destinationAddress,
//                                      Common::Network::NetworkEventType networkEventType,
//                                      const std::string& data) = 0;

//     /**
//      * @brief 获取事件循环实例
//      * @details 获取与事件分发器关联的CUVLoop事件循环实例
//      * @return CUVLoop实例指针
//      */
//     virtual Common::Network::CUVLoop* getEventLoop() = 0;

//     /**
//      * @brief 设置事件循环实例
//      * @details 设置与事件分发器关联的CUVLoop事件循环实例
//      * @param loop CUVLoop实例指针
//      */
//     virtual void setEventLoop(Common::Network::CUVLoop* loop) = 0;

//     /**
//      * @brief 向事件循环中提交任务
//      * @details 将任务提交到CUVLoop事件循环中执行
//      * @param task 要执行的任务
//      */
//     virtual void postTask(const std::function<void()>& task) = 0;
// };

// /**
//  * @brief 事件分发器工厂类
//  * @details 用于创建事件分发器实例，支持不同类型的事件分发器实现
//  */
// class EventDispatcherFactory {
// public:
//     /**
//      * @brief 创建事件分发器实例
//      * @return 事件分发器实例指针
//      */
//     static std::shared_ptr<IEventDispatcher> createEventDispatcher();
// };

// } // namespace Event
// } // namespace Core
// } // namespace Server

// #endif // IEVENTDISPATCHER_COPY_H
