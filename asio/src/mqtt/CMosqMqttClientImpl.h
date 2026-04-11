#pragma once

#include "../include/mqtt/CMosqMqttClient.h"
#include <atomic>
#include <mosquitto/libmosquittopp.h>

namespace mosq {

class CMosqMqttClient::Impl : public mosqpp::mosquittopp
{
    enum State : int {
        DISCONNECTED = 0, ///< 已断开连接
        CONNECTING = 1,   ///< 正在连接中
        CONNECTED = 2     ///< 已连接
    };

public:
    Impl();
    ~Impl();

public:
    bool init(const Config &config) noexcept;
    bool connect() noexcept;
    void disconnect() noexcept;
    bool publish(const std::string &topic,
                 const char *payload,
                 size_t payloadlen,
                 int qos,
                 bool retain) noexcept;
    bool subscribe(const std::string &topic, int qos) noexcept;
    bool unsubscribe(const std::string &topic) noexcept;
    void setCallback(const ClientCallback &callback) noexcept;
    void setCallback(ClientCallback &&callback) noexcept;

private:
    void reportError(const std::string &msg) noexcept;
    void reportConnected() noexcept;
    void reportDisconnected() noexcept;
    void reportMessageReceived(const std::string &topic, std::string_view payload) noexcept;
    void reportSubscribed(int mid, int qos) noexcept;
    void reportUnsubscribed(int mid) noexcept;
    void reportPublished(int mid) noexcept;

private:
    void on_connect_v5(int rc, int flags, const mosquitto_property *props) noexcept override;
    void on_disconnect_v5(int rc, const mosquitto_property *props) noexcept override;
    void on_message_v5(const struct mosquitto_message *message,
                       const mosquitto_property *props) noexcept override;
    void on_subscribe_v5(int mid,
                         int qos_count,
                         const int *granted_qos,
                         const mosquitto_property *props) noexcept override;
    void on_unsubscribe_v5(int mid, const mosquitto_property *props) noexcept override;
    void on_publish_v5(int mid, int reason_code, const mosquitto_property *props) noexcept override;

    void on_error() noexcept override;
    void on_log(int level, const char *str) noexcept override;

private:
    std::string m_brokerAddress; ///< MQTT代理地址
    uint16_t m_brokerPort;       ///< MQTT代理端口
    std::string m_clientId;      ///< 客户端ID
    std::string m_username;      ///< 认证用户名
    std::string m_password;      ///< 认证密码

    std::atomic<bool> m_initialized{false}; ///< 是否已初始化标志

    std::atomic<State> m_state{State::DISCONNECTED}; ///< 当前连接状态

    ClientCallback m_callback; ///< 用户回调集合
};

} // namespace mosq