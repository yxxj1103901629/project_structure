#pragma once

#include "Defs.h"
#include <memory>

#ifdef NETWORK_EXPORTS
#define CMOSQ_MQTT_CLIENT_API __declspec(dllexport)
#else
#define CMOSQ_MQTT_CLIENT_API __declspec(dllimport)
#endif

/**
 * @file CMosqMqttClient.h
 * @brief 基于 mosquitto 库的 MQTT 客户端封装类
 * @details 该类封装了 mosquitto 库的 MQTT 客户端功能，提供了一个简单易用的接口用于连接到 MQTT 代理、发布消息、订阅主题等操作。
 *          内部使用 Pimpl 设计模式隐藏实现细节，确保接口的稳定性和易用性。
 */

namespace mosq {

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