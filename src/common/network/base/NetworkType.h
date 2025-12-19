#ifndef NETWORKTYPE_H
#define NETWORKTYPE_H

namespace OutSide {
namespace Common {
namespace Network {

/**
 * @brief 网络类型枚举
 */
enum class NetworkType {
    TCP_SERVER = 0,  // TCP服务器
    TCP_CLIENT = 1,  // TCP客户端
    MQTT_CLIENT = 2, // MQTT客户端
    UDP_SERVER = 3,  // UDP服务器
    UDP_CLIENT = 4,  // UDP客户端
    WEBSOCKET = 5,   // WebSocket
};

/**
 * @brief 网络事件类型枚举
 */
enum class NetworkEventType {
    CONNECTED,           // 连接成功
    DISCONNECTED,        // 连接断开
    DATA_RECEIVED,       // 数据接收
    DATA_SENT,           // 数据发送完成
    ERROR_OCCURRED,      // 错误发生
    TIMEOUT,             // 超时
    SUBSCRIBE_SUCCESS,   // 订阅成功
    SUBSCRIBE_FAILED,    // 订阅失败
    UNSUBSCRIBE_SUCCESS, // 取消订阅成功
    UNSUBSCRIBE_FAILED,  // 取消订阅失败
    PUBLISH_SUCCESS,     // 发布成功
    PUBLISH_FAILED       // 发布失败
};

} // namespace Network
} // namespace Common
} // namespace OutSide

#endif // NETWORKTYPE_H
