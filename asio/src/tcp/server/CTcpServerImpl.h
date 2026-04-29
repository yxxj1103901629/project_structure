#pragma once

#include "CTcpSession.h"
#include "tcp/CTcpServer.h"
#include "utils/AsioThreadPool.h"

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace asio {

using boost::asio::ip::tcp;

enum class ServerState : uint8_t {
    Uninitialized,
    Idle,
    Running
};

class CTcpServer::Impl : public std::enable_shared_from_this<CTcpServer::Impl>,
                         public ISessionObserver
{
    using Threads = ThreadGroup;
    using SessionMap = std::unordered_map<NetAddr, std::shared_ptr<CTcpSession>>;

public:
    Impl();
    ~Impl();

    bool init(size_t threads) noexcept;
    bool listen(uint16_t port) noexcept;
    void stop() noexcept;

    bool send2c(const NetAddr& clientAddr, std::string_view data) noexcept;
    bool broadcast(std::string_view data) noexcept;
    void disconnect(const NetAddr& clientAddr) noexcept;

    void setCallback(const ServerCallback& callback) noexcept;
    void setCallback(ServerCallback&& callback) noexcept;

private:
    bool startPool(size_t n, boost::asio::io_context& ctx, WorkGuardPtr& wg, Threads& pool) noexcept;
    std::shared_ptr<CTcpSession> createSession(tcp::socket socket) noexcept;
    void onAccept() noexcept;
    void registerSession(tcp::socket socket) noexcept;
    std::shared_ptr<CTcpSession> getSession(const NetAddr& addr) const noexcept;
    void notifyError(std::string msg) noexcept;
    ServerCallback copyCallback() const noexcept;

    void onSessionError(const std::string& msg) noexcept override;
    void onSessionData(const NetAddr& addr, std::string_view data) noexcept override;
    void onSessionDisconnected(const NetAddr& addr) noexcept override;

    void postTask(std::function<void(Impl&)> task) noexcept;

private:
    std::atomic<ServerState> m_state{ServerState::Uninitialized};

    boost::asio::io_context m_ioContext;
    tcp::acceptor m_acceptor;
    WorkGuardPtr m_ioWorkGuard;
    Threads m_ioThreads;
    size_t m_ioThreadCount{0};

    boost::asio::io_context m_taskContext;
    WorkGuardPtr m_taskWorkGuard;
    Threads m_taskThreads;
    size_t m_taskThreadCount{0};

    SessionMap m_sessions;
    mutable std::shared_mutex m_sessionsMutex;

    ServerCallback m_callback;
    mutable std::mutex m_callbackMutex;
};

} // namespace asio
