#include "CTcpServerImpl.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

namespace {

/**
 * 获取硬件并发线程数，至少为2个线程。
 */
size_t getHardwareConcurrency() noexcept
{
    // 默认 IO 线程数为 CPU 核心数的两倍，至少为 2 个线程，以充分利用多核性能。
    const auto hc = std::thread::hardware_concurrency();
    return std::max(2u, hc * 2u);
}

} // namespace

namespace asio {

CTcpServer::Impl::Impl()
    : m_ioContext()
    , m_acceptor(m_ioContext)
    , m_ioWorkGuard(nullptr)
    , m_taskWorkGuard(nullptr)
    , m_callback()
{}

CTcpServer::Impl::~Impl()
{
    stop(); // 确保服务器停止，释放资源
}

bool CTcpServer::Impl::init(size_t threads) noexcept
{
    if (threads == 0) {
        threads = getHardwareConcurrency(); // 获取默认线程数
    }

    // IO线程为任务线程的一半，总数为threads，保持1:2比例，确保IO线程有足够资源处理网络事件，同时任务线程有足够资源处理回调
    // 计算IO线程和任务线程数量，确保至少有1个线程用于IO和任务处理
    m_ioThreadCount = std::max<size_t>(1, threads / 3);
    m_taskThreadCount = std::max<size_t>(1, threads - m_ioThreadCount);

    return true;
}

bool CTcpServer::Impl::listen(uint16_t port) noexcept
{
    if (CAtomicUtil::exchange(m_isRunning, true)) {
        return false; // 已经在监听，避免重复启动
    }

    struct Guard
    {
        CTcpServer::Impl* impl;
        bool ok = false; // 标志资源是否成功初始化
        ~Guard()
        {
            if (!ok) {
                CAtomicUtil::store(impl->m_isRunning, false);
                impl->stop();
            }
        }
        // 资源成功初始化后调用，防止Guard析构时误调用stop()
        void release() noexcept { ok = true; }
    } guard{this}; // 资源管理，确保在发生错误时正确释放资源

    // 设置套接字选项，绑定端口并开始监听
    {
        tcp::endpoint endpoint(tcp::v4(), port);
        boost::system::error_code ec;

        if (m_acceptor.open(endpoint.protocol(), ec)) {
            reportError("打开套接字失败: " + ec.message());
            return false;
        }
        if (m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec)) {
            reportError("设置套接字选项失败: " + ec.message());
            return false;
        }
        if (m_acceptor.bind(endpoint, ec)) {
            reportError("绑定端口失败: " + ec.message());
            return false;
        }
        if (m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec)) {
            reportError("监听失败: " + ec.message());
            return false;
        }
    }

    auto initThreads = [this](size_t threadCount,
                              boost::asio::io_context& context,
                              WorkGuardPtr& workGuard,
                              Threads& threads) -> bool {
        workGuard = make_unique_noexcept<WorkGuard>(boost::asio::make_work_guard(context));
        if (!workGuard) {
            reportError("创建工作保护失败");
            return false;
        }

        try {
            Threads tempThreads;
            tempThreads.reserve(threadCount);
            std::atomic_bool threadFailed{false};

            for (size_t i = 0; i < threadCount; ++i) {
                tempThreads.emplace_back([this, &threadFailed, &context] {
                    try {
                        context.run();
                    } catch (const std::exception& ex) {
                        reportError("线程异常: " + std::string(ex.what()));
                        CAtomicUtil::store(threadFailed, true);
                    } catch (...) {
                        reportError("线程发生未知异常");
                        CAtomicUtil::store(threadFailed, true);
                    }
                });
            }

            // 检查是否有线程启动失败
            if (CAtomicUtil::load(threadFailed)) {
                reportError("线程启动失败");
                return false; // Guard会自动调用stop()停止所有线程
            }

            threads = std::move(tempThreads); // 只有所有线程成功启动后才更新成员变量

        } catch (const std::exception& ex) {
            reportError("启动IO线程失败: " + std::string(ex.what()));
            return false; // Guard会自动调用stop()停止所有线程
        }

        return true;
    };

    if (!initThreads(m_ioThreadCount, m_ioContext, m_ioWorkGuard, m_ioThreads)) {
        reportError("初始化IO线程失败");
        return false; // 初始化IO线程失败，Guard会自动调用stop()释放资源
    }
    if (!initThreads(m_taskThreadCount, m_taskContext, m_taskWorkGuard, m_taskThreads)) {
        reportError("初始化任务线程失败");
        return false; // 初始化任务线程失败，Guard会自动调用stop()释放资源
    }

    guard.release(); // 资源初始化成功，释放Guard

    // 开始接受客户端连接
    onAccept();

    return true;
}

void CTcpServer::Impl::stop() noexcept
{
    if (!CAtomicUtil::exchange(m_isRunning, false)) {
        return; // 已经停止，避免重复调用
    }

    // 取消所有异步操作，关闭接受器，防止新的连接进入
    boost::system::error_code ec;
    [[maybe_unused]] auto cancel = m_acceptor.cancel(ec);
    [[maybe_unused]] auto close = m_acceptor.close(ec);

    // 复制当前会话列表，避免在关闭过程中持有锁，防止死锁
    std::vector<std::shared_ptr<CTcpSession>> sessionsToClose;
    {
        std::unique_lock lock(m_sessionsMutex);
        sessionsToClose.reserve(m_sessions.size());
        for (auto& [addr, session] : m_sessions) {
            sessionsToClose.push_back(session);
        }
        m_sessions.clear();
    }
    // 关闭所有会话
    for (auto& session : sessionsToClose) {
        session->close();
        reportClientDisconnected(session->getClientAddr());
    }

    // 清空工作守卫，允许io_context退出
    m_ioWorkGuard.reset();
    // 清空任务工作守卫，允许task_context退出
    m_taskWorkGuard.reset();

    // 停止io_context和task_context
    m_ioContext.stop();
    m_taskContext.stop();

    // 等待所有IO线程和任务线程完成
    auto joinThreads = [](Threads& threads) {
        for (auto& thread : threads) {
            if (thread.joinable())
                thread.join();
        }
        threads.clear();
    };
    joinThreads(m_ioThreads);
    joinThreads(m_taskThreads);

    m_ioContext.restart();   // 重置io_context以便下次使用
    m_taskContext.restart(); // 重置task_context以便下次使用
}

