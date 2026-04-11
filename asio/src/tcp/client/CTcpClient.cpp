#include "tcp/CTcpClient.h"
#include "tcp/client/CTcpClientImpl.h"
#include "utils/MakeHelper.h"

namespace asio {

/**
 * @brief 在构造时即创建 Impl（Facade + Pimpl 模式）。
 *
 * @details
 * 将 Impl 构造提至此处，统一资源生命周期：
 * - 无需在 init() 中判断 m_pImpl 是否存在；
 * - 所有公开方法通过 `m_pImpl && m_pImpl->xxx()` 短路求值，
 *   OOM 时静默返回 false / no-op，不触发 assert 或 UB。
 *
 * @note 若 make_shared_noexcept 因内存不足返回 nullptr，
 *       m_pImpl 为空，所有调用均静默失败。
 */
CTcpClient::CTcpClient()
    : m_pImpl(make_shared_noexcept<Impl>())
{}

CTcpClient::~CTcpClient() = default;

bool CTcpClient::init() noexcept
{
    return m_pImpl && m_pImpl->init();
}

bool CTcpClient::connect(const NetAddr& serverAddr) noexcept
{
    return m_pImpl && m_pImpl->connect(serverAddr);
}

void CTcpClient::disconnect() noexcept
{
    if (m_pImpl)
        m_pImpl->disconnect();
}

bool CTcpClient::send(std::string_view data) noexcept
{
    return m_pImpl && m_pImpl->send(data);
}

bool CTcpClient::send(const std::string& data) noexcept
{
    return send(std::string_view(data));
}

bool CTcpClient::send(const char* data, size_t length) noexcept
{
    return send(std::string_view(data, length));
}

void CTcpClient::setCallback(ClientCallback&& callback) noexcept
{
    if (m_pImpl)
        m_pImpl->setCallback(std::move(callback));
}

void CTcpClient::setCallback(const ClientCallback& callback) noexcept
{
    if (m_pImpl)
        m_pImpl->setCallback(callback);
}

} // namespace asio
