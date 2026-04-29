#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

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
    using MessageReceivedCb = std::function<void(const std::string&, std::string_view)>;
    using PublishedCb = std::function<void(int)>;
    using SubscribedCb = std::function<void(int, int)>;
    using UnsubscribedCb = std::function<void(int)>;
    using ErrorCb = std::function<void(const std::string&)>;

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
     * @brief 初始化 MQTT 客户端
     * @param config 连接配置
     * @return 初始化是否成功
     */
    bool init(const Config& config) noexcept;

    /**
     * @brief 连接到 MQTT broker
     * @return 是否成功发起异步连接
     */
    bool connect() noexcept;

    /**
     * @brief 断开与 MQTT broker 的连接
     */
    void disconnect() noexcept;

    /**
     * @brief 向指定主题发布消息
     * @param topic 主题
     * @param payload 消息 payload
     * @param length payload 长度
     * @param qos QoS 等级
     * @param retain 是否保留消息
     * @return 发布请求是否成功提交
     */
    bool publish(const std::string& topic,
                 const char* payload,
                 size_t length,
                 int qos = 0,
                 bool retain = false) noexcept;

    /**
     * @brief 向指定主题发布字符串消息
     */
    bool publish(const std::string& topic,
                 const std::string& payload,
                 int qos = 0,
                 bool retain = false) noexcept;

    /**
     * @brief 向指定主题发布 `string_view` 消息
     */
    bool publish(const std::string& topic,
                 std::string_view payload,
                 int qos = 0,
                 bool retain = false) noexcept;

    /**
     * @brief 订阅指定主题
     * @param topic 主题
     * @param qos QoS 等级
     * @return 订阅请求是否成功提交
     */
    bool subscribe(const std::string& topic, int qos = 0) noexcept;

    /**
     * @brief 取消订阅指定主题
     * @param topic 主题
     * @return 取消订阅请求是否成功提交
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
    class Impl;                    ///< Pimpl implementation type
    std::shared_ptr<Impl> m_pImpl; ///< Pimpl smart pointer
};

} // namespace mosq