bool CTcpServer::Impl::send2c(const NetAddr& clientAddr, std::string_view data) noexcept
{
    auto session = getSession(clientAddr);
    if (!session) {
        return false;
    }
    session->send(std::string(data)); // 发送数据到指定客户端
    return true;
}
bool CTcpServer::Impl::broadcast(std::string_view data) noexcept
{
    std::vector<std::shared_ptr<CTcpSession>> sessions;
    {
        std::shared_lock lock(m_sessionsMutex);
        sessions.reserve(m_sessions.size());
        for (auto& [addr, session] : m_sessions) {
            sessions.push_back(session);
        }
    }
    for (auto& session : sessions) {
        session->send(std::string(data));
    }
    return true;
}

void CTcpServer::Impl::disconnect(const NetAddr& clientAddr) noexcept
{
    auto session = getSession(clientAddr);
    if (session) {
        session->close(); // 关闭指定客户端的连接
        reportClientDisconnected(clientAddr);
    }
}

void CTcpServer::Impl::setCallback(const ServerCallback& callback) noexcept
{
    m_callback = callback;
}

void CTcpServer::Impl::setCallback(ServerCallback&& callback) noexcept
{
    m_callback = std::move(callback);
}

void CTcpServer::Impl::onAccept() noexcept
{
    m_acceptor.async_accept(
        [self = shared_from_this()](boost::system::error_code ec, tcp::socket socket) {
            if (!CAtomicUtil::load(self->m_isRunning)) {
                return; // 服务器已停止，忽略新的连接
            }

            if (ec) {
                self->reportError("接受连接失败: " + ec.message());
            } else {
                // 处理新连接
                self->registerSession(std::move(socket));
            }

            // 继续接受下一个连接
            self->onAccept();
        });
}

void CTcpServer::Impl::registerSession(tcp::socket socket) noexcept
{
    auto self = shared_from_this();
    auto session = make_shared_noexcept<CTcpSession>(
        std::move(socket),
        m_ioContext,
        [self](const std::string& msg) { self->reportError(msg); },
        [self](const NetAddr& addr, std::string_view msg) {
            self->reportMessageReceived(addr, msg);
        },
        [self](const NetAddr& addr) { self->reportClientDisconnected(addr); });

    if (!session) {
        reportError("创建会话失败");
        return;
    }

    if (!session->start()) {
        reportError("启动会话失败");
        return;
    }

    {
        std::unique_lock lock(m_sessionsMutex);
        m_sessions.emplace(session->getClientAddr(), session);
    }

    reportClientConnected(session->getClientAddr());
}

void CTcpServer::Impl::unregisterSession(const NetAddr& clientAddr) noexcept
{
    std::unique_lock lock(m_sessionsMutex);
    m_sessions.erase(clientAddr); // 从会话映射中移除断开的客户端
}

std::shared_ptr<CTcpSession> CTcpServer::Impl::getSession(const NetAddr& clientAddr) const noexcept
{
    std::shared_lock lock(m_sessionsMutex);
    auto it = m_sessions.find(clientAddr);
    if (it != m_sessions.end()) {
        return it->second;
    }
    return nullptr;
}

void CTcpServer::Impl::reportClientConnected(const NetAddr& clientAddr) noexcept
{
    if (!m_callback.clientConnected)
        return;
    auto self = shared_from_this();
    boost::asio::post(m_taskContext, [self, clientAddr]() {
        if (self->m_callback.clientConnected) {
            self->m_callback.clientConnected(clientAddr);
        }
    });
}

void CTcpServer::Impl::reportClientDisconnected(const NetAddr& clientAddr) noexcept
{
    unregisterSession(clientAddr); // 从会话映射中移除断开的客户端，确保资源释放和状态更新
    if (!m_callback.clientDisconnected) {
        return;
    }
    auto self = shared_from_this();
    boost::asio::post(m_taskContext, [self, clientAddr]() {
        if (self->m_callback.clientDisconnected) {
            self->m_callback.clientDisconnected(clientAddr);
        }
    });
}

void CTcpServer::Impl::reportMessageReceived(const NetAddr& clientAddr,
                                             std::string_view msg) noexcept
{
    if (!m_callback.messageReceived) {
        return;
    }
    auto self = shared_from_this();
    boost::asio::post(m_taskContext, [self, clientAddr, msg = std::string(msg)]() {
        if (self->m_callback.messageReceived) {
            self->m_callback.messageReceived(clientAddr, std::string_view(msg));
        }
    });
}

void CTcpServer::Impl::reportError(const std::string& msg) noexcept
{
    if (!m_callback.errorOccurred) {
        return;
    }
    auto self = shared_from_this();
    boost::asio::post(m_taskContext, [self, msg = std::move(msg)]() {
        if (self->m_callback.errorOccurred) {
            self->m_callback.errorOccurred(msg);
        }
    });
}

} // namespace asio