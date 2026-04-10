#include "../include/tcp/CTcpServer.h"
#include "CTcpServerImpl.h"
#include "utils/MakeHelper.h"
#include <cassert>

namespace asio {

CTcpServer::CTcpServer()
    : m_pImpl(nullptr)
{}

CTcpServer::~CTcpServer() {}

bool CTcpServer::init(size_t threads) noexcept
{
    m_pImpl = make_shared_noexcept<Impl>();
    return m_pImpl && m_pImpl->init(threads);
}

bool CTcpServer::listen(uint16_t port) noexcept
{
    if (!m_pImpl) {
        assert(false && "CTcpServer not initialized");
        return false;
    }
    return m_pImpl->listen(port);
}

void CTcpServer::stop() noexcept
{
    if (m_pImpl) {
        m_pImpl->stop();
    }
}

bool CTcpServer::sendToClient(const NetAddr& clientAddr, std::string_view data) noexcept
{
    if (!m_pImpl) {
        assert(false && "CTcpServer not initialized");
        return false;
    }
    return m_pImpl->send2c(clientAddr, data);
}
bool CTcpServer::sendToClient(const NetAddr& clientAddr, const std::string& data) noexcept
{
    return sendToClient(clientAddr, std::string_view(data));
}
bool CTcpServer::sendToClient(const NetAddr& clientAddr, std::string&& data) noexcept
{
    return sendToClient(clientAddr, std::string_view(data));
}
bool CTcpServer::sendToClient(const NetAddr& clientAddr, const char* data, size_t length) noexcept
{
    return sendToClient(clientAddr, std::string_view(data, length));
}

bool CTcpServer::broadcast(std::string_view data) noexcept
{
    if (!m_pImpl) {
        assert(false && "CTcpServer not initialized");
        return false;
    }
    return m_pImpl->broadcast(data);
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
    if (!m_pImpl) {
        assert(false && "CTcpServer not initialized");
        return;
    }
    m_pImpl->disconnect(clientAddr);
}

void CTcpServer::setCallback(ServerCallback&& callback) noexcept
{
    if (!m_pImpl) {
        assert(false && "CTcpServer not initialized");
        return;
    }
    m_pImpl->setCallback(std::move(callback));
}

void CTcpServer::setCallback(const ServerCallback& callback) noexcept
{
    if (!m_pImpl) {
        assert(false && "CTcpServer not initialized");
        return;
    }
    m_pImpl->setCallback(callback);
}

} // namespace asio