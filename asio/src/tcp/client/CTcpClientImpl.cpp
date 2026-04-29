#include "CTcpClientImpl.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

#include <algorithm>

using namespace boost::asio::ip;
using namespace boost::system;
using namespace boost::asio::error;
using namespace std::chrono_literals;

namespace {

constexpr size_t WRITE_BUFFER_BATCH = 16;
constexpr size_t IO_THREAD_COUNT = 1;
constexpr size_t WORKER_THREAD_COUNT = 1;

constexpr auto RECONNECT_BASE_DELAY = 1s;
constexpr auto RECONNECT_MAX_DELAY = 30s;
constexpr size_t RECONNECT_MAX_EXPONENT = 5;

inline std::chrono::steady_clock::duration calculateReconnectDelay(size_t& attempt) noexcept
{
    const auto exponent = std::min(attempt, RECONNECT_MAX_EXPONENT);
    auto delay = RECONNECT_BASE_DELAY * static_cast<int>(1ULL << exponent);
    if (delay > RECONNECT_MAX_DELAY) {
        delay = RECONNECT_MAX_DELAY;
    }
    ++attempt;
    return delay;
}

}

namespace asio {

CTcpClient::Impl::Impl()
    : m_ioContext()
    , m_socket(m_ioContext)
    , m_reconnectTimer(m_ioContext)
    , m_ioStrand(boost::asio::make_strand(m_ioContext))
    , m_taskContext()
    , m_taskStrand(boost::asio::make_strand(m_taskContext))
{}

CTcpClient::Impl::~Impl()
{
    shutdownContexts();
}

bool CTcpClient::Impl::init() noexcept
{
    bool expected = false;
    if (!m_initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;
    }

    struct Guard
    {
        Impl* self;
        bool ok{false};
        ~Guard()
        {
            if (!ok) {
                self->shutdownContexts();
            }
        }
        void release() noexcept { ok = true; }
    } guard{this};

    try {
        m_batchMessages.resize(WRITE_BUFFER_BATCH);
        m_batchViews.reserve(WRITE_BUFFER_BATCH);
    } catch (const std::exception& ex) {
        reportError("failed to initialize buffers: " + std::string(ex.what()));
        return false;
    } catch (...) {
        reportError("failed to initialize buffers: unknown error");
        return false;
    }

    if (!initializeContext(IO_THREAD_COUNT, m_ioContext, m_ioWorkGuard, m_ioThreads)) {
        return false;
    }
    if (!initializeContext(WORKER_THREAD_COUNT, m_taskContext, m_taskWorkGuard, m_taskThreads)) {
        return false;
    }

    guard.release();
    return true;
}

bool CTcpClient::Impl::connect(const NetAddr& serverAddr) noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("client is not initialized");
        return false;
    }

    ClientState expected = ClientState::Disconnected;
    if (!m_connectionState.compare_exchange_strong(expected, ClientState::Connecting, std::memory_order_acq_rel)) {
        return false;
    }

    postIo([serverAddr](Impl& self) {
        self.m_reconnectTimer.cancel();
        self.m_reconnectAttempt = 0;
        self.m_connectTarget = serverAddr;
        self.doConnect();
    });
    return true;
}

void CTcpClient::Impl::disconnect() noexcept
{
    if (!CAtomicUtil::load(m_initialized)) {
        return;
    }

    postIo([](Impl& self) {
        self.m_reconnectAttempt = 0;
        self.m_reconnectTimer.cancel();
        self.doDisconnect(false);
    });
}

bool CTcpClient::Impl::send(std::string_view data) noexcept
{
    if (data.empty()) {
        return false;
    }
    if (!CAtomicUtil::load(m_initialized)) {
        reportError("client is not initialized");
        return false;
    }
    if (CAtomicUtil::load(m_connectionState) != ClientState::Connected) {
        return false;
    }

    m_sendQueue.enqueue(std::string(data));
    postIo([](Impl& self) {
        if (!CAtomicUtil::exchange(self.m_isWriting, true)) {
            self.doWrite();
        }
    });
    return true;
}

void CTcpClient::Impl::setCallback(const ClientCallback& callback) noexcept
{
    std::lock_guard lock(m_callbackMutex);
    m_callback = callback;
}

void CTcpClient::Impl::setCallback(ClientCallback&& callback) noexcept
{
    std::lock_guard lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void CTcpClient::Impl::doConnect() noexcept
{
    if (CAtomicUtil::load(m_connectionState) != ClientState::Connecting) {
        return;
    }
    struct Guard
    {
        Impl* self;
        bool ok{false};
        ~Guard()
        {
            if (!ok) {
                CAtomicUtil::store(self->m_connectionState, ClientState::Disconnected);
            }
        }
        void release() noexcept { ok = true; }
    } guard{this};

    try {
        error_code ec;
        const auto address = make_address(m_connectTarget.ip, ec);
        if (ec) {
            reportError("failed to parse address: " + ec.message());
            return;
        }
        tcp::endpoint endpoint(address, m_connectTarget.port);

        closeSocket();
        m_socket = tcp::socket(m_ioContext);
        m_socket.async_connect(endpoint, bind_executor(m_ioStrand, [weak = make_weak_noexcept(shared_from_this())](error_code ec) {
                                   if (auto self = weak.lock()) {
                                       self->onConnect(ec);
                                   }
                               }));

        guard.release();
    } catch (const std::exception& ex) {
        reportError("connect exception: " + std::string(ex.what()));
    } catch (...) {
        reportError("connect exception: unknown error");
    }
}

void CTcpClient::Impl::onConnect(const error_code& ec) noexcept
{
    if (ec) {
        reportError("failed to connect: " + ec.message());
        doDisconnect();
        return;
    }

    CAtomicUtil::store(m_connectionState, ClientState::Connected);
    m_reconnectAttempt = 0;
    reportConnected();
    doRead();
}

void CTcpClient::Impl::doDisconnect(bool isRetry) noexcept
{
    const auto previousState = CAtomicUtil::exchange(m_connectionState, ClientState::Disconnected);
    if (previousState == ClientState::Disconnected) {
        return;
    }

    CAtomicUtil::store(m_isWriting, false);
    closeSocket();

    if (previousState == ClientState::Connected) {
        reportDisconnected();
    }

    if (!isRetry) {
        return;
    }

    m_reconnectTimer.expires_after(calculateReconnectDelay(m_reconnectAttempt));
    m_reconnectTimer.async_wait(bind_executor(m_ioStrand, [weak = make_weak_noexcept(shared_from_this())](const error_code& ec) {
                                    if (ec == operation_aborted) {
                                        return;
                                    }
                                    if (auto self = weak.lock()) {
                                        if (ec) {
                                            self->reportError("reconnect timer failed: " + ec.message());
                                        } else {
                                            self->doConnect();
                                        }
                                    }
                                }));
}

void CTcpClient::Impl::closeSocket() noexcept
{
    error_code ec;
    [[maybe_unused]] const auto shutdownResult = m_socket.shutdown(tcp::socket::shutdown_both, ec);
    [[maybe_unused]] const auto closeResult = m_socket.close(ec);
}

void CTcpClient::Impl::doRead() noexcept
{
    if (CAtomicUtil::load(m_connectionState) != ClientState::Connected) {
        return;
    }

    auto weak = make_weak_noexcept(shared_from_this());
    m_socket.async_read_some(boost::asio::buffer(m_readBuffer), bind_executor(m_ioStrand, [weak](error_code ec, size_t len) {
                                 if (auto self = weak.lock()) {
                                     self->onRead(ec, len);
                                 }
                             }));
}

void CTcpClient::Impl::onRead(const error_code& ec, size_t len) noexcept
{
    if (ec) {
        if (ec == operation_aborted) {
            return;
        }
        reportError("failed to read data: " + ec.message());
        doDisconnect();
        return;
    }

    if (len == 0) {
        reportError("server closed the connection");
        doDisconnect();
        return;
    }

    reportDataReceived(m_readBuffer.data(), len);
    doRead();
}

void CTcpClient::Impl::doWrite() noexcept
{
    if (CAtomicUtil::load(m_connectionState) != ClientState::Connected) {
        CAtomicUtil::store(m_isWriting, false);
        return;
    }

    const auto count = m_sendQueue.try_dequeue_bulk(m_batchMessages.data(), WRITE_BUFFER_BATCH);
    if (count == 0) {
        CAtomicUtil::store(m_isWriting, false);
        return;
    }

    m_batchViews.clear();
    for (size_t i = 0; i < count; ++i) {
        m_batchViews.emplace_back(boost::asio::buffer(m_batchMessages[i]));
    }

    auto weak = make_weak_noexcept(shared_from_this());
    boost::asio::async_write(m_socket, m_batchViews, bind_executor(m_ioStrand, [weak](error_code ec, size_t len) {
                                 if (auto self = weak.lock()) {
                                     self->onWrite(ec, len);
                                 }
                             }));
}

void CTcpClient::Impl::onWrite(const error_code& ec, size_t) noexcept
{
    if (ec) {
        CAtomicUtil::store(m_isWriting, false);
        reportError("failed to send data: " + ec.message());
        doDisconnect();
        return;
    }

    doWrite();
}

void CTcpClient::Impl::reportConnected() noexcept
{
    postTask([](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.connected) {
            callback.connected(self.m_connectTarget);
        }
    });
}

