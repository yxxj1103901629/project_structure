#include "tcp/CTcpClient.h"
#include "tcp/client/CTcpClientImpl.h"
#include "utils/MakeHelper.h"
#include <cassert>

namespace asio {

CTcpClient::CTcpClient()
    : m_pImpl(nullptr)
{}

CTcpClient::~CTcpClient() {}

bool CTcpClient::init() noexcept
{
    m_pImpl = make_shared_noexcept<Impl>();
    return m_pImpl && m_pImpl->init();
}

bool CTcpClient::connect(const NetAddr& serverAddr) noexcept
{
    if (!m_pImpl) {
        assert(false && "客户端未初始化");
        return false;
    }
    return m_pImpl->connect(serverAddr);
}

void CTcpClient::disconnect() noexcept
{
    if (!m_pImpl) {
        return;
    }
    m_pImpl->disconnect();
}

bool CTcpClient::send(std::string_view data) noexcept
{
    if (!m_pImpl) {
        return false;
    }
    return m_pImpl->send(data);
}

bool CTcpClient::send(const std::string& data) noexcept
{
    return send(std::string_view(data));
}

bool CTcpClient::send(std::string&& data) noexcept
{
    return send(std::string_view(data));
}

bool CTcpClient::send(const char* data, size_t length) noexcept
{
    return send(std::string_view(data, length));
}

void CTcpClient::setCallback(ClientCallback&& callback) noexcept
{
    if (!m_pImpl) {
        return;
    }
    m_pImpl->setCallback(std::move(callback));
}

void CTcpClient::setCallback(const ClientCallback& callback) noexcept
{
    if (!m_pImpl) {
        return;
    }
    m_pImpl->setCallback(callback);
}

} // namespace asio
