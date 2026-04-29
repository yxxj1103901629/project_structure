#pragma once

#include "mqtt/CMosqMqttClient.h"
#include "utils/AsioThreadPool.h"

#include <atomic>
#include <boost/asio.hpp>
#include <mosquitto/libmosquittopp.h>
#include <mutex>

namespace mosq {

class CMosqMqttClient::Impl : public mosqpp::mosquittopp
{
    enum class State : int {
        Disconnected = 0,
        Connecting = 1,
        Connected = 2
    };

public:
    Impl();
    ~Impl();

    bool init(const Config& config) noexcept;
    bool connect() noexcept;
    void disconnect() noexcept;
    bool publish(const std::string& topic,
                 const char* payload,
                 size_t payloadlen,
                 int qos,
                 bool retain) noexcept;
    bool subscribe(const std::string& topic, int qos) noexcept;
    bool unsubscribe(const std::string& topic) noexcept;
    void setCallback(const ClientCallback& callback) noexcept;
    void setCallback(ClientCallback&& callback) noexcept;

private:
    void reportError(const std::string& msg) noexcept;
    void reportConnected() noexcept;
    void reportDisconnected() noexcept;
    void reportMessageReceived(const std::string& topic, std::string payload) noexcept;
    void reportSubscribed(int mid, int qos) noexcept;
    void reportUnsubscribed(int mid) noexcept;
    void reportPublished(int mid) noexcept;
    ClientCallback copyCallback() const noexcept;
    void postTask(std::function<void(Impl&)> task) noexcept;

    void on_connect_v5(int rc, int flags, const mosquitto_property* props) noexcept override;
    void on_disconnect_v5(int rc, const mosquitto_property* props) noexcept override;
    void on_message_v5(const struct mosquitto_message* message,
                       const mosquitto_property* props) noexcept override;
    void on_subscribe_v5(int mid,
                         int qos_count,
                         const int* granted_qos,
                         const mosquitto_property* props) noexcept override;
    void on_unsubscribe_v5(int mid, const mosquitto_property* props) noexcept override;
    void on_publish_v5(int mid, int reason_code, const mosquitto_property* props) noexcept override;
    void on_error() noexcept override;
    void on_log(int level, const char* str) noexcept override;

private:
    std::string m_brokerAddress;
    uint16_t m_brokerPort{0};
    std::string m_clientId;
    std::string m_username;
    std::string m_password;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_loopStarted{false};
    std::atomic<State> m_state{State::Disconnected};

    ClientCallback m_callback;
    mutable std::mutex m_callbackMutex;
};

} // namespace mosq
