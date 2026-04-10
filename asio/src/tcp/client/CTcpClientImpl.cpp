#include "CTcpClientImpl.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

namespace {

constexpr size_t WRITE_BUFFER_BATCH = 16;
constexpr size_t IO_THREAD_COUNT = 1;
constexpr size_t WORKER_THREAD_COUNT = 2;

} // namespace

namespace asio {

CTcpClient::Impl::Impl()
    : m_ioContext()
    , m_taskContext()
    , m_socket(m_ioContext)
    , m_readStrand(boost::asio::make_strand(m_ioContext))
    , m_writeStrand(boost::asio::make_strand(m_ioContext))
    , m_reconnectController(m_ioContext, m_writeStrand)
{}

CTcpClient::Impl::~Impl()
{
    stop();
}

bool CTcpClient::Impl::init() noexcept
{
    // 只能从 Created 或 Stopped 状态进入 Running，防止重复 init 或在 Stopping 中调用 init 导致状态混乱
    while (true) {
        LifecycleState state = CAtomicUtil::load(m_lifecycleState);
        if (state != LifecycleState::Created && state != LifecycleState::Stopped) {
            return false;
        }
        if (CAtomicUtil::compareExchange(m_lifecycleState, state, LifecycleState::Running)) {
            break;
        }
    }

    struct Guard
    {
        CTcpClient::Impl* impl;
        bool ok = false;
        ~Guard()
        {
            if (!ok) {
                CAtomicUtil::store(impl->m_lifecycleState, LifecycleState::Created);
                impl->stop();
            }
        }
        void release() noexcept { ok = true; }
    } guard{this};

    m_ioThreadCount = IO_THREAD_COUNT;
    m_taskThreadCount = WORKER_THREAD_COUNT;

    try {
        m_batchMessages.resize(WRITE_BUFFER_BATCH);
        m_batchViews.reserve(WRITE_BUFFER_BATCH);
    } catch (const std::exception& ex) {
        reportError("初始化缓冲失败: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("初始化缓冲发生未知异常");
        return false;
    }

    if (!initThreads(m_ioThreadCount, m_ioContext, m_ioWorkGuard, m_ioThreads)) {
        reportError("初始化IO线程失败");
        return false;
    }
    if (!initThreads(m_taskThreadCount, m_taskContext, m_taskWorkGuard, m_taskThreads)) {
        reportError("初始化任务线程失败");
        return false;
    }

    CAtomicUtil::store(m_connectionState, ConnectionState::Idle);
    m_reconnectController.disable();      // 默认禁用自动重连，连接成功后由 connect() 启用
    m_reconnectController.cancel();       // 确保重连控制器处于非等待状态
    m_reconnectController.resetAttempt(); // 重置重连尝试计数，确保初始状态正确

    guard.release();
    return true;
}

bool CTcpClient::Impl::initThreads(size_t threadCount,
                                   boost::asio::io_context& context,
                                   WorkGuardPtr& workGuard,
                                   Threads& threads) noexcept
{
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

        if (CAtomicUtil::load(threadFailed)) {
            reportError("线程启动失败");
            return false;
        }

        threads = std::move(tempThreads);
    } catch (const std::exception& ex) {
        reportError("启动线程失败: " + std::string(ex.what()));
        return false;
    }

    return true;
}

void CTcpClient::Impl::scheduleReconnectAttempt() noexcept
{
    CAtomicUtil::store(m_connectionState, ConnectionState::Reconnecting);
    m_reconnectController.schedule([self = shared_from_this()] {
        // 在重连定时器回调中再次检查生命周期状态和连接状态，确保只有在适当的状态下才会尝试重连
        if (CAtomicUtil::load(self->m_lifecycleState) != LifecycleState::Running) {
            return;
        }
        // 连接状态可能已经被其他操作改变，只有在仍然是 Reconnecting 状态时才继续重连流程
        if (CAtomicUtil::load(self->m_connectionState) != ConnectionState::Reconnecting) {
            return;
        }
        self->beginConnect();
    });
}

bool CTcpClient::Impl::connect(const NetAddr& serverAddr) noexcept
{
    if (CAtomicUtil::load(m_lifecycleState) != LifecycleState::Running) {
        reportError("客户端未初始化");
        return false;
    }

    while (true) {
        ConnectionState state = CAtomicUtil::load(m_connectionState);
        if (state != ConnectionState::Idle && state != ConnectionState::Reconnecting) {
            return false;
        }
        if (CAtomicUtil::compareExchange(m_connectionState, state, ConnectionState::Connecting)) {
            break;
        }
    }

    m_connectTarget = serverAddr;

    postWrite([](Impl& self) {
        self.m_reconnectController.enable();
        self.m_reconnectController.cancel();
        self.m_reconnectController.resetAttempt();
        self.beginConnect();
    });

    return true;
}

void CTcpClient::Impl::postWrite(std::function<void(Impl&)> task) noexcept
{
    auto self = shared_from_this();
    boost::asio::post(m_writeStrand, [self, task = std::move(task)]() mutable { task(*self); });
}

void CTcpClient::Impl::postTask(std::function<void(Impl&)> task) noexcept
{
    auto self = shared_from_this();
    boost::asio::post(m_taskContext, [self, task = std::move(task)]() mutable { task(*self); });
}

void CTcpClient::Impl::beginConnect() noexcept
{
    const auto life = CAtomicUtil::load(m_lifecycleState);
    const auto conn = CAtomicUtil::load(m_connectionState);
    if (life != LifecycleState::Running
        || (conn != ConnectionState::Connecting && conn != ConnectionState::Reconnecting)) {
        return;
    }

    try {
        boost::system::error_code ec;
        auto addr = boost::asio::ip::make_address(m_connectTarget.ip(), ec);
        if (ec) {
            reportError("地址解析失败: " + ec.message());
            CAtomicUtil::store(m_connectionState, ConnectionState::Idle);
            return;
        }

        tcp::endpoint endpoint(addr, m_connectTarget.port());

        if (m_socket.is_open()) {
            [[maybe_unused]] auto closeEc = m_socket.close(ec);
        }
        m_socket = tcp::socket(m_ioContext);

        auto self = shared_from_this();
        m_socket.async_connect(endpoint,
                               boost::asio::bind_executor(m_writeStrand,
                                                          [self](boost::system::error_code ec2) {
                                                              self->onConnectResult(ec2);
                                                          }));
    } catch (const std::exception& ex) {
        reportError("连接失败: " + std::string(ex.what()));
        scheduleReconnectAttempt();
    } catch (...) {
        reportError("连接发生未知异常");
        scheduleReconnectAttempt();
    }
}

void CTcpClient::Impl::onConnectResult(boost::system::error_code ec) noexcept
{
    const auto state = CAtomicUtil::load(m_connectionState);
    if (state != ConnectionState::Connecting && state != ConnectionState::Reconnecting) {
        return;
    }

    if (ec) {
        reportError("连接服务器失败: " + ec.message());
        scheduleReconnectAttempt();
        return;
    }

    CAtomicUtil::store(m_connectionState, ConnectionState::Connected);
    m_reconnectController.resetAttempt();
    m_reconnectController.cancel();
    reportConnected();
    startReadLoop();
}

void CTcpClient::Impl::startReadLoop() noexcept
{
    if (CAtomicUtil::load(m_connectionState) != ConnectionState::Connected) {
        return;
    }

    auto self = shared_from_this();
    m_socket.async_read_some(
        boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(m_readStrand,
                                   [self](boost::system::error_code ec, std::size_t len) {
                                       if (!ec && len > 0) {
                                           self->reportMessageReceived(self->m_readBuffer.data(),
                                                                       len);
                                           self->startReadLoop();
                                           return;
                                       }

                                       using namespace boost::asio::error;
                                       if (ec && ec != eof && ec != connection_reset
                                           && ec != connection_aborted && ec != operation_aborted) {
                                           self->reportError("读取数据失败: " + ec.message());
                                       }

                                       self->processDisconnect(DisconnectMode::ReconnectIfEnabled,
                                                               SocketCloseMode::OnWriteStrand);
                                   }));
}

bool CTcpClient::Impl::send(std::string_view data) noexcept
{
    if (data.empty() || CAtomicUtil::load(m_connectionState) != ConnectionState::Connected) {
        return false;
    }

    m_sendQueue.enqueue(std::string(data));
    postWrite([](Impl& self) { self.tryStartWritePump(); });
    return true;
}

bool CTcpClient::Impl::tryStartWritePump() noexcept
{
    bool expected = false;
    if (!CAtomicUtil::compareExchange(m_writePumpRunning, expected, true)) {
        return false;
    }

    startWriteLoop();
    return true;
}

bool CTcpClient::Impl::dequeueWriteBatch(size_t& count) noexcept
{
    count = m_sendQueue.try_dequeue_bulk(m_batchMessages.data(), WRITE_BUFFER_BATCH);
    if (count > 0) {
        return true;
    }

    finishWritePump();
    if (m_sendQueue.size_approx() > 0) {
        tryStartWritePump();
    }

    return false;
}

void CTcpClient::Impl::finishWritePump() noexcept
{
    CAtomicUtil::store(m_writePumpRunning, false);
}

void CTcpClient::Impl::startWriteLoop() noexcept
{
    try {
        if (CAtomicUtil::load(m_connectionState) != ConnectionState::Connected) {
            finishWritePump();
            return;
        }

        size_t count = 0;
        if (!dequeueWriteBatch(count)) {
            return;
        }

        m_batchViews.clear();
        for (size_t i = 0; i < count; ++i) {
            m_batchViews.emplace_back(boost::asio::buffer(m_batchMessages[i]));
        }

        auto self = shared_from_this();
        boost::asio::async_write(
            m_socket,
            m_batchViews,
            boost::asio::bind_executor(
                m_writeStrand, [self](boost::system::error_code ec, std::size_t) {
                    if (ec) {
                        self->finishWritePump();
                        self->reportError("发送数据失败: " + ec.message());
                        self->processDisconnect(DisconnectMode::ReconnectIfEnabled,
                                                SocketCloseMode::OnWriteStrand);
                        return;
                    }

                    self->startWriteLoop();
                }));
    } catch (const std::exception& ex) {
        finishWritePump();
        reportError("发送数据失败: " + std::string(ex.what()));
        processDisconnect(DisconnectMode::ReconnectIfEnabled, SocketCloseMode::Immediate);
    } catch (...) {
        finishWritePump();
        reportError("发送数据发生未知错误");
        processDisconnect(DisconnectMode::ReconnectIfEnabled, SocketCloseMode::Immediate);
    }
}

void CTcpClient::Impl::closeSocketNow() noexcept
{
    boost::system::error_code ec;
    [[maybe_unused]] auto shutdownEc = m_socket.shutdown(tcp::socket::shutdown_both, ec);
    [[maybe_unused]] auto closeEc = m_socket.close(ec);
}

void CTcpClient::Impl::processDisconnect(DisconnectMode mode, SocketCloseMode closeMode) noexcept
{
    const auto current = CAtomicUtil::load(m_connectionState);
    if (current == ConnectionState::Idle || current == ConnectionState::Stopped) {
        return;
    }

    CAtomicUtil::store(m_connectionState, ConnectionState::Disconnecting);
    finishWritePump();
    reportDisconnected();

    if (closeMode == SocketCloseMode::OnWriteStrand) {
        postWrite([](Impl& self) { self.closeSocketNow(); });
    } else {
        closeSocketNow();
    }

    if (mode == DisconnectMode::ReconnectIfEnabled
        && m_reconnectController.policyState() == CReconnectController::PolicyState::Enabled
        && CAtomicUtil::load(m_lifecycleState) == LifecycleState::Running) {
        scheduleReconnectAttempt();
        return;
    }

    CAtomicUtil::store(m_connectionState, ConnectionState::Idle);
}

void CTcpClient::Impl::disconnect() noexcept
{
    postWrite([](Impl& self) {
        self.m_reconnectController.disable();
        self.m_reconnectController.cancel();
        self.processDisconnect(DisconnectMode::KeepIdle, SocketCloseMode::OnWriteStrand);
    });
}

void CTcpClient::Impl::stop() noexcept
{
    LifecycleState expected = LifecycleState::Running;
    if (!CAtomicUtil::compareExchange(m_lifecycleState, expected, LifecycleState::Stopping)) {
        return;
    }

    m_reconnectController.disable();
    m_reconnectController.cancel();

    processDisconnect(DisconnectMode::KeepIdle, SocketCloseMode::Immediate);

    m_ioWorkGuard.reset();
    m_taskWorkGuard.reset();

    m_ioContext.stop();
    m_taskContext.stop();

    auto joinThreads = [](Threads& threads) {
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads.clear();
    };

    joinThreads(m_ioThreads);
    joinThreads(m_taskThreads);

    m_ioContext.restart();
    m_taskContext.restart();

    CAtomicUtil::store(m_connectionState, ConnectionState::Stopped);
    CAtomicUtil::store(m_lifecycleState, LifecycleState::Stopped);
}

void CTcpClient::Impl::reportConnected() noexcept
{
    if (!m_callback.connected) {
        return;
    }

    postTask([](Impl& self) {
        if (self.m_callback.connected) {
            self.m_callback.connected(self.m_connectTarget);
        }
    });
}

void CTcpClient::Impl::reportDisconnected() noexcept
{
    if (!m_callback.disconnected) {
        return;
    }

    postTask([](Impl& self) {
        if (self.m_callback.disconnected) {
            self.m_callback.disconnected(self.m_connectTarget);
        }
    });
}

void CTcpClient::Impl::reportMessageReceived(const char* data, size_t length) noexcept
{
    if (!m_callback.messageReceived) {
        return;
    }

    auto msg = std::string(data, length);
    postTask([msg = std::move(msg)](Impl& self) {
        if (self.m_callback.messageReceived) {
            self.m_callback.messageReceived(std::string_view(msg));
        }
    });
}

void CTcpClient::Impl::reportError(const std::string& msg) noexcept
{
    if (!m_callback.errorOccurred) {
        return;
    }

    postTask([msg](Impl& self) {
        if (self.m_callback.errorOccurred) {
            self.m_callback.errorOccurred(msg);
        }
    });
}

void CTcpClient::Impl::setCallback(const ClientCallback& callback) noexcept
{
    m_callback = callback;
}

void CTcpClient::Impl::setCallback(ClientCallback&& callback) noexcept
{
    m_callback = std::move(callback);
}

} // namespace asio
