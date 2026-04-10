#pragma once

#include "../include/tcp/CTcpServer.h"
#include "CTcpSession.h"

#include <boost/asio.hpp>
#include <memory>

namespace asio {

using boost::asio::ip::tcp;

class CTcpServer::Impl : public std::enable_shared_from_this<CTcpServer::Impl>
{
    /// @brief IO上下文工作保护，确保io_context在没有任务时不会退出
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkGuardPtr = std::unique_ptr<WorkGuard>;

    using Threads = std::vector<std::thread>;

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
    void onAccept() noexcept;

    void registerSession(tcp::socket socket) noexcept;
    void unregisterSession(const NetAddr& clientAddr) noexcept;
    std::shared_ptr<CTcpSession> getSession(const NetAddr& clientAddr) const noexcept;

    void reportClientConnected(const NetAddr& clientAddr) noexcept;
    void reportClientDisconnected(const NetAddr& clientAddr) noexcept;
    void reportMessageReceived(const NetAddr& clientAddr, std::string_view msg) noexcept;
    void reportError(const std::string& msg) noexcept;

private:
    boost::asio::io_context m_ioContext; ///< IO上下文对象，管理异步操作
    tcp::acceptor m_acceptor;            ///< TCP连接接受器，监听客户端连接请求
    WorkGuardPtr m_ioWorkGuard;          ///< IO上下文工作保护，防止io_context在没有任务时退出
    Threads m_ioThreads;                 ///< IO线程池，处理异步事件
    size_t m_ioThreadCount{0};           ///< IO线程数量，控制线程池大小

    boost::asio::io_context m_taskContext; ///< 任务上下文对象，处理用户回调
    WorkGuardPtr m_taskWorkGuard;          ///< 任务上下文工作保护，防止task_context在没有任务时退出
    Threads m_taskThreads;                 ///< 任务线程池，执行用户回调
    size_t m_taskThreadCount{0};           ///< 任务线程数量，控制线程池大小

    std::atomic<bool> m_isRunning{false}; ///< 服务器运行状态标志，控制服务器的启动和停止

    SessionMap m_sessions;                     ///< 活跃连接会话映射，管理客户端连接
    mutable std::shared_mutex m_sessionsMutex; ///< 保护会话映射的读写锁，确保线程安全访问

    ServerCallback m_callback;
};

} // namespace asio