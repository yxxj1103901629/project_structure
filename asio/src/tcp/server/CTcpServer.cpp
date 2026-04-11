#include "../include/tcp/CTcpServer.h"
#include "CTcpServerImpl.h"
#include "utils/MakeHelper.h"

namespace asio {

/**
 * @brief 在构造时即创建 Impl（Facade + Pimpl）。
 *
 * @details 提前持有 Impl 使所有公共方法无需空指针守卫（`m_pImpl &&` 短路求值除外）。
 * @note 若 make_shared_noexcept 因 OOM 失败，m_pImpl 为 nullptr，
 *       所有方法通过短路求值静默返回 @c false / no-op，调用方从返回值感知失败。
 */
CTcpServer::CTcpServer()
    : m_pImpl(make_shared_noexcept<Impl>())
{}

CTcpServer::~CTcpServer() = default;

bool CTcpServer::init(size_t threads) noexcept
{
    return m_pImpl && m_pImpl->init(threads);
}

bool CTcpServer::listen(uint16_t port) noexcept
{
    return m_pImpl && m_pImpl->listen(port);
}

void CTcpServer::stop() noexcept
{
    if (m_pImpl)
        m_pImpl->stop();
}

bool CTcpServer::sendToClient(const NetAddr& clientAddr, std::string_view data) noexcept
{
    return m_pImpl && m_pImpl->send2c(clientAddr, data);
}

bool CTcpServer::sendToClient(const NetAddr& clientAddr, const std::string& data) noexcept
{
    return sendToClient(clientAddr, std::string_view(data));
}

bool CTcpServer::sendToClient(const NetAddr& clientAddr, const char* data, size_t length) noexcept
{
    return sendToClient(clientAddr, std::string_view(data, length));
}

bool CTcpServer::broadcast(std::string_view data) noexcept
{
    return m_pImpl && m_pImpl->broadcast(data);
}

bool CTcpServer::broadcast(const std::string& data) noexcept
{
    return broadcast(std::string_view(data));
}

bool CTcpServer::broadcast(std::string&& data) noexcept
{
    return broadcast(std::string_view(data));
}

bool CTcpServer::broadcast(const char* data, size_t length) noexcept
{
    return broadcast(std::string_view(data, length));
}

void CTcpServer::disconnect(const NetAddr& clientAddr) noexcept
{
    if (m_pImpl)
        m_pImpl->disconnect(clientAddr);
}

void CTcpServer::setCallback(ServerCallback&& callback) noexcept
{
    if (m_pImpl)
        m_pImpl->setCallback(std::move(callback));
}

void CTcpServer::setCallback(const ServerCallback& callback) noexcept
{
    if (m_pImpl)
        m_pImpl->setCallback(callback);
}

} // namespace asio