#include "mqtt/CMosqMqttClient.h"
#include "CMosqMqttClientImpl.h"
#include "utils/MakeHelper.h"

namespace mosq {

CMosqMqttClient::CMosqMqttClient()
    : m_pImpl(nullptr)
{}

CMosqMqttClient::~CMosqMqttClient() = default;

bool CMosqMqttClient::init(const Config& config) noexcept
{
    if (m_pImpl) {
        return false;
    }

    m_pImpl = make_shared_noexcept<Impl>();
    if (!m_pImpl) {
        return false;
    }

    if (!m_pImpl->init(config)) {
        m_pImpl.reset();
        return false;
    }

    return true;
}

bool CMosqMqttClient::connect() noexcept
{
    return m_pImpl && m_pImpl->connect();
}

void CMosqMqttClient::disconnect() noexcept
{
    if (m_pImpl) {
        m_pImpl->disconnect();
    }
}

bool CMosqMqttClient::publish(const std::string& topic,
                              const char* payload,
                              size_t length,
                              int qos,
                              bool retain) noexcept
{
    return m_pImpl && m_pImpl->publish(topic, payload, length, qos, retain);
}

bool CMosqMqttClient::publish(const std::string& topic,
                              const std::string& payload,
                              int qos,
                              bool retain) noexcept
{
    return publish(topic, payload.c_str(), payload.size(), qos, retain);
}

bool CMosqMqttClient::publish(const std::string& topic,
                              std::string_view payload,
                              int qos,
                              bool retain) noexcept
{
    return publish(topic, payload.data(), payload.size(), qos, retain);
}

bool CMosqMqttClient::subscribe(const std::string& topic, int qos) noexcept
{
    return m_pImpl && m_pImpl->subscribe(topic, qos);
}

bool CMosqMqttClient::unsubscribe(const std::string& topic) noexcept
{
    return m_pImpl && m_pImpl->unsubscribe(topic);
}

void CMosqMqttClient::setCallback(const ClientCallback& callback) noexcept
{
    if (m_pImpl) {
        m_pImpl->setCallback(callback);
    }
}

void CMosqMqttClient::setCallback(ClientCallback&& callback) noexcept
{
    if (m_pImpl) {
        m_pImpl->setCallback(std::move(callback));
    }
}

} // namespace mosq
