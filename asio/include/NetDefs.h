#pragma once

#include "NetAddr.h"
#include <functional>

namespace asio {

/** 服务器回调结构体 */
struct ServerCallback
{
    using AddrCb = std::function<void(const NetAddr &)>;
    using MsgCb = std::function<void(const NetAddr &, std::string_view)>;
    using ErrorCb = std::function<void(const std::string &)>;

    AddrCb clientConnected = nullptr;    ///< 客户端连接回调
    AddrCb clientDisconnected = nullptr; ///< 客户端断开连接回调
    MsgCb messageReceived = nullptr;     ///< 消息接收回调
    ErrorCb errorOccurred = nullptr;     ///< 错误发生回调
};

/** 客户端回调结构体 */
struct ClientCallback
{
    using AddrCb = std::function<void(const NetAddr &)>;
    using MsgCb = std::function<void(std::string_view)>;
    using ErrorCb = std::function<void(const std::string &)>;

    AddrCb connected = nullptr;      ///< 已连接回调（传入服务器地址）
    AddrCb disconnected = nullptr;   ///< 已断开连接回调（传入服务器地址）
    MsgCb messageReceived = nullptr; ///< 消息接收回调
    ErrorCb errorOccurred = nullptr; ///< 错误发生回调
};

} // namespace asio

namespace mosq {

struct Config
{
    std::string brokerAddress;
    uint16_t brokerPort{1883};
    std::string clientId = "";
    std::string username = "";
    std::string password = "";
};

struct ClientCallback
{
    using ConnectedCb = std::function<void()>;
    using DisconnectedCb = std::function<void()>;
    using MessageReceivedCb = std::function<void(const std::string &, std::string_view)>;
    using PublishedCb = std::function<void(int)>;       // 发布完成回调，参数为消息ID
    using SubscribedCb = std::function<void(int, int)>; // 订阅完成回调，参数为消息ID和返回的QoS等级
    using UnsubscribedCb = std::function<void(int)>;    // 取消订阅完成回调，参数为消息ID
    using ErrorCb = std::function<void(const std::string &)>;

    ConnectedCb connected = nullptr;
    DisconnectedCb disconnected = nullptr;
    MessageReceivedCb messageReceived = nullptr;
    PublishedCb published = nullptr;
    SubscribedCb subscribed = nullptr;
    UnsubscribedCb unsubscribed = nullptr;
    ErrorCb errorOccurred = nullptr;
};

} // namespace mosq