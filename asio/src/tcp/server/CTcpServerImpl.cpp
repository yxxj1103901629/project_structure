#include "CTcpServerImpl.h"
#include "utils/CAtomicUtil.h"
#include "utils/MakeHelper.h"

using namespace boost::asio;
using namespace boost::system;
using namespace boost::asio::error;

namespace {

size_t getHardwareConcurrency() noexcept
{
    return std::max(2u, std::thread::hardware_concurrency() * 2u);
}

} // namespace

namespace asio {

CTcpServer::Impl::Impl()
    : m_ioContext()
    , m_acceptor(m_ioContext)
{}

CTcpServer::Impl::~Impl()
{
    stop();
}

bool CTcpServer::Impl::init(size_t threads) noexcept
{
    ServerState expected = ServerState::Uninitialized;
    if (!CAtomicUtil::compareExchange(m_state, expected, ServerState::Idle)) {
        return false;
    }

    if (threads == 0) {
        threads = getHardwareConcurrency();
    }

    m_taskThreadCount = 1;
    m_ioThreadCount = std::max<size_t>(1, threads - m_taskThreadCount);
    return true;
}

bool CTcpServer::Impl::listen(uint16_t port) noexcept
{
    ServerState expected = ServerState::Idle;
    if (!CAtomicUtil::compareExchange(m_state, expected, ServerState::Running)) {
        return false;
    }

    struct Guard
    {
        Impl* self;
        bool ok{false};
        ~Guard()
        {
            if (!ok) {
                self->stop();
            }
        }
        void release() noexcept { ok = true; }
    } guard{this};

    error_code ec;
    tcp::endpoint endpoint(tcp::v4(), port);
    if (m_acceptor.open(endpoint.protocol(), ec)
        || m_acceptor.set_option(socket_base::reuse_address(true), ec)
        || m_acceptor.bind(endpoint, ec)
        || m_acceptor.listen(socket_base::max_listen_connections, ec)) {
        notifyError("failed to configure acceptor: " + ec.message());
        return false;
    }

    if (!startPool(m_ioThreadCount, m_ioContext, m_ioWorkGuard, m_ioThreads)
        || !startPool(m_taskThreadCount, m_taskContext, m_taskWorkGuard, m_taskThreads)) {
        return false;
    }

    guard.release();
    onAccept();
    return true;
}

bool CTcpServer::Impl::startPool(size_t n,
                                 boost::asio::io_context& ctx,
                                 WorkGuardPtr& wg,
                                 Threads& pool) noexcept
{
    return startAsioThreadPool(n, ctx, wg, pool, [this](const std::string& msg) {
        notifyError(msg);
    });
}

void CTcpServer::Impl::stop() noexcept
{
    ServerState expected = ServerState::Running;
    if (!CAtomicUtil::compareExchange(m_state, expected, ServerState::Idle)) {
        return;
    }

    error_code ec;
    [[maybe_unused]] const auto cancelResult = m_acceptor.cancel(ec);
    [[maybe_unused]] const auto closeResult = m_acceptor.close(ec);

    SessionMap sessions;
    {
        std::unique_lock lock(m_sessionsMutex);
        sessions = std::move(m_sessions);
    }

    for (auto& [addr, session] : sessions) {
        session->forceClose();
        postTask([addr = std::move(addr)](Impl& self) {
            const auto callback = self.copyCallback();
            if (callback.clientDisconnected) {
                callback.clientDisconnected(addr);
            }
        });
    }

    m_ioWorkGuard.reset();
    auto joinAll = [](Threads& pool) {
        for (auto& thread : pool) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        pool.clear();
    };
    joinAll(m_ioThreads);

    m_taskWorkGuard.reset();
    joinAll(m_taskThreads);

    m_ioContext.restart();
    m_taskContext.restart();
}

bool CTcpServer::Impl::send2c(const NetAddr& clientAddr, std::string_view data) noexcept
{
    auto session = getSession(clientAddr);
    if (!session) {
        return false;
    }
    session->send(data);
    return true;
}

bool CTcpServer::Impl::broadcast(std::string_view data) noexcept
{
    std::vector<std::shared_ptr<CTcpSession>> snapshot;
    {
        std::shared_lock lock(m_sessionsMutex);
        try {
            snapshot.reserve(m_sessions.size());
        } catch (const std::exception& e) {
            notifyError(std::string("broadcast failed: ") + e.what());
            return false;
        }
        for (const auto& [addr, session] : m_sessions) {
            (void) addr;
            snapshot.push_back(session);
        }
    }

    for (const auto& session : snapshot) {
        session->send(data);
    }
    return true;
}

void CTcpServer::Impl::disconnect(const NetAddr& clientAddr) noexcept
{
    auto session = getSession(clientAddr);
    if (session) {
        session->close();
    }
}

void CTcpServer::Impl::setCallback(const ServerCallback& callback) noexcept
{
    std::lock_guard lock(m_callbackMutex);
    m_callback = callback;
}

void CTcpServer::Impl::setCallback(ServerCallback&& callback) noexcept
{
    std::lock_guard lock(m_callbackMutex);
    m_callback = std::move(callback);
}

void CTcpServer::Impl::onAccept() noexcept
{
    auto weak = make_weak_noexcept(shared_from_this());
    m_acceptor.async_accept([weak](error_code ec, tcp::socket socket) {
        auto self = weak.lock();
        if (!self || CAtomicUtil::load(self->m_state) != ServerState::Running) {
            return;
        }

        if (ec) {
            if (ec != operation_aborted) {
                self->notifyError("accept failed: " + ec.message());
            }
        } else {
            self->registerSession(std::move(socket));
        }

        if (CAtomicUtil::load(self->m_state) == ServerState::Running) {
            self->onAccept();
        }
    });
}

std::shared_ptr<CTcpSession> CTcpServer::Impl::createSession(tcp::socket socket) noexcept
{
    return make_shared_noexcept<CTcpSession>(std::move(socket), m_ioContext, this);
}

void CTcpServer::Impl::registerSession(tcp::socket socket) noexcept
{
    auto session = createSession(std::move(socket));
    if (!session) {
        notifyError("failed to create session");
        return;
    }

    if (!session->start()) {
        notifyError("failed to start session");
        return;
    }

    const NetAddr addr = session->getClientAddr();
    {
        std::unique_lock lock(m_sessionsMutex);
        m_sessions.emplace(addr, std::move(session));
    }

    postTask([addr](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.clientConnected) {
            callback.clientConnected(addr);
        }
    });
}

std::shared_ptr<CTcpSession> CTcpServer::Impl::getSession(const NetAddr& addr) const noexcept
{
    std::shared_lock lock(m_sessionsMutex);
    const auto it = m_sessions.find(addr);
    return it != m_sessions.end() ? it->second : nullptr;
}

void CTcpServer::Impl::notifyError(std::string msg) noexcept
{
    postTask([msg = std::move(msg)](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.errorOccurred) {
            callback.errorOccurred(msg);
        }
    });
}

ServerCallback CTcpServer::Impl::copyCallback() const noexcept
{
    std::lock_guard lock(m_callbackMutex);
    return m_callback;
}

void CTcpServer::Impl::onSessionDisconnected(const NetAddr& addr) noexcept
{
    {
        std::unique_lock lock(m_sessionsMutex);
        if (m_sessions.erase(addr) == 0) {
            return;
        }
    }

    postTask([addr](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.clientDisconnected) {
            callback.clientDisconnected(addr);
        }
    });
}

void CTcpServer::Impl::onSessionData(const NetAddr& addr, std::string_view data) noexcept
{
    postTask([addr, buffer = std::string(data)](Impl& self) {
        const auto callback = self.copyCallback();
        if (callback.dataReceived) {
            callback.dataReceived(addr, std::string_view(buffer));
        }
    });
}

void CTcpServer::Impl::onSessionError(const std::string& msg) noexcept
{
    notifyError(msg);
}

void CTcpServer::Impl::postTask(std::function<void(Impl&)> task) noexcept
{
    auto weak = make_weak_noexcept(shared_from_this());
    post(m_taskContext, [weak, t = std::move(task)]() mutable {
        if (auto self = weak.lock()) {
            t(*self);
        }
    });
}

} // namespace asio
