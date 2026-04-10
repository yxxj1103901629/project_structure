#include "../include/mqtt/CMosqMqttClient.h"
#include "CMosqMqttClientImpl.h"
#include "utils/MakeHelper.h"
#include <cassert>

namespace mosq {

CMosqMqttClient::CMosqMqttClient()
    : m_pImpl(nullptr)
{}
CMosqMqttClient::~CMosqMqttClient() {}

bool CMosqMqttClient::init(const Config& config) noexcept
{
    m_pImpl = make_shared_noexcept<Impl>();
    return m_pImpl && m_pImpl->init(config);
}

bool CMosqMqttClient::connect() noexcept
{
    if (!m_pImpl) {
        assert(false && "CMosqMqttClient 没有初始化");
        return false;
    }
    return m_pImpl->connect();
}

void CMosqMqttClient::disconnect() noexcept
{
    if (m_pImpl) {
        m_pImpl->disconnect();
    }
}

bool CMosqMqttClient::publish(const std::string& topic, const std::string& payload) noexcept
{
    if (!m_pImpl) {
        return false;
    }
    return m_pImpl->publish(topic, payload);
}

bool CMosqMqttClient::publish(const std::string& topic, const char* payload, size_t length) noexcept
{
    return publish(topic, std::string(payload, length));
}

bool CMosqMqttClient::publish(const std::string& topic, std::string_view payload) noexcept
{
    return publish(topic, std::string(payload));
}

bool CMosqMqttClient::subscribe(const std::string& topic, int qos) noexcept
{
    if (!m_pImpl) {
        return false;
    }
    return m_pImpl->subscribe(topic, qos);
}

bool CMosqMqttClient::unsubscribe(const std::string& topic) noexcept
{
    if (!m_pImpl) {
        return false;
    }
    return m_pImpl->unsubscribe(topic);
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