void CTcpClient::Impl::reportDisconnected() noexcept
{
    postTask([](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.disconnected) {
            callback.disconnected(self.m_connectTarget);
        }
    });
}

void CTcpClient::Impl::reportDataReceived(const char* data, size_t length) noexcept
{
    auto buffer = std::string(data, length);
    postTask([buffer = std::move(buffer)](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.dataReceived) {
            callback.dataReceived(std::string_view(buffer));
        }
    });
}

void CTcpClient::Impl::reportError(std::string msg) noexcept
{
    postTask([msg = std::move(msg)](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.errorOccurred) {
            callback.errorOccurred(msg);
        }
    });
}

void CTcpClient::Impl::postIo(std::function<void(Impl&)> task) noexcept
{
    boost::asio::post(m_ioStrand, [weak = make_weak_noexcept(shared_from_this()), task = std::move(task)]() mutable {
        if (auto self = weak.lock()) {
            task(*self);
        }
    });
}

void CTcpClient::Impl::postTask(std::function<void(Impl&)> task) noexcept
{
    boost::asio::post(m_taskStrand, [weak = make_weak_noexcept(shared_from_this()), task = std::move(task)]() mutable {
        if (auto self = weak.lock()) {
            task(*self);
        }
    });
}

bool CTcpClient::Impl::initializeContext(size_t threadCount,
                                         boost::asio::io_context& context,
                                         WorkGuardPtr& workGuard,
                                         Threads& threads) noexcept
{
    return startAsioThreadPool(threadCount,
                               context,
                               workGuard,
                               threads,
                               [this](const std::string& msg) { reportError(msg); });
}

ClientCallback CTcpClient::Impl::copyCallback() const noexcept
{
    std::lock_guard lock(m_callbackMutex);
    return m_callback;
}

void CTcpClient::Impl::shutdownContexts() noexcept
{
    if (!CAtomicUtil::exchange(m_initialized, false)) {
        return;
    }

    CAtomicUtil::store(m_connectionState, ClientState::Disconnected);
    CAtomicUtil::store(m_isWriting, false);
    m_reconnectTimer.cancel();
    closeSocket();

    m_ioWorkGuard.reset();
    m_taskWorkGuard.reset();

    auto joinAll = [](Threads& threads) {
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads.clear();
    };
    joinAll(m_ioThreads);
    joinAll(m_taskThreads);

    m_ioContext.restart();
    m_taskContext.restart();
}

} // namespace asio
