#pragma once

#include "Defs.h"
#include <memory>

#ifdef NETWORK_EXPORTS
#define CMOSQ_MQTT_CLIENT_API __declspec(dllexport)
#else
#define CMOSQ_MQTT_CLIENT_API __declspec(dllimport)
#endif

namespace mosq {

class CMOSQ_MQTT_CLIENT_API CMosqMqttClient final
{
public:
    CMosqMqttClient();
    ~CMosqMqttClient();
    CMosqMqttClient(const CMosqMqttClient&) = delete;
    CMosqMqttClient& operator=(const CMosqMqttClient&) = delete;

public:
    bool init(const Config& config) noexcept;
    bool connect() noexcept;
    void disconnect() noexcept;

    bool publish(const std::string& topic, const std::string& payload) noexcept;
    bool publish(const std::string& topic, const char* payload, size_t length) noexcept;
    bool publish(const std::string& topic, std::string_view payload) noexcept;

    bool subscribe(const std::string& topic, int qos = 0) noexcept;
    bool unsubscribe(const std::string& topic) noexcept;

    void setCallback(const ClientCallback& callback) noexcept;
    void setCallback(ClientCallback&& callback) noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> m_pImpl;
};

} // namespace mosq