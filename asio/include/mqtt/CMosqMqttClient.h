#pragma once

#include <functional>
#include <memory>
#include <string>

#ifdef NETWORK_EXPORTS
#define CMOSQ_MQTT_CLIENT_API __declspec(dllexport)
#else
#define CMOSQ_MQTT_CLIENT_API __declspec(dllimport)
#endif

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
    using PublishedCb = std::function<void(int)>;
    using SubscribedCb = std::function<void(int, int)>;
    using UnsubscribedCb = std::function<void(int)>;
    using ErrorCb = std::function<void(const std::string &)>;

    ConnectedCb connected = nullptr;
    DisconnectedCb disconnected = nullptr;
    MessageReceivedCb messageReceived = nullptr;
    PublishedCb published = nullptr;
    SubscribedCb subscribed = nullptr;
    UnsubscribedCb unsubscribed = nullptr;
    ErrorCb errorOccurred = nullptr;
};

class CMOSQ_MQTT_CLIENT_API CMosqMqttClient final
{
public:
    CMosqMqttClient();
    ~CMosqMqttClient();
    CMosqMqttClient(const CMosqMqttClient&) = delete;
    CMosqMqttClient& operator=(const CMosqMqttClient&) = delete;

public:
    /**
     * @brief 初始化客户端
     * @param config 配置参数
     * @return 初始化是否成功
     */
    bool init(const Config& config) noexcept;
    /**
     * @brief 连接到MQTT代理
     * @return 连接是否成功
     */
    bool connect() noexcept;
    /**
     * @brief 断开与MQTT代理的连接
     */
    void disconnect() noexcept;

    /**
     * @brief 发布消息到指定主题
     * @param topic 主题
     * @param payload 消息内容
     * @param length 消息长度
     * @param qos 服务质量
     * @param retain 是否保留消息
     * @return 发布是否成功
     */
    bool publish(const std::string& topic,
                 const char* payload,
                 size_t length,
                 int qos = 0,
                 bool retain = false) noexcept;
    /**
     * @brief 发布消息到指定主题
     * @param topic 主题
     * @param payload 消息内容
     * @param qos 服务质量
     * @param retain 是否保留消息
     * @return 发布是否成功
     */
    bool publish(const std::string& topic,
                 const std::string& payload,
                 int qos = 0,
                 bool retain = false) noexcept;
    /**
     * @brief 发布消息到指定主题
     * @param topic 主题
     * @param payload 消息内容
     * @param qos 服务质量
     * @param retain 是否保留消息
     * @return 发布是否成功
     */
    bool publish(const std::string& topic,
                 std::string_view payload,
                 int qos = 0,
                 bool retain = false) noexcept;

    /**
     * @brief 订阅指定主题
     * @param topic 主题
     * @param qos 服务质量
     * @return 订阅是否成功
     */
    bool subscribe(const std::string& topic, int qos = 0) noexcept;
    /**
     * @brief 取消订阅指定主题
     * @param topic 主题
     * @return 取消订阅是否成功
     */
    bool unsubscribe(const std::string& topic) noexcept;

    /**
     * @brief 设置客户端回调
     * @param callback 回调对象
     */
    void setCallback(const ClientCallback& callback) noexcept;
    /**
     * @brief 设置客户端回调
     * @param callback 回调对象
     */
    void setCallback(ClientCallback&& callback) noexcept;

private:
    class Impl;                    ///< Pimpl实现类
    std::shared_ptr<Impl> m_pImpl; ///< Pimpl智能指针
};

} // namespace mosq